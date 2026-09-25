#import <UIKit/UIKit.h>
#import <QuartzCore/CAEAGLLayer.h>
#import <OpenGLES/ES3/gl.h>
#import <mach/mach.h>
#include "ios_host.h"
#include "ios_language.h"
#include "ios_presentation_layout.h"
#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cfloat>
#include <exception>
#include <mutex>
#include <string>
#include <signal.h>
#include <unistd.h>

extern "C" bool th20_ios_audio_suspend(bool paused);
@class TH20ViewController;
@class TH20Joystick;

namespace {
TH20IOSCallbacks callbacks{};
__weak TH20ViewController *host;
std::mutex logMutex;
FILE *logFile{};
int crashLogFD = -1;
std::string logPath;
double logEpoch{};
int logicalWidth = 640, logicalHeight = 480;
bool deferPresentation = false, presentationPending = false;
GLuint pendingFramebuffer = 0;
int pendingWidth = 0, pendingHeight = 0;

void onMain(dispatch_block_t block) {
    if ([NSThread isMainThread]) block();
    else dispatch_async(dispatch_get_main_queue(), block);
}
void fatalSignal(int number) {
    // Only async-signal-safe syscalls here. Preserve native OS crash reporting.
    static const char text[] = "FATAL SIGNAL: consult the iOS crash report for the backtrace.\n";
    if (crashLogFD >= 0) write(crashLogFD, text, sizeof(text) - 1);
    write(STDERR_FILENO, text, sizeof(text) - 1);
    signal(number, SIG_DFL);
    raise(number);
}
void uncaughtException(NSException *exception) {
    th20_ios_log("FATAL Objective-C exception %s: %s\n%s", exception.name.UTF8String,
                 exception.reason.UTF8String, exception.callStackSymbols.description.UTF8String);
    th20_ios_flush_log();
}
void openLog() {
    NSString *documents = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES).firstObject;
    NSString *directory = [documents stringByAppendingPathComponent:@"Logs"];
    NSError *error = nil;
    [[NSFileManager defaultManager] createDirectoryAtPath:directory withIntermediateDirectories:YES attributes:nil error:&error];
    NSDateFormatter *formatter = [NSDateFormatter new];
    formatter.locale = [NSLocale localeWithLocaleIdentifier:@"en_US_POSIX"];
    formatter.dateFormat = @"yyyyMMdd-HHmmss-SSS";
    NSString *filename = [NSString stringWithFormat:@"th20-%@.log", [formatter stringFromDate:[NSDate date]]];
    logPath = [[directory stringByAppendingPathComponent:filename] fileSystemRepresentation];
    logFile = std::fopen(logPath.c_str(), "ab");
    logEpoch = CACurrentMediaTime();
    if (logFile) { setvbuf(logFile, nullptr, _IOLBF, 0); crashLogFD = fileno(logFile); }
    NSSetUncaughtExceptionHandler(uncaughtException);
    std::set_terminate([] {
        try { if (auto error = std::current_exception()) std::rethrow_exception(error); }
        catch (const std::exception& error) { th20_ios_log("FATAL uncaught C++ exception: %s", error.what()); }
        catch (...) { th20_ios_log("FATAL unknown C++ exception"); }
        th20_ios_flush_log();
        std::abort();
    });
    for (int number : {SIGABRT, SIGBUS, SIGFPE, SIGILL, SIGSEGV}) signal(number, fatalSignal);
    NSDictionary *info = NSBundle.mainBundle.infoDictionary;
    th20_ios_log("SESSION START version=%s build=%s iOS=%s model=%s native=yes sizeof_pointer=%zu",
                 [info[@"CFBundleShortVersionString"] UTF8String], [info[@"CFBundleVersion"] UTF8String],
                 UIDevice.currentDevice.systemVersion.UTF8String, UIDevice.currentDevice.model.UTF8String, sizeof(void *));
    th20_ios_log("paths resources=%s documents=%s log=%s", NSBundle.mainBundle.resourcePath.fileSystemRepresentation,
                 documents.fileSystemRepresentation, logPath.c_str());
    if (error || !logFile) NSLog(@"TH20 cannot create persistent diagnostic log: %@", error);
}
uint64_t residentBytes() {
    mach_task_basic_info_data_t info{};
    mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
    return task_info(mach_task_self(), MACH_TASK_BASIC_INFO, reinterpret_cast<task_info_t>(&info), &count) == KERN_SUCCESS
        ? info.resident_size : 0;
}
}

@interface TH20GLView : UIView
@property(nonatomic, strong) EAGLContext *context;
@property(nonatomic) GLuint framebuffer;
@property(nonatomic) GLuint colorbuffer;
@property(nonatomic) GLint pixelWidth;
@property(nonatomic) GLint pixelHeight;
@property(nonatomic) BOOL drawableDirty;
@property(nonatomic, weak) TH20ViewController *owner;
- (BOOL)createContext;
- (BOOL)prepareDrawable;
- (BOOL)presentFramebuffer:(GLuint)source width:(int)width height:(int)height;
@end

@interface TH20ViewController : UIViewController
@property(nonatomic, strong) TH20GLView *gameView;
@property(nonatomic, strong) CADisplayLink *displayLink;
@property(nonatomic, strong) UILabel *statusLabel;
@property(nonatomic, strong) UIView *statusPanel;
@property(nonatomic, strong) UIButton *exportButton;
@property(nonatomic, strong) NSMutableArray<UIButton *> *controls;
@property(nonatomic, strong) TH20Joystick *joystick;
@property(nonatomic, strong) UILabel *performanceLabel;
@property(nonatomic, strong) NSMutableDictionary<NSNumber *, NSNumber *> *keyDownTicks;
@property(nonatomic, strong) NSMutableSet<NSNumber *> *pendingKeyUps;
@property(nonatomic) uint64_t updateTick;
@property(nonatomic, strong) UITouch *moveTouch;
@property(nonatomic) CGPoint previousPoint;
@property(nonatomic) CGPoint startPoint;
@property(nonatomic) NSTimeInterval touchStart;
@property(nonatomic) CGPoint swipeOrigin;
@property(nonatomic) BOOL touchScrolled;
@property(nonatomic) BOOL suppressTouches;
@property(nonatomic, strong) NSMutableSet<UITouch *> *gestureTouches;
@property(nonatomic, strong) NSMapTable<UITouch *, NSValue *> *gestureOrigins;
@property(nonatomic, strong) NSTimer *gestureTimer;
@property(nonatomic) NSTimeInterval gestureStarted;
@property(nonatomic) BOOL gestureInvalid;
@property(nonatomic) BOOL gestureHadThird;
@property(nonatomic) uint64_t touchID;
@property(nonatomic) uint64_t nextTouchID;
@property(nonatomic) TH20IOSInputMode inputMode;
@property(nonatomic) BOOL combatScene;
@property(nonatomic) BOOL ready;
@property(nonatomic) BOOL initialized;
@property(nonatomic) BOOL engineAvailable;
@property(nonatomic) BOOL fatalError;
@property(nonatomic) BOOL appActive;
@property(nonatomic) BOOL modalPaused;
@property(nonatomic) BOOL enginePaused;
@property(nonatomic) BOOL shotHeld;
@property(nonatomic) BOOL slowHeld;
@property(nonatomic) BOOL bombHeld;
@property(nonatomic) BOOL pauseHeld;
@property(nonatomic) BOOL shotLatched;
@property(nonatomic) BOOL slowLatched;
@property(nonatomic) BOOL shotToggle;
@property(nonatomic) BOOL slowToggle;
@property(nonatomic) float sensitivity;
@property(nonatomic) float buttonOpacity;
@property(nonatomic) float buttonSize;
@property(nonatomic) BOOL controlsVisible;
@property(nonatomic) BOOL leftHanded;
@property(nonatomic) BOOL haptics;
@property(nonatomic) NSInteger controlMode;
@property(nonatomic) BOOL autoShot;
@property(nonatomic) BOOL autoSlow;
@property(nonatomic) BOOL autoBomb;
@property(nonatomic) BOOL developerMode;
@property(nonatomic) BOOL devInvincible;
@property(nonatomic, strong) UIButton *devLauncher;
@property(nonatomic, strong) UIView *devOverlay;
@property(nonatomic, strong) UIView *devPanel;
@property(nonatomic, strong) UIScrollView *devList;
@property(nonatomic, strong) UILabel *devHeader;
@property(nonatomic, strong) UILabel *devStatus;
@property(nonatomic, strong) NSMutableArray<UIButton *> *devButtons;
@property(nonatomic) float joystickDeadzone;
@property(nonatomic) float controlHeight;
@property(nonatomic) NSInteger displayFPS;
@property(nonatomic) BOOL smoothScaling;
@property(nonatomic) BOOL showPerformance;
@property(nonatomic) BOOL editingLayout;
@property(nonatomic, strong) UIView *layoutToolbar;
@property(nonatomic, strong) NSMutableDictionary *customLayouts;
@property(nonatomic, strong) NSDictionary *layoutBackup;
@property(nonatomic, strong) UIImpactFeedbackGenerator *feedback;
@property(nonatomic) float renderScale;
@property(nonatomic) CFTimeInterval lastTimestamp;
@property(nonatomic) double accumulator;
@property(nonatomic) double telemetryStart;
@property(nonatomic) double maxFrameMS;
@property(nonatomic) uint64_t telemetryFrames;
@property(nonatomic) uint64_t telemetryUpdates;
@property(nonatomic) uint64_t presentedFrames;
@property(nonatomic) uint64_t stallCount;
@property(nonatomic, strong) NSString *stage;
- (void)clearInput;
- (void)setMode:(TH20IOSInputMode)mode;
- (void)setCombatPresentation:(BOOL)active;
- (void)showError:(NSString *)message;
- (void)showStage:(NSString *)stage;
- (void)changeReady:(BOOL)ready;
- (void)setApplicationActive:(BOOL)active;
- (void)applyPausedState;
- (void)updateControlColors;
- (void)sendKey:(int)key down:(BOOL)down;
- (void)applyAutomaticInput;
- (void)applyDisplaySettings;
- (void)beginLayoutEditing;
- (NSString *)layoutOrientation;
- (void)openSettings;
- (void)syncCombatOptions;
- (void)closeDeveloper;
- (BOOL)usesPortraitBattleLayout;
- (void)exportLogs:(id)sender;
- (void)stop;
- (void)touchesBegan:(NSSet<UITouch *> *)touches event:(UIEvent *)event;
- (void)touchesMoved:(NSSet<UITouch *> *)touches event:(UIEvent *)event;
- (void)touchesEnded:(NSSet<UITouch *> *)touches cancelled:(BOOL)cancelled;
- (void)threeFingerPause:(NSTimer *)timer;
@end

@interface TH20Joystick : UIControl
@property(nonatomic, weak) TH20ViewController *owner;
@property(nonatomic, strong) UIView *knob;
@property(nonatomic) NSUInteger directions;
- (void)reset;
@end

@interface TH20SettingsController : UITableViewController <UITextFieldDelegate>
@property(nonatomic, weak) TH20ViewController *owner;
@property(nonatomic, strong) UITextField *cheatField;
@property(nonatomic, strong) UILabel *cheatResult;
@end

// Mineral facets and lacquer/gold borders echo TH20's stone theme. These
// views use custom drawing and rectangular controls, not a system sheet.
@interface TH20DevPanel : UIView
@end
@implementation TH20DevPanel
- (void)drawRect:(CGRect)rect {
    CGContextRef c = UIGraphicsGetCurrentContext();
    CGFloat w = self.bounds.size.width, h = self.bounds.size.height;
    [[UIColor colorWithRed:0.09 green:0.045 blue:0.12 alpha:0.98] setFill];
    UIRectFill(self.bounds);
    for (int i = 0; i < 6; ++i) {
        CGFloat x = w * i / 5.0;
        CGContextBeginPath(c); CGContextMoveToPoint(c, x - 70, h);
        CGContextAddLineToPoint(c, x + 30, h * 0.3); CGContextAddLineToPoint(c, x + 90, h);
        CGContextClosePath(c);
        CGContextSetRGBFillColor(c, 0.6, 0.22, 0.4, i % 2 ? 0.13 : 0.07); CGContextFillPath(c);
    }
    CGContextSetRGBStrokeColor(c, 0.83, 0.64, 0.38, 1); CGContextSetLineWidth(c, 2);
    CGContextStrokeRect(c, CGRectInset(self.bounds, 2, 2));
    CGContextSetRGBStrokeColor(c, 0.7, 0.36, 0.5, 0.8); CGContextSetLineWidth(c, 1);
    CGContextStrokeRect(c, CGRectInset(self.bounds, 7, 7));
}
@end

@implementation TH20Joystick
- (instancetype)initWithFrame:(CGRect)frame {
    if ((self = [super initWithFrame:frame])) {
        self.backgroundColor = [UIColor colorWithWhite:0.1 alpha:0.65];
        self.layer.borderColor = [UIColor colorWithWhite:1 alpha:0.7].CGColor;
        self.layer.borderWidth = 2;
        self.multipleTouchEnabled = NO;
        self.knob = [UIView new]; self.knob.userInteractionEnabled = NO;
        self.knob.backgroundColor = [UIColor colorWithWhite:0.85 alpha:0.65];
        self.knob.layer.borderColor = UIColor.whiteColor.CGColor; self.knob.layer.borderWidth = 1;
        [self addSubview:self.knob];
        self.accessibilityLabel = @"方向摇杆";
    }
    return self;
}
- (void)layoutSubviews {
    [super layoutSubviews];
    self.layer.cornerRadius = self.bounds.size.width / 2;
    CGFloat size = self.bounds.size.width * 0.43;
    self.knob.bounds = CGRectMake(0, 0, size, size); self.knob.layer.cornerRadius = size / 2;
    if (!self.tracking) self.knob.center = CGPointMake(CGRectGetMidX(self.bounds), CGRectGetMidY(self.bounds));
}
- (BOOL)pointInside:(CGPoint)point withEvent:(UIEvent *)event {
    return std::hypot(point.x - CGRectGetMidX(self.bounds), point.y - CGRectGetMidY(self.bounds)) <= self.bounds.size.width * 0.58;
}
- (void)updatePoint:(CGPoint)point {
    CGFloat radius = self.bounds.size.width / 2;
    CGFloat x = (point.x - radius) / radius, y = (point.y - radius) / radius;
    CGFloat length = std::hypot(x, y), divisor = std::max(CGFloat(1), length);
    self.knob.center = CGPointMake(radius + x / divisor * radius * 0.58, radius + y / divisor * radius * 0.58);
    NSUInteger directions = 0;
    if (length >= self.owner.joystickDeadzone) {
        // Eight directions use the game's own velocity and movement animation.
        const CGFloat axis = length * 0.3826834324;
        if (x < -axis) directions |= 1;
        if (x > axis) directions |= 2;
        if (y < -axis) directions |= 4;
        if (y > axis) directions |= 8;
    }
    const int keys[] = {0x25, 0x27, 0x26, 0x28};
    for (unsigned bit = 0; bit != 4; ++bit)
        if ((directions ^ self.directions) & (1u << bit))
            [self.owner sendKey:keys[bit] down:(directions & (1u << bit)) != 0];
    self.directions = directions;
}
- (BOOL)beginTrackingWithTouch:(UITouch *)touch withEvent:(UIEvent *)event {
    if (!self.owner.ready || self.owner.modalPaused || self.owner.editingLayout || self.owner.inputMode == TH20_IOS_INPUT_LOADING) return NO;
    [self updatePoint:[touch locationInView:self]]; return YES;
}
- (BOOL)continueTrackingWithTouch:(UITouch *)touch withEvent:(UIEvent *)event {
    [self updatePoint:[touch locationInView:self]]; return YES;
}
- (void)endTrackingWithTouch:(UITouch *)touch withEvent:(UIEvent *)event { [self reset]; }
- (void)cancelTrackingWithEvent:(UIEvent *)event { [self reset]; }
- (void)reset {
    const int keys[] = {0x25, 0x27, 0x26, 0x28};
    for (unsigned bit = 0; bit != 4; ++bit)
        if (self.directions & (1u << bit)) [self.owner sendKey:keys[bit] down:NO];
    self.directions = 0;
    self.knob.center = CGPointMake(CGRectGetMidX(self.bounds), CGRectGetMidY(self.bounds));
}
@end

@implementation TH20GLView
+ (Class)layerClass { return CAEAGLLayer.class; }
- (BOOL)createContext {
    self.contentScaleFactor = UIScreen.mainScreen.scale;
    self.opaque = YES;
    self.multipleTouchEnabled = YES;
    CAEAGLLayer *layer = (CAEAGLLayer *)self.layer;
    layer.opaque = YES;
    layer.drawableProperties = @{kEAGLDrawablePropertyRetainedBacking: @NO,
                                 kEAGLDrawablePropertyColorFormat: kEAGLColorFormatRGBA8};
    self.context = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES3];
    if (!self.context || ![EAGLContext setCurrentContext:self.context]) return NO;
    self.drawableDirty = YES;
    th20_ios_log("graphics context GLES3 vendor=%s renderer=%s version=%s Retina=%.2f",
                 glGetString(GL_VENDOR), glGetString(GL_RENDERER), glGetString(GL_VERSION), self.contentScaleFactor);
    return YES;
}
- (void)layoutSubviews { [super layoutSubviews]; self.drawableDirty = YES; }
- (BOOL)prepareDrawable {
    if (![EAGLContext setCurrentContext:self.context]) return NO;
    if (!self.drawableDirty && self.framebuffer && self.pixelWidth > 0 && self.pixelHeight > 0) return YES;
    if (self.bounds.size.width <= 0 || self.bounds.size.height <= 0) return NO;
    GLint readFBO = 0, drawFBO = 0, oldRBO = 0;
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &readFBO);
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &drawFBO);
    glGetIntegerv(GL_RENDERBUFFER_BINDING, &oldRBO);
    if (!self.framebuffer) { GLuint fbo; glGenFramebuffers(1, &fbo); self.framebuffer = fbo; }
    if (!self.colorbuffer) { GLuint rbo; glGenRenderbuffers(1, &rbo); self.colorbuffer = rbo; }
    glBindRenderbuffer(GL_RENDERBUFFER, self.colorbuffer);
    BOOL allocated = [self.context renderbufferStorage:GL_RENDERBUFFER fromDrawable:(CAEAGLLayer *)self.layer];
    GLint width = 0, height = 0;
    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH, &width);
    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_HEIGHT, &height);
    self.pixelWidth = width; self.pixelHeight = height;
    glBindFramebuffer(GL_FRAMEBUFFER, self.framebuffer);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, self.colorbuffer);
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, readFBO);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, drawFBO);
    glBindRenderbuffer(GL_RENDERBUFFER, oldRBO);
    self.drawableDirty = !(allocated && status == GL_FRAMEBUFFER_COMPLETE && width > 0 && height > 0);
    th20_ios_log("graphics drawable points=%.0fx%.0f pixels=%dx%d status=0x%x allocated=%d",
                 self.bounds.size.width, self.bounds.size.height, width, height, status, allocated);
    return !self.drawableDirty;
}
- (BOOL)presentFramebuffer:(GLuint)source width:(int)width height:(int)height {
    if (width <= 0 || height <= 0 || ![self prepareDrawable]) return NO;
    GLint readFBO = 0, drawFBO = 0, renderbuffer = 0;
    GLboolean colorMask[4]; GLfloat clearColor[4];
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &readFBO);
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &drawFBO);
    glGetIntegerv(GL_RENDERBUFFER_BINDING, &renderbuffer);
    glGetBooleanv(GL_COLOR_WRITEMASK, colorMask);
    glGetFloatv(GL_COLOR_CLEAR_VALUE, clearColor);
    const GLboolean scissor = glIsEnabled(GL_SCISSOR_TEST);
    glDisable(GL_SCISSOR_TEST);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, source);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, self.framebuffer);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    auto layout = th20::ios::presentation::make_layout(self.bounds.size.width, self.bounds.size.height, width, height,
        [self.owner usesPortraitBattleLayout], self.owner.view.safeAreaInsets.top);
    const double outputX = self.pixelWidth / self.bounds.size.width, outputY = self.pixelHeight / self.bounds.size.height;
    for (unsigned index = 0; index != layout.count; ++index) {
        const auto& src = layout.regions[index].source;
        const auto& dst = layout.regions[index].destination;
        // Both the composition and touch transforms use top-left coordinates;
        // convert only at this GLES boundary, preserving upright source text.
        glBlitFramebuffer(int(std::lround(src.x)), int(std::lround(height - src.y - src.height)),
                          int(std::lround(src.x + src.width)), int(std::lround(height - src.y)),
                          int(std::lround(dst.x * outputX)), int(std::lround(self.pixelHeight - (dst.y + dst.height) * outputY)),
                          int(std::lround((dst.x + dst.width) * outputX)), int(std::lround(self.pixelHeight - dst.y * outputY)),
                          GL_COLOR_BUFFER_BIT, self.owner.smoothScaling ? GL_LINEAR : GL_NEAREST);
    }
    // Keep compositing opaque without modifying the game backbuffer's alpha.
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_TRUE);
    glClearColor(0, 0, 0, 1); glClear(GL_COLOR_BUFFER_BIT);
    GLenum error = glGetError();
    glBindRenderbuffer(GL_RENDERBUFFER, self.colorbuffer);
    BOOL presented = [self.context presentRenderbuffer:GL_RENDERBUFFER];
    glBindFramebuffer(GL_READ_FRAMEBUFFER, readFBO);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, drawFBO);
    glBindRenderbuffer(GL_RENDERBUFFER, renderbuffer);
    glClearColor(clearColor[0], clearColor[1], clearColor[2], clearColor[3]);
    glColorMask(colorMask[0], colorMask[1], colorMask[2], colorMask[3]);
    if (scissor) glEnable(GL_SCISSOR_TEST);
    if (!presented || error != GL_NO_ERROR) {
        static unsigned failures = 0;
        if (failures++ < 12 || failures % 300 == 0)
            th20_ios_log("ERROR graphics present=%d GL=0x%x source=%u size=%dx%d", presented, error, source, width, height);
    }
    return presented && error == GL_NO_ERROR;
}
- (void)touchesBegan:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event { [self.owner touchesBegan:touches event:event]; }
- (void)touchesMoved:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event { [self.owner touchesMoved:touches event:event]; }
- (void)touchesEnded:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event { [self.owner touchesEnded:touches cancelled:NO]; }
- (void)touchesCancelled:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event { [self.owner touchesEnded:touches cancelled:YES]; }
- (void)dealloc {
    [EAGLContext setCurrentContext:self.context];
    GLuint fbo = self.framebuffer, rbo = self.colorbuffer;
    if (fbo) glDeleteFramebuffers(1, &fbo);
    if (rbo) glDeleteRenderbuffers(1, &rbo);
}
@end

@implementation TH20ViewController
- (void)viewDidLoad {
    [super viewDidLoad];
    host = self;
    self.view.backgroundColor = [UIColor colorWithRed:0.035 green:0.035 blue:0.055 alpha:1];
    self.appActive = YES;
    self.stage = @"等待引擎初始化";
    self.inputMode = TH20_IOS_INPUT_LOADING;
    NSUserDefaults *defaults = NSUserDefaults.standardUserDefaults;
    [defaults registerDefaults:@{@"touchSensitivity": @1.0, @"buttonOpacity": @0.55,
                                @"shotToggle": @NO, @"slowToggle": @NO, @"renderScale": @1.0,
                                @"buttonSize": @1.0, @"controlsVisible": @YES, @"leftHanded": @NO, @"haptics": @NO,
                                @"controlMode": @2, @"autoShot": @NO, @"autoSlow": @NO,
                                @"joystickDeadzone": @0.18, @"controlHeight": @0.0,
                                @"displayFPS": @60, @"smoothScaling": @YES, @"showPerformance": @NO}];
    self.sensitivity = std::clamp([defaults floatForKey:@"touchSensitivity"], 0.25f, 3.0f);
    self.buttonOpacity = std::clamp([defaults floatForKey:@"buttonOpacity"], 0.2f, 1.0f);
    self.buttonSize = std::clamp([defaults floatForKey:@"buttonSize"], 0.75f, 1.4f);
    self.controlsVisible = [defaults boolForKey:@"controlsVisible"];
    self.leftHanded = [defaults boolForKey:@"leftHanded"];
    self.haptics = [defaults boolForKey:@"haptics"];
    self.controlMode = std::clamp([defaults integerForKey:@"controlMode"], NSInteger(0), NSInteger(2));
    self.autoShot = [defaults boolForKey:@"autoShot"]; self.autoSlow = [defaults boolForKey:@"autoSlow"];
    self.autoBomb = [defaults boolForKey:@"autoBomb"]; self.developerMode = [defaults boolForKey:@"developerMode"];
    [self syncCombatOptions];
    self.joystickDeadzone = std::clamp([defaults floatForKey:@"joystickDeadzone"], 0.1f, 0.4f);
    self.controlHeight = std::clamp([defaults floatForKey:@"controlHeight"], 0.0f, 0.3f);
    self.displayFPS = [defaults integerForKey:@"displayFPS"] == 30 ? 30 : 60;
    self.smoothScaling = [defaults boolForKey:@"smoothScaling"];
    self.showPerformance = [defaults boolForKey:@"showPerformance"];
    self.feedback = [[UIImpactFeedbackGenerator alloc] initWithStyle:UIImpactFeedbackStyleLight];
    self.shotToggle = [defaults boolForKey:@"shotToggle"];
    self.slowToggle = [defaults boolForKey:@"slowToggle"];
    self.renderScale = std::clamp([defaults floatForKey:@"renderScale"], 0.5f, 1.0f);
    self.gameView = [TH20GLView new]; self.gameView.owner = self;
    [self.view addSubview:self.gameView];
    self.controls = [NSMutableArray new];
    self.keyDownTicks = [NSMutableDictionary new]; self.pendingKeyUps = [NSMutableSet new];
    self.customLayouts = [[defaults dictionaryForKey:@"customLayoutsV2"] mutableCopy] ?: [NSMutableDictionary new];
    NSArray<NSString *> *titles = @[@"Z", @"S", @"X", @"Ⅱ", @"⚙"];
    for (NSUInteger index = 0; index < titles.count; ++index) {
        UIButton *button = [UIButton buttonWithType:UIButtonTypeSystem];
        button.tag = index; button.multipleTouchEnabled = NO; button.exclusiveTouch = NO;
        [button setTitle:titles[index] forState:UIControlStateNormal];
        button.titleLabel.font = [UIFont systemFontOfSize:index < 3 ? 25 : 21 weight:UIFontWeightMedium];
        button.accessibilityLabel = titles[index];
        [button setTitleColor:UIColor.whiteColor forState:UIControlStateNormal];
        button.backgroundColor = [UIColor colorWithWhite:0.18 alpha:1];
        button.layer.cornerRadius = 15; button.layer.borderWidth = 1;
        button.layer.borderColor = [UIColor colorWithWhite:0.5 alpha:0.7].CGColor;
        button.alpha = self.buttonOpacity;
        [button addTarget:self action:@selector(buttonDown:) forControlEvents:UIControlEventTouchDown];
        [button addTarget:self action:@selector(buttonUp:) forControlEvents:UIControlEventTouchUpInside | UIControlEventTouchUpOutside | UIControlEventTouchCancel];
        [self.controls addObject:button]; [self.view addSubview:button];
        UIPanGestureRecognizer *drag = [[UIPanGestureRecognizer alloc] initWithTarget:self action:@selector(dragLayoutControl:)];
        drag.enabled = NO; [button addGestureRecognizer:drag];
    }
    self.joystick = [[TH20Joystick alloc] initWithFrame:CGRectZero]; self.joystick.owner = self;
    self.joystick.tag = 5;
    UIPanGestureRecognizer *stickDrag = [[UIPanGestureRecognizer alloc] initWithTarget:self action:@selector(dragLayoutControl:)];
    stickDrag.enabled = NO; [self.joystick addGestureRecognizer:stickDrag];
    [self.view addSubview:self.joystick];
    self.devLauncher = [UIButton buttonWithType:UIButtonTypeCustom];
    [self.devLauncher setTitle:@"DEV" forState:UIControlStateNormal];
    self.devLauncher.titleLabel.font = [UIFont fontWithName:@"Menlo-Bold" size:19];
    [self.devLauncher setTitleColor:[UIColor colorWithRed:1 green:0.9 blue:0.7 alpha:1] forState:UIControlStateNormal];
    self.devLauncher.backgroundColor = [UIColor colorWithRed:0.3 green:0.08 blue:0.2 alpha:0.9];
    self.devLauncher.layer.borderWidth = 1.5;
    self.devLauncher.layer.borderColor = [UIColor colorWithRed:0.83 green:0.64 blue:0.38 alpha:1].CGColor;
    self.devLauncher.accessibilityLabel = @"DEV 开发者菜单";
    [self.devLauncher addTarget:self action:@selector(openDeveloper) forControlEvents:UIControlEventTouchUpInside];
    [self.view addSubview:self.devLauncher];
    self.performanceLabel = [UILabel new]; self.performanceLabel.userInteractionEnabled = NO;
    self.performanceLabel.font = [UIFont monospacedDigitSystemFontOfSize:12 weight:UIFontWeightMedium];
    self.performanceLabel.textColor = UIColor.whiteColor; self.performanceLabel.numberOfLines = 2;
    self.performanceLabel.backgroundColor = [UIColor colorWithWhite:0 alpha:0.55];
    self.performanceLabel.text = @"FPS —\n更新 — Hz"; self.performanceLabel.hidden = !self.showPerformance;
    [self.view addSubview:self.performanceLabel];
    self.statusPanel = [UIView new];
    self.statusPanel.backgroundColor = [UIColor colorWithWhite:0.08 alpha:0.97];
    self.statusPanel.layer.cornerRadius = 18;
    self.statusLabel = [UILabel new]; self.statusLabel.numberOfLines = 0;
    self.statusLabel.textColor = UIColor.whiteColor; self.statusLabel.textAlignment = NSTextAlignmentCenter;
    self.statusLabel.font = [UIFont systemFontOfSize:16];
    self.statusLabel.text = @"正在初始化原生引擎…\n详细日志保存在 文件 → 东方锦上京 → Logs。";
    [self.statusPanel addSubview:self.statusLabel];
    self.exportButton = [UIButton buttonWithType:UIButtonTypeSystem];
    [self.exportButton setTitle:@"导出诊断日志" forState:UIControlStateNormal];
    [self.exportButton addTarget:self action:@selector(exportLogs:) forControlEvents:UIControlEventTouchUpInside];
    [self.statusPanel addSubview:self.exportButton]; [self.view addSubview:self.statusPanel];
    if (![self.gameView createContext]) { [self showError:@"无法创建 OpenGL ES 3 原生图形上下文。"]; return; }
    self.displayLink = [CADisplayLink displayLinkWithTarget:self selector:@selector(frame:)];
    self.displayLink.preferredFramesPerSecond = 60;
    [self.displayLink addToRunLoop:NSRunLoop.mainRunLoop forMode:NSRunLoopCommonModes];
    [self applyDisplaySettings];
    th20_ios_log("host loaded sensitivity=%.2f opacity=%.2f Ztoggle=%d Stoggle=%d renderScale=%.2f",
                 self.sensitivity, self.buttonOpacity, self.shotToggle, self.slowToggle, self.renderScale);
}
- (BOOL)prefersStatusBarHidden { return YES; }
- (BOOL)prefersHomeIndicatorAutoHidden { return YES; }
- (UIRectEdge)preferredScreenEdgesDeferringSystemGestures { return UIRectEdgeAll; }
- (void)viewDidLayoutSubviews {
    [super viewDidLayoutSubviews];
    // The game owns the complete screen. Controls float above the fitted 4:3
    // image; safe-area margins protect the controls, never shrink the game.
    CGRect bounds = self.view.bounds;
    CGRect safe = UIEdgeInsetsInsetRect(bounds, self.view.safeAreaInsets);
    if ([self usesPortraitBattleLayout]) self.gameView.frame = bounds;
    else {
        CGFloat fit = std::min(bounds.size.width / logicalWidth, bounds.size.height / logicalHeight);
        CGSize size = CGSizeMake(logicalWidth * fit, logicalHeight * fit);
        self.gameView.frame = CGRectMake(CGRectGetMidX(bounds) - size.width / 2,
                                        CGRectGetMidY(bounds) - size.height / 2, size.width, size.height);
    }
    CGFloat unit = std::min(CGFloat(74), std::min(safe.size.width, safe.size.height) * 0.16) * self.buttonSize;
    CGFloat margin = std::max(CGFloat(12), unit * 0.25);
    CGFloat baseline = CGRectGetMaxY(safe) - margin - unit / 2 - safe.size.height * self.controlHeight;
    CGFloat right = CGRectGetMaxX(safe) - margin - unit / 2;
    CGPoint centers[] = {CGPointMake(right, baseline - unit * 0.43),
                         CGPointMake(right - unit * 0.95, baseline - unit * 1.15),
                         CGPointMake(right - unit * 1.13, baseline + unit * 0.03)};
    for (NSUInteger i = 0; i < 3; ++i) {
        UIButton *button = self.controls[i];
        CGPoint center = centers[i];
        if (self.leftHanded) center.x = CGRectGetMinX(safe) + CGRectGetMaxX(safe) - center.x;
        CGFloat diameter = i == 0 ? unit : unit * 0.86;
        button.frame = CGRectMake(center.x - diameter / 2, center.y - diameter / 2, diameter, diameter);
        button.layer.cornerRadius = diameter / 2;
        button.hidden = !self.editingLayout && !self.controlsVisible;
    }
    CGFloat small = std::max(CGFloat(42), unit * 0.65);
    for (NSUInteger i = 3; i < 5; ++i) {
        UIButton *button = self.controls[i];
        CGFloat x = CGRectGetMaxX(safe) - 10 - small - (i - 3) * (small + 8);
        button.frame = CGRectMake(x, CGRectGetMinY(safe) + 10, small, small);
        button.layer.cornerRadius = small / 2;
    }
    CGFloat stickSize = unit * 1.9;
    CGFloat stickX = CGRectGetMinX(safe) + margin + stickSize / 2;
    if (self.leftHanded) stickX = CGRectGetMinX(safe) + CGRectGetMaxX(safe) - stickX;
    CGFloat stickY = CGRectGetMaxY(safe) - margin - stickSize / 2 - safe.size.height * self.controlHeight;
    self.joystick.frame = CGRectMake(stickX - stickSize / 2, stickY - stickSize / 2, stickSize, stickSize);
    self.joystick.alpha = self.buttonOpacity;
    self.joystick.hidden = !self.editingLayout && (!self.controlsVisible ||
        (self.controlMode == 1 && self.inputMode == TH20_IOS_INPUT_GAMEPLAY));
    NSDictionary *positions = self.customLayouts[[self layoutOrientation]];
    NSMutableArray<UIView *> *movable = [NSMutableArray arrayWithArray:self.controls]; [movable addObject:self.joystick];
    for (UIView *control in movable) {
        NSArray *point = positions[[NSString stringWithFormat:@"%ld", (long)control.tag]];
        if ([point isKindOfClass:NSArray.class] && point.count == 2) {
            CGFloat halfW = control.bounds.size.width / 2, halfH = control.bounds.size.height / 2;
            CGFloat x = safe.origin.x + [point[0] doubleValue] * safe.size.width;
            CGFloat y = safe.origin.y + [point[1] doubleValue] * safe.size.height;
            control.center = CGPointMake(std::clamp(x, CGRectGetMinX(safe) + halfW, CGRectGetMaxX(safe) - halfW),
                                         std::clamp(y, CGRectGetMinY(safe) + halfH, CGRectGetMaxY(safe) - halfH));
        }
        control.alpha = self.editingLayout ? 0.9 : self.buttonOpacity;
    }
    if (self.layoutToolbar) {
        CGFloat toolbarW = std::min(CGFloat(380), safe.size.width - 20);
        self.layoutToolbar.frame = CGRectMake(CGRectGetMidX(safe) - toolbarW / 2, CGRectGetMinY(safe) + 12, toolbarW, 78);
        for (UIView *item in self.layoutToolbar.subviews) {
            if ([item isKindOfClass:UIButton.class]) item.frame = CGRectMake(item.tag * toolbarW / 3, 0, toolbarW / 3, 44);
            else item.frame = CGRectMake(8, 44, toolbarW - 16, 26);
        }
    }
    self.performanceLabel.frame = CGRectMake(CGRectGetMinX(safe) + 8, CGRectGetMinY(safe) + 8, 165, 38);
    self.performanceLabel.hidden = !self.showPerformance;
    CGFloat panelW = std::min(500.0, safe.size.width - 28), panelH = std::min(260.0, safe.size.height - 20);
    self.statusPanel.frame = CGRectMake(CGRectGetMidX(safe) - panelW / 2, CGRectGetMidY(safe) - panelH / 2, panelW, panelH);
    self.statusLabel.frame = CGRectMake(16, 16, panelW - 32, panelH - 86);
    self.exportButton.frame = CGRectMake(16, panelH - 64, panelW - 32, 48);
    self.devLauncher.frame = CGRectMake(CGRectGetMaxX(safe) - 80, CGRectGetMinY(safe) + small + 22, 68, 44);
    self.devLauncher.hidden = !self.developerMode || !self.combatScene || !self.ready || self.editingLayout || self.fatalError;
    if (self.devOverlay) {
        self.devOverlay.frame = bounds;
        CGFloat width = std::min(CGFloat(420), safe.size.width - 24);
        CGFloat height = std::min(CGFloat(574), safe.size.height - 16);
        self.devPanel.frame = CGRectMake(CGRectGetMidX(safe) - width / 2, CGRectGetMidY(safe) - height / 2, width, height);
        self.devHeader.frame = CGRectMake(18, 14, width - 36, 58);
        self.devList.frame = CGRectMake(18, 78, width - 36, height - 124);
        self.devList.contentSize = CGSizeMake(width - 36, self.devButtons.count * 54);
        for (NSUInteger i = 0; i < self.devButtons.count; ++i)
            self.devButtons[i].frame = CGRectMake(0, i * 54, width - 36, 47);
        self.devStatus.frame = CGRectMake(18, height - 41, width - 36, 30);
        [self.view bringSubviewToFront:self.devOverlay];
    }
}
- (void)syncCombatOptions {
    if (!self.developerMode) self.devInvincible = NO;
    if (callbacks.combat_options) callbacks.combat_options(callbacks.userdata, self.developerMode, self.autoBomb);
    [self.view setNeedsLayout];
}
- (void)openDeveloper {
    if (!self.developerMode || !self.combatScene || !self.ready || self.modalPaused || self.devOverlay) return;
    [self clearInput]; self.modalPaused = YES; [self applyPausedState];
    self.devOverlay = [UIView new];
    self.devOverlay.backgroundColor = [UIColor colorWithWhite:0 alpha:0.5];
    [self.view addSubview:self.devOverlay];
    self.devPanel = [TH20DevPanel new]; self.devPanel.contentMode = UIViewContentModeRedraw;
    [self.devOverlay addSubview:self.devPanel];
    self.devHeader = [UILabel new]; self.devHeader.numberOfLines = 2;
    self.devHeader.text = @"東方錦上京  ◆  DEV\nFOSSILIZED WONDERS";
    self.devHeader.font = [UIFont fontWithName:@"HiraginoMinchoProN-W6" size:21] ?: [UIFont boldSystemFontOfSize:21];
    self.devHeader.textAlignment = NSTextAlignmentCenter;
    self.devHeader.textColor = [UIColor colorWithRed:1 green:0.9 blue:0.7 alpha:1];
    [self.devPanel addSubview:self.devHeader];
    self.devList = [UIScrollView new]; self.devList.alwaysBounceVertical = YES;
    [self.devPanel addSubview:self.devList];
    self.devButtons = [NSMutableArray new];
    NSArray *labels = @[@"INVINCIBLE  /  无敌", @"MAX SCORE  /  最高分", @"MAX ITEMS  /  道具最大",
        @"MAX POWER  /  满火力", @"FULL STOCK  /  满残机·符卡", @"MAX ALL  /  全部最大", @"CLEAR BULLETS  /  清弹", @"CLOSE  /  关闭"];
    for (NSInteger i = 0; i < labels.count; ++i) {
        UIButton *button = [UIButton buttonWithType:UIButtonTypeCustom]; button.tag = i;
        [button setTitle:i == 0 ? [NSString stringWithFormat:@"INVINCIBLE  /  无敌  %@", self.devInvincible ? @"ON" : @"OFF"] : labels[i] forState:UIControlStateNormal];
        button.titleLabel.font = [UIFont fontWithName:@"Menlo-Bold" size:17];
        button.titleLabel.adjustsFontSizeToFitWidth = YES; button.titleLabel.minimumScaleFactor = 0.7;
        button.contentEdgeInsets = UIEdgeInsetsMake(0, 12, 0, 12);
        button.contentHorizontalAlignment = UIControlContentHorizontalAlignmentLeft;
        [button setTitleColor:[UIColor colorWithRed:1 green:0.88 blue:0.81 alpha:1] forState:UIControlStateNormal];
        [button setTitleColor:UIColor.whiteColor forState:UIControlStateHighlighted];
        button.backgroundColor = [UIColor colorWithRed:i == 7 ? 0.36 : 0.23 green:0.08 blue:0.22 alpha:0.9];
        button.layer.borderWidth = 1; button.layer.borderColor = [UIColor colorWithRed:0.72 green:0.4 blue:0.5 alpha:1].CGColor;
        [button addTarget:self action:@selector(devAction:) forControlEvents:UIControlEventTouchUpInside];
        [self.devButtons addObject:button]; [self.devList addSubview:button];
    }
    self.devStatus = [UILabel new]; self.devStatus.text = @"战斗已暂停 · 关闭菜单继续";
    self.devStatus.textAlignment = NSTextAlignmentCenter; self.devStatus.numberOfLines = 2;
    self.devStatus.font = [UIFont systemFontOfSize:12];
    self.devStatus.textColor = [UIColor colorWithRed:0.88 green:0.74 blue:0.61 alpha:1];
    [self.devPanel addSubview:self.devStatus]; [self.view setNeedsLayout];
}
- (void)devAction:(UIButton *)button {
    if (button.tag == 7) { [self closeDeveloper]; return; }
    if (!self.devOverlay || !self.developerMode || !self.combatScene) return;
    int result = callbacks.dev_action && self.engineAvailable ? callbacks.dev_action(callbacks.userdata, (int)button.tag) : 0;
    if (button.tag == 0 && result > 0) {
        self.devInvincible = result == 2;
        [button setTitle:[NSString stringWithFormat:@"INVINCIBLE  /  无敌  %@", self.devInvincible ? @"ON" : @"OFF"] forState:UIControlStateNormal];
    }
    self.devStatus.text = result > 0 ? @"已应用 · 关闭菜单继续战斗" : result == 0 ? @"当前不可用（回放中不启用作弊）" : @"操作失败，请导出诊断日志";
}
- (void)closeDeveloper {
    if (!self.devOverlay) return;
    [self.devOverlay removeFromSuperview]; self.devOverlay = nil; self.devPanel = nil;
    self.devList = nil; self.devButtons = nil; self.devHeader = nil; self.devStatus = nil;
    self.modalPaused = NO; [self applyPausedState];
}
- (void)applyDisplaySettings {
    self.displayLink.preferredFramesPerSecond = self.displayFPS;
    self.gameView.contentScaleFactor = UIScreen.mainScreen.scale * self.renderScale;
    self.gameView.drawableDirty = YES;
    self.performanceLabel.hidden = !self.showPerformance;
}
- (void)applyAutomaticInput {
    if (!self.ready || !self.appActive || self.modalPaused || self.inputMode != TH20_IOS_INPUT_GAMEPLAY) return;
    if (self.autoShot && !self.shotHeld) { self.shotHeld = YES; [self sendKey:0x5a down:YES]; }
    if (self.autoSlow && !self.slowHeld) { self.slowHeld = YES; [self sendKey:0x10 down:YES]; }
}
- (NSString *)layoutOrientation { return self.view.bounds.size.width >= self.view.bounds.size.height ? @"landscape" : @"portrait"; }
- (BOOL)usesPortraitBattleLayout {
    // Dialogue and pause retain the battle composition. The scene bridge
    // explicitly excludes Options/Help/Key Config and title transitions.
    return self.combatScene && logicalWidth * 3 == logicalHeight * 4 &&
        self.view.bounds.size.height > self.view.bounds.size.width;
}
- (void)beginLayoutEditing {
    [self clearInput]; self.modalPaused = YES; [self applyPausedState]; self.editingLayout = YES;
    self.layoutBackup = [self.customLayouts copy];
    self.layoutToolbar = [UIView new]; self.layoutToolbar.backgroundColor = [UIColor colorWithWhite:0.08 alpha:0.92];
    self.layoutToolbar.layer.cornerRadius = 14;
    NSArray *titles = @[@"保存", @"重置本方向", @"取消"];
    NSArray *selectors = @[@"saveLayout", @"resetLayout", @"cancelLayout"];
    for (NSInteger i = 0; i != 3; ++i) {
        UIButton *button = [UIButton buttonWithType:UIButtonTypeSystem]; button.tag = i;
        [button setTitle:titles[i] forState:UIControlStateNormal]; [button setTitleColor:UIColor.whiteColor forState:UIControlStateNormal];
        [button addTarget:self action:NSSelectorFromString(selectors[i]) forControlEvents:UIControlEventTouchUpInside];
        [self.layoutToolbar addSubview:button];
    }
    UILabel *hint = [UILabel new]; hint.text = @"拖动任意按键和摇杆 · 横竖屏分别保存";
    hint.textColor = UIColor.whiteColor; hint.textAlignment = NSTextAlignmentCenter; hint.font = [UIFont systemFontOfSize:12];
    [self.layoutToolbar addSubview:hint]; [self.view addSubview:self.layoutToolbar];
    NSMutableArray<UIView *> *movable = [NSMutableArray arrayWithArray:self.controls]; [movable addObject:self.joystick];
    for (UIView *control in movable) for (UIGestureRecognizer *gesture in control.gestureRecognizers) gesture.enabled = YES;
    [self.view setNeedsLayout];
}
- (void)dragLayoutControl:(UIPanGestureRecognizer *)gesture {
    if (!self.editingLayout) return;
    UIView *control = gesture.view;
    CGRect safe = UIEdgeInsetsInsetRect(self.view.bounds, self.view.safeAreaInsets);
    CGPoint delta = [gesture translationInView:self.view]; [gesture setTranslation:CGPointZero inView:self.view];
    CGFloat halfW = control.bounds.size.width / 2, halfH = control.bounds.size.height / 2;
    control.center = CGPointMake(std::clamp(control.center.x + delta.x, CGRectGetMinX(safe) + halfW, CGRectGetMaxX(safe) - halfW),
                                 std::clamp(control.center.y + delta.y, CGRectGetMinY(safe) + halfH, CGRectGetMaxY(safe) - halfH));
    NSString *orientation = [self layoutOrientation];
    NSMutableDictionary *positions = [self.customLayouts[orientation] mutableCopy] ?: [NSMutableDictionary new];
    positions[[NSString stringWithFormat:@"%ld", (long)control.tag]] = @[@((control.center.x - safe.origin.x) / safe.size.width),
        @((control.center.y - safe.origin.y) / safe.size.height)];
    self.customLayouts[orientation] = positions;
}
- (void)finishLayoutEditing {
    self.editingLayout = NO; [self.layoutToolbar removeFromSuperview]; self.layoutToolbar = nil;
    NSMutableArray<UIView *> *movable = [NSMutableArray arrayWithArray:self.controls]; [movable addObject:self.joystick];
    for (UIView *control in movable) for (UIGestureRecognizer *gesture in control.gestureRecognizers) gesture.enabled = NO;
    self.modalPaused = NO; [self applyPausedState]; [self.view setNeedsLayout]; [self openSettings];
}
- (void)saveLayout {
    [NSUserDefaults.standardUserDefaults setObject:self.customLayouts forKey:@"customLayoutsV2"];
    th20_ios_log("settings custom control layout saved"); [self finishLayoutEditing];
}
- (void)resetLayout { [self.customLayouts removeObjectForKey:[self layoutOrientation]]; [self.view setNeedsLayout]; }
- (void)cancelLayout { self.customLayouts = [self.layoutBackup mutableCopy]; [self finishLayoutEditing]; }
- (void)viewWillTransitionToSize:(CGSize)size withTransitionCoordinator:(id<UIViewControllerTransitionCoordinator>)coordinator {
    [self clearInput];
    [super viewWillTransitionToSize:size withTransitionCoordinator:coordinator];
    th20_ios_log("lifecycle rotate points=%.0fx%.0f", size.width, size.height);
}
- (void)frame:(CADisplayLink *)link {
    if (self.fatalError || !self.appActive || self.modalPaused) { self.lastTimestamp = 0; return; }
    if (![self.gameView prepareDrawable]) return;
    if (!self.initialized) {
        self.initialized = YES;
        th20_ios_log("engine initialize begin");
        NSString *saves = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES).firstObject;
        if (!callbacks.initialize || !callbacks.update || !callbacks.key || !callbacks.touch) {
            [self showError:@"完整游戏引擎未正确连接：必需的初始化、更新或输入接口缺失。"]; return;
        }
        bool okay = callbacks.initialize(callbacks.userdata, NSBundle.mainBundle.resourcePath.fileSystemRepresentation, saves.fileSystemRepresentation);
        if (!okay || self.fatalError) {
            if (!self.fatalError) [self showError:@"游戏引擎初始化失败。请导出日志检查资源和运行时错误。"];
            return;
        }
        self.engineAvailable = YES;
        [self applyDisplaySettings];
        [self changeReady:YES];
        th20_ios_log("engine initialize complete");
        self.lastTimestamp = 0;
        // Initialization may take many seconds. Its display-link timestamp
        // predates that work and must not become the simulation's first delta.
        return;
    }
    if (!self.ready) return;
    double start = CACurrentMediaTime();
    if (!self.telemetryStart) self.telemetryStart = start;
    double elapsed = self.lastTimestamp ? link.timestamp - self.lastTimestamp : 1.0 / 60.0;
    self.lastTimestamp = link.timestamp;
    if (!std::isfinite(elapsed) || elapsed < 0) elapsed = 0;
    if (elapsed > 0.25) { ++self.stallCount; th20_ios_log("WARN frame stalled %.1fms stage=%s", elapsed * 1000, self.stage.UTF8String); elapsed = 0.25; }
    self.accumulator += elapsed;
    // Keep every fixed simulation update and its draw side effects, but swap
    // the drawable only once. Multiple swaps per display callback otherwise
    // block on several vblanks and starve UIKit's delivery of finger motion.
    struct PresentationBatch {
        PresentationBatch() { deferPresentation = true; presentationPending = false; }
        ~PresentationBatch() { deferPresentation = false; presentationPending = false; }
    } presentationBatch;
    unsigned steps = 0;
    while (self.accumulator + 1e-9 >= 1.0 / 60.0 && steps < 5) {
        [self applyAutomaticInput];
        callbacks.update(callbacks.userdata, 1.0 / 60.0);
        ++self.updateTick;
        // A tap shorter than one display interval must still be sampled by the
        // simulation (especially bomb and menu confirm).
        for (NSNumber *key in self.pendingKeyUps) {
            callbacks.key(callbacks.userdata, key.intValue, false);
            [self.keyDownTicks removeObjectForKey:key];
        }
        [self.pendingKeyUps removeAllObjects];
        self.accumulator -= 1.0 / 60.0; ++steps; ++self.telemetryUpdates;
        if (self.pauseHeld) {
            self.pauseHeld = NO; callbacks.key(callbacks.userdata, 0x1b, false);
            [self.keyDownTicks removeObjectForKey:@(0x1b)]; [self.pendingKeyUps removeObject:@(0x1b)];
        }
        if (self.fatalError || !self.ready) return;
    }
    if (self.accumulator >= 1.0 / 60.0) {
        ++self.stallCount;
        if (self.stallCount < 6 || self.stallCount % 120 == 0)
            th20_ios_log("WARN simulation fell behind; dropped walltime=%.1fms", self.accumulator * 1000);
        self.accumulator = std::fmod(self.accumulator, 1.0 / 60.0);
    }
    if (callbacks.render) callbacks.render(callbacks.userdata);
    deferPresentation = false;
    if (presentationPending && !th20_ios_graphics_present(pendingFramebuffer, pendingWidth, pendingHeight)) [self showError:@"画面显示失败，请导出诊断日志。"];
    presentationPending = false;
    ++self.telemetryFrames;
    self.maxFrameMS = std::max(self.maxFrameMS, (CACurrentMediaTime() - start) * 1000);
    if (self.showPerformance && self.telemetryFrames % 30 == 0 && start > self.telemetryStart) {
        self.performanceLabel.text = [NSString stringWithFormat:@" FPS %.0f · %.0f MB\n 更新 %.0f Hz",
            self.telemetryFrames / (start - self.telemetryStart), residentBytes() / 1048576.0,
            self.telemetryUpdates / (start - self.telemetryStart)];
    }
    if (start - self.telemetryStart >= 10.0) {
        th20_ios_log("frame-summary stage=%s fps=%.2f updateHz=%.2f maxCPUms=%.2f residentMiB=%.1f presents=%llu stalls=%llu",
                     self.stage.UTF8String, self.telemetryFrames / (start - self.telemetryStart),
                     self.telemetryUpdates / (start - self.telemetryStart), self.maxFrameMS,
                     residentBytes() / 1048576.0, self.presentedFrames, self.stallCount);
        self.telemetryStart = start; self.telemetryFrames = self.telemetryUpdates = 0; self.maxFrameMS = 0;
        th20_ios_flush_log();
    }
}
- (void)sendKey:(int)key down:(BOOL)down {
    if (!callbacks.key || !self.engineAvailable) return;
    NSNumber *number = @(key);
    if (down) {
        [self.pendingKeyUps removeObject:number];
        if (!self.keyDownTicks[number]) self.keyDownTicks[number] = @(self.updateTick);
        callbacks.key(callbacks.userdata, key, true);
    } else if (self.keyDownTicks[number] && self.keyDownTicks[number].unsignedLongLongValue == self.updateTick) {
        [self.pendingKeyUps addObject:number];
    } else {
        [self.keyDownTicks removeObjectForKey:number]; callbacks.key(callbacks.userdata, key, false);
    }
}
- (void)updateControlColors {
    for (UIButton *button in self.controls) {
        BOOL held = (button.tag == 0 && self.shotHeld) || (button.tag == 1 && self.slowHeld) || (button.tag == 2 && self.bombHeld);
        button.backgroundColor = held ? [UIColor colorWithRed:0.27 green:0.2 blue:0.53 alpha:1] : [UIColor colorWithWhite:0.18 alpha:1];
        button.alpha = self.buttonOpacity;
    }
    self.joystick.alpha = self.buttonOpacity;
}
- (void)buttonDown:(UIButton *)button {
    if (self.editingLayout) return;
    if (button.tag == 4) { [self openSettings]; return; }
    if (!self.ready || self.fatalError || self.modalPaused || self.inputMode == TH20_IOS_INPUT_LOADING) return;
    if (self.haptics) [self.feedback impactOccurred];
    BOOL gameplay = self.inputMode == TH20_IOS_INPUT_GAMEPLAY;
    switch (button.tag) {
        case 0:
            if (gameplay && self.shotToggle && !self.autoShot) self.shotLatched = !self.shotLatched;
            self.shotHeld = gameplay && self.autoShot ? YES : (gameplay && self.shotToggle ? self.shotLatched : YES);
            [self sendKey:0x5a down:self.shotHeld]; break;
        case 1:
            if (gameplay && self.slowToggle && !self.autoSlow) self.slowLatched = !self.slowLatched;
            self.slowHeld = gameplay && self.autoSlow ? YES : (gameplay && self.slowToggle ? self.slowLatched : YES);
            [self sendKey:0x10 down:self.slowHeld]; break;
        case 2: self.bombHeld = YES; [self sendKey:0x58 down:YES]; break;
        case 3:
            [self clearInput]; self.pauseHeld = YES; [self sendKey:0x1b down:YES]; break;
    }
    th20_ios_log("input button=%ld down mode=%d shot=%d slow=%d", (long)button.tag, self.inputMode, self.shotHeld, self.slowHeld);
    [self updateControlColors];
}
- (void)buttonUp:(UIButton *)button {
    BOOL gameplay = self.inputMode == TH20_IOS_INPUT_GAMEPLAY;
    switch (button.tag) {
        case 0: if (!(gameplay && (self.shotToggle || self.autoShot))) { self.shotHeld = NO; [self sendKey:0x5a down:NO]; } break;
        case 1: if (!(gameplay && (self.slowToggle || self.autoSlow))) { self.slowHeld = NO; [self sendKey:0x10 down:NO]; } break;
        case 2: self.bombHeld = NO; [self sendKey:0x58 down:NO]; break;
    }
    [self updateControlColors];
}
- (CGPoint)logicalPoint:(CGPoint)point {
    CGSize size = self.gameView.bounds.size;
    const auto layout = th20::ios::presentation::make_layout(size.width, size.height, logicalWidth, logicalHeight,
        [self usesPortraitBattleLayout], self.view.safeAreaInsets.top);
    const auto logical = layout.logical_point({point.x, point.y});
    return CGPointMake(logical.x, logical.y);
}
- (void)emitTouch:(TH20IOSTouchPhase)phase point:(CGPoint)point delta:(CGPoint)delta {
    if (!callbacks.touch || !self.engineAvailable) return;
    CGPoint logical = [self logicalPoint:point];
    CGSize size = self.gameView.bounds.size;
    const auto layout = th20::ios::presentation::make_layout(size.width, size.height, logicalWidth, logicalHeight,
        [self usesPortraitBattleLayout], self.view.safeAreaInsets.top);
    const auto logicalDelta = layout.logical_delta({delta.x, delta.y});
    CGFloat sensitivity = phase == TH20_IOS_TOUCH_MENU_SWIPE ? 1 : self.sensitivity;
    callbacks.touch(callbacks.userdata, phase, self.touchID, logical.x, logical.y,
                    logicalDelta.x * sensitivity, logicalDelta.y * sensitivity);
}
- (void)touchesBegan:(NSSet<UITouch *> *)touches event:(UIEvent *)event {
    if (!self.ready || self.modalPaused || self.fatalError || self.inputMode == TH20_IOS_INPUT_LOADING) return;
    if (self.inputMode == TH20_IOS_INPUT_GAMEPLAY && self.gestureTouches.count) {
        for (UITouch *touch in touches) if (touch.view == self.gameView) {
            [self.gestureTouches addObject:touch];
            [self.gestureOrigins setObject:[NSValue valueWithCGPoint:[touch locationInView:self.gameView]] forKey:touch];
            self.gestureHadThird = YES;
        }
        [self.gestureTimer invalidate]; self.gestureTimer = nil;
        if (self.gestureTouches.count == 3 && !self.gestureInvalid &&
            touches.anyObject.timestamp - self.gestureStarted < 0.35) {
            self.gestureTimer = [NSTimer timerWithTimeInterval:0.6 target:self selector:@selector(threeFingerPause:)
                                                 userInfo:nil repeats:NO];
            [NSRunLoop.mainRunLoop addTimer:self.gestureTimer forMode:NSRunLoopCommonModes];
        } else self.gestureInvalid = YES;
        return;
    }
    NSUInteger active = 0;
    NSTimeInterval earliest = DBL_MAX, latest = 0;
    for (UITouch *candidate in event.allTouches) {
        if (candidate.view != self.gameView || candidate.phase == UITouchPhaseEnded || candidate.phase == UITouchPhaseCancelled) continue;
        ++active; earliest = std::min(earliest, candidate.timestamp); latest = std::max(latest, candidate.timestamp);
    }
    if (active <= 1) self.suppressTouches = NO;
    if (active >= 2 && !self.suppressTouches && latest - (self.moveTouch ? self.touchStart : earliest) < 0.22) {
        if (self.inputMode == TH20_IOS_INPUT_GAMEPLAY) {
            if (self.moveTouch && (self.controlMode != 0 || !self.controlsVisible))
                [self emitTouch:TH20_IOS_TOUCH_CANCEL point:self.previousPoint delta:CGPointZero];
            self.moveTouch = nil; self.suppressTouches = YES;
            self.gestureTouches = [NSMutableSet new];
            self.gestureOrigins = [NSMapTable strongToStrongObjectsMapTable];
            self.gestureStarted = earliest; self.gestureInvalid = active > 3;
            self.gestureHadThird = active >= 3;
            for (UITouch *touch in event.allTouches) if (touch.view == self.gameView &&
                touch.phase != UITouchPhaseEnded && touch.phase != UITouchPhaseCancelled) {
                [self.gestureTouches addObject:touch];
                [self.gestureOrigins setObject:[NSValue valueWithCGPoint:[touch locationInView:self.gameView]] forKey:touch];
            }
            if (active == 3) {
                self.gestureTimer = [NSTimer timerWithTimeInterval:0.6 target:self selector:@selector(threeFingerPause:)
                                                     userInfo:nil repeats:NO];
                [NSRunLoop.mainRunLoop addTimer:self.gestureTimer forMode:NSRunLoopCommonModes];
            }
        } else {
            [self clearInput]; self.suppressTouches = YES;
            [self sendKey:0x1b down:YES]; [self sendKey:0x1b down:NO];
            th20_ios_log("input two-finger back mode=%d", self.inputMode);
        }
        return;
    }
    if (self.suppressTouches || self.moveTouch) return;
    UITouch *touch = touches.anyObject;
    self.moveTouch = touch; self.touchID = ++self.nextTouchID;
    self.startPoint = self.previousPoint = self.swipeOrigin = [touch locationInView:self.gameView];
    self.touchStart = touch.timestamp; self.touchScrolled = NO;
    if (self.inputMode == TH20_IOS_INPUT_GAMEPLAY && (self.controlMode != 0 || !self.controlsVisible))
        [self emitTouch:TH20_IOS_TOUCH_BEGIN point:self.previousPoint delta:CGPointZero];
}
- (void)touchesMoved:(NSSet<UITouch *> *)touches event:(UIEvent *)event {
    if (self.gestureTouches.count) {
        for (UITouch *touch in touches) if ([self.gestureTouches containsObject:touch]) {
            CGPoint origin = [[self.gestureOrigins objectForKey:touch] CGPointValue];
            CGPoint point = [touch locationInView:self.gameView];
            if (std::hypot(point.x - origin.x, point.y - origin.y) > 18) {
                self.gestureInvalid = YES;
                [self.gestureTimer invalidate]; self.gestureTimer = nil;
            }
        }
        return;
    }
    if (self.suppressTouches || !self.moveTouch || ![touches containsObject:self.moveTouch]) return;
    CGPoint point = [self.moveTouch locationInView:self.gameView];
    CGPoint delta = CGPointMake(point.x - self.previousPoint.x, point.y - self.previousPoint.y);
    self.previousPoint = point;
    if (self.inputMode == TH20_IOS_INPUT_MENU) {
        CGPoint movement = CGPointMake(point.x - self.swipeOrigin.x, point.y - self.swipeOrigin.y);
        const auto layout = th20::ios::presentation::make_layout(self.gameView.bounds.size.width, self.gameView.bounds.size.height,
            logicalWidth, logicalHeight, [self usesPortraitBattleLayout], self.view.safeAreaInsets.top);
        const auto logical = layout.logical_delta({movement.x, movement.y});
        const auto factor = layout.logical_delta({1, 1});
        CGFloat distance = std::max(std::abs(logical.x), std::abs(logical.y));
        unsigned count = std::min(8u, unsigned(distance / 24));
        if (count) {
            BOOL horizontal = std::abs(logical.x) > std::abs(logical.y);
            CGFloat direction = (horizontal ? logical.x : logical.y) < 0 ? -1 : 1;
            CGPoint step = horizontal ? CGPointMake(direction * 24 / factor.x, 0)
                                     : CGPointMake(0, direction * 24 / factor.y);
            self.touchScrolled = YES;
            for (unsigned i = 0; i != count; ++i) [self emitTouch:TH20_IOS_TOUCH_MENU_SWIPE point:point delta:step];
            self.swipeOrigin = CGPointMake(self.swipeOrigin.x + step.x * count, self.swipeOrigin.y + step.y * count);
        }
    } else if (self.inputMode == TH20_IOS_INPUT_GAMEPLAY && (self.controlMode != 0 || !self.controlsVisible)) {
        [self emitTouch:TH20_IOS_TOUCH_MOVE point:point delta:delta];
    }
}
- (void)touchesEnded:(NSSet<UITouch *> *)touches cancelled:(BOOL)cancelled {
    if (self.gestureTouches.count) {
        if (cancelled) self.gestureInvalid = YES;
        for (UITouch *touch in touches) if ([self.gestureTouches containsObject:touch]) {
            CGPoint origin = [[self.gestureOrigins objectForKey:touch] CGPointValue];
            CGPoint point = [touch locationInView:self.gameView];
            if (std::hypot(point.x - origin.x, point.y - origin.y) > 18) self.gestureInvalid = YES;
            [self.gestureTouches removeObject:touch];
            [self.gestureOrigins removeObjectForKey:touch];
        }
        if (self.gestureTouches.count < 3) { [self.gestureTimer invalidate]; self.gestureTimer = nil; }
        if (!self.gestureTouches.count) {
            if (!self.gestureInvalid && !self.gestureHadThird &&
                touches.anyObject.timestamp - self.gestureStarted <= 0.35 &&
                self.inputMode == TH20_IOS_INPUT_GAMEPLAY) {
                [self sendKey:0x58 down:YES]; [self sendKey:0x58 down:NO];
                th20_ios_log("input two-finger bomb");
            }
            self.gestureOrigins = nil; self.gestureTouches = nil; self.suppressTouches = NO;
        }
        return;
    }
    if (!self.moveTouch || ![touches containsObject:self.moveTouch]) return;
    CGPoint point = [self.moveTouch locationInView:self.gameView];
    if (self.inputMode == TH20_IOS_INPUT_MENU || self.inputMode == TH20_IOS_INPUT_DIALOGUE) {
        double distance = std::hypot(point.x - self.startPoint.x, point.y - self.startPoint.y);
        if (!cancelled && !self.suppressTouches && !self.touchScrolled && distance <= 18 && self.moveTouch.timestamp - self.touchStart < 0.6) {
            [self emitTouch:TH20_IOS_TOUCH_MENU_TAP point:point delta:CGPointZero];
            CGPoint logical = [self logicalPoint:point];
            th20_ios_log("input menu/dialogue-tap logical=(%.1f,%.1f)", logical.x, logical.y);
        }
    } else if ((self.controlMode != 0 || !self.controlsVisible)) {
        CGPoint finalDelta = CGPointMake(point.x - self.previousPoint.x, point.y - self.previousPoint.y);
        if (!cancelled && (finalDelta.x != 0 || finalDelta.y != 0))
            [self emitTouch:TH20_IOS_TOUCH_MOVE point:point delta:finalDelta];
        [self emitTouch:cancelled ? TH20_IOS_TOUCH_CANCEL : TH20_IOS_TOUCH_END point:point delta:CGPointZero];
    }
    self.moveTouch = nil;
}
- (void)threeFingerPause:(NSTimer *)timer {
    if (timer != self.gestureTimer || self.gestureTouches.count != 3 || self.gestureInvalid ||
        self.inputMode != TH20_IOS_INPUT_GAMEPLAY || !self.ready || self.modalPaused) return;
    self.gestureTimer = nil; self.gestureInvalid = YES;
    [self sendKey:0x1b down:YES]; [self sendKey:0x1b down:NO];
    th20_ios_log("input three-finger long-press pause");
}
- (void)clearInput {
    [self.gestureTimer invalidate]; self.gestureTimer = nil;
    self.gestureTouches = nil; self.gestureOrigins = nil; self.suppressTouches = NO;
    [self.joystick reset];
    if (self.moveTouch && self.inputMode != TH20_IOS_INPUT_MENU)
        [self emitTouch:TH20_IOS_TOUCH_CANCEL point:self.previousPoint delta:CGPointZero];
    self.moveTouch = nil;
    // Scene/lifecycle boundaries must cancel queued edges immediately.
    [self.pendingKeyUps removeAllObjects]; [self.keyDownTicks removeAllObjects];
    if (callbacks.key && self.engineAvailable)
        for (int key : {0x5a, 0x10, 0x58, 0x1b, 0x25, 0x26, 0x27, 0x28}) callbacks.key(callbacks.userdata, key, false);
    if (callbacks.clear_input && self.engineAvailable) callbacks.clear_input(callbacks.userdata);
    self.shotHeld = self.slowHeld = self.bombHeld = self.pauseHeld = self.shotLatched = self.slowLatched = NO;
    [self updateControlColors];
}
- (void)setMode:(TH20IOSInputMode)mode {
    if (mode == self.inputMode) return;
    [self clearInput]; self.inputMode = mode;
    self.controls[0].accessibilityLabel = mode == TH20_IOS_INPUT_GAMEPLAY ? @"Z 射击" : @"Z 确认";
    self.controls[1].accessibilityLabel = @"S 低速";
    self.controls[2].accessibilityLabel = mode == TH20_IOS_INPUT_GAMEPLAY ? @"X 符卡" : @"X 返回";
    [self.view setNeedsLayout];
    th20_ios_log("input scene mode=%d; all held and toggle states cleared", mode);
}
- (void)showStage:(NSString *)stage {
    self.stage = stage;
    if (!self.ready && !self.fatalError) self.statusLabel.text = [NSString stringWithFormat:@"正在加载…\n%@", stage];
}
- (void)setCombatPresentation:(BOOL)active {
    if (self.combatScene == active) return;
    if (!active) [self closeDeveloper];
    [self clearInput]; self.combatScene = active;
    // Apply the scene's drawable size before the frame being submitted now;
    // otherwise one portrait frame can be composed into the old menu bounds.
    [self.view setNeedsLayout]; [self.view layoutIfNeeded];
    th20_ios_log("presentation combat=%d portrait=%d", active, [self usesPortraitBattleLayout]);
}
- (void)changeReady:(BOOL)ready {
    if (self.fatalError) return;
    self.ready = ready; self.statusPanel.hidden = ready;
    if (!ready) [self clearInput];
    th20_ios_log("engine ready=%d stage=%s", ready, self.stage.UTF8String);
}
- (void)showError:(NSString *)message {
    [self clearInput]; self.fatalError = YES; self.ready = NO;
    self.displayLink.paused = YES; th20_ios_audio_suspend(true);
    self.statusPanel.hidden = NO;
    self.statusLabel.text = [NSString stringWithFormat:@"游戏运行失败\n%@\n请导出日志协助定位。", message];
    th20_ios_log("FATAL stage=%s error=%s", self.stage.UTF8String, message.UTF8String); th20_ios_flush_log();
}
- (void)applyPausedState {
    BOOL paused = !self.appActive || self.modalPaused || self.fatalError;
    if (paused == self.enginePaused) return;
    self.enginePaused = paused; [self clearInput]; self.lastTimestamp = 0; self.accumulator = 0;
    self.displayLink.paused = paused;
    if (callbacks.pause && self.engineAvailable) callbacks.pause(callbacks.userdata, paused);
    th20_ios_audio_suspend(paused);
    th20_ios_log("lifecycle paused=%d appActive=%d modal=%d", paused, self.appActive, self.modalPaused); th20_ios_flush_log();
}
- (void)setApplicationActive:(BOOL)active { self.appActive = active; [self applyPausedState]; }
- (void)openSettings {
    if (self.presentedViewController) return;
    self.modalPaused = YES; [self applyPausedState];
    TH20SettingsController *settings = [[TH20SettingsController alloc] initWithStyle:UITableViewStyleInsetGrouped]; settings.owner = self;
    UINavigationController *navigation = [[UINavigationController alloc] initWithRootViewController:settings];
    navigation.modalPresentationStyle = UIModalPresentationFormSheet;
    navigation.preferredContentSize = CGSizeMake(620, 680);
    navigation.modalInPresentation = YES;
    [self presentViewController:navigation animated:YES completion:nil];
}
- (void)exportLogs:(id)sender {
    th20_ios_log("diagnostics export requested stage=%s residentMiB=%.1f", self.stage.UTF8String, residentBytes() / 1048576.0);
    th20_ios_flush_log();
    NSString *source = [NSString stringWithUTF8String:th20_ios_log_path()];
    NSString *copy = [NSTemporaryDirectory() stringByAppendingPathComponent:source.lastPathComponent];
    NSFileManager *files = NSFileManager.defaultManager;
    [files removeItemAtPath:copy error:nil];
    NSError *error = nil;
    if (![files copyItemAtPath:source toPath:copy error:&error]) {
        th20_ios_log("ERROR diagnostic export: %s", error.description.UTF8String); return;
    }
    BOOL wasModal = self.modalPaused; self.modalPaused = YES; [self applyPausedState];
    UIActivityViewController *share = [[UIActivityViewController alloc] initWithActivityItems:@[[NSURL fileURLWithPath:copy]] applicationActivities:nil];
    __weak TH20ViewController *weakSelf = self;
    share.completionWithItemsHandler = ^(UIActivityType type, BOOL completed, NSArray *items, NSError *shareError) {
        TH20ViewController *strong = weakSelf;
        if (!strong) return;
        strong.modalPaused = wasModal; [strong applyPausedState];
        th20_ios_log("diagnostics export completed=%d error=%s", completed, shareError ? shareError.description.UTF8String : "none");
    };
    UIViewController *presenter = self.presentedViewController ?: self;
    share.popoverPresentationController.sourceView = presenter.view;
    share.popoverPresentationController.sourceRect = CGRectMake(presenter.view.bounds.size.width / 2, 60, 1, 1);
    [presenter presentViewController:share animated:YES completion:nil];
}
- (void)didReceiveMemoryWarning {
    [super didReceiveMemoryWarning];
    th20_ios_log("WARN memory pressure stage=%s residentMiB=%.1f", self.stage.UTF8String, residentBytes() / 1048576.0); th20_ios_flush_log();
}
- (void)stop {
    [self clearInput]; [self.displayLink invalidate]; self.displayLink = nil;
    if (callbacks.shutdown && self.engineAvailable) callbacks.shutdown(callbacks.userdata);
    th20_ios_audio_suspend(true); th20_ios_log("SESSION END"); th20_ios_flush_log();
}
@end

@implementation TH20SettingsController
- (void)viewDidLoad {
    [super viewDidLoad];
    self.title = @"设置";
    self.navigationItem.rightBarButtonItem = [[UIBarButtonItem alloc] initWithTitle:@"完成" style:UIBarButtonItemStyleDone target:self action:@selector(done)];
    self.tableView.keyboardDismissMode = UIScrollViewKeyboardDismissModeOnDrag;
    self.tableView.rowHeight = UITableViewAutomaticDimension; self.tableView.estimatedRowHeight = 54;
    UITapGestureRecognizer *back = [[UITapGestureRecognizer alloc] initWithTarget:self action:@selector(done)];
    back.numberOfTouchesRequired = 2; back.cancelsTouchesInView = YES; [self.tableView addGestureRecognizer:back];
}
- (NSInteger)numberOfSectionsInTableView:(UITableView *)tableView { return 6; }
- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section {
    const NSInteger counts[] = {8, 6, 4, 4, 1, 1}; return counts[section];
}
- (NSString *)tableView:(UITableView *)tableView titleForHeaderInSection:(NSInteger)section {
    return @[@"操作方式", @"摇杆与按键", @"画面与性能", @"手势与诊断", @"Cheat Code", @"语言 / 言語"][section];
}
- (NSString *)tableView:(UITableView *)tableView titleForFooterInSection:(NSInteger)section {
    if (section == 5) return th20::ios::language::available() ? @"首次启动默认跟随系统：中文系统使用简体中文，其余使用日文。手动选择会保存。切换后重新载入主菜单；战斗中会先询问。\n初回はシステム言語に従います。切替後はタイトルへ戻ります。" : @"此构建未包含汉化资源。中文语言包需在构建时导入。";
    if (section == 0) return @"Hybrid：摇杆或拖动均可移动。Drag：相对拖动。Joystick：仅使用摇杆。No Button 隐藏操作控件，设置入口始终保留。";
    if (section == 1) return @"自由布局分别保存横屏和竖屏位置。左手布局改变默认位置；已自定义的位置以保存结果为准。";
    if (section == 2) return @"横屏和主菜单保持原始比例。竖屏战斗将状态栏移至顶部，战斗区域铺满下方。30 FPS 降低显示频率，游戏逻辑仍以 60 Hz 更新；降低显示清晰度可减少输出画面的开销。";
    return nil;
}
- (UITableViewCell *)baseCell:(NSString *)title detail:(NSString *)detail {
    UITableViewCell *cell = [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleSubtitle reuseIdentifier:nil];
    cell.textLabel.text = title; cell.textLabel.numberOfLines = 0;
    cell.detailTextLabel.text = detail; cell.detailTextLabel.numberOfLines = 0; cell.detailTextLabel.textColor = UIColor.secondaryLabelColor;
    cell.selectionStyle = UITableViewCellSelectionStyleNone;
    return cell;
}
- (UITableViewCell *)switchCell:(NSString *)title detail:(NSString *)detail tag:(NSInteger)tag value:(BOOL)value {
    UITableViewCell *cell = [self baseCell:title detail:detail];
    UISwitch *control = [UISwitch new]; control.tag = tag; control.on = value;
    [control addTarget:self action:@selector(toggleChanged:) forControlEvents:UIControlEventValueChanged]; cell.accessoryView = control;
    return cell;
}
- (UITableViewCell *)stackCell:(NSString *)title control:(UIView *)control {
    UITableViewCell *cell = [self baseCell:nil detail:nil];
    UIStackView *stack = [UIStackView new]; stack.axis = UILayoutConstraintAxisVertical; stack.spacing = 9;
    stack.translatesAutoresizingMaskIntoConstraints = NO;
    if (title) { UILabel *label = [UILabel new]; label.text = title; label.numberOfLines = 0; [stack addArrangedSubview:label]; }
    [stack addArrangedSubview:control]; [cell.contentView addSubview:stack];
    UILayoutGuide *margins = cell.contentView.layoutMarginsGuide;
    [NSLayoutConstraint activateConstraints:@[[stack.leadingAnchor constraintEqualToAnchor:margins.leadingAnchor],
        [stack.trailingAnchor constraintEqualToAnchor:margins.trailingAnchor], [stack.topAnchor constraintEqualToAnchor:margins.topAnchor constant:5],
        [stack.bottomAnchor constraintEqualToAnchor:margins.bottomAnchor constant:-5]]];
    return cell;
}
- (UITableViewCell *)segmentCell:(NSString *)title items:(NSArray *)items tag:(NSInteger)tag selected:(NSInteger)selected {
    UISegmentedControl *control = [[UISegmentedControl alloc] initWithItems:items]; control.tag = tag; control.selectedSegmentIndex = selected;
    [control addTarget:self action:@selector(segmentChanged:) forControlEvents:UIControlEventValueChanged];
    return [self stackCell:title control:control];
}
- (UITableViewCell *)sliderCell:(NSString *)title tag:(NSInteger)tag value:(float)value minimum:(float)minimum maximum:(float)maximum {
    UISlider *slider = [UISlider new]; slider.tag = tag; slider.value = value; slider.minimumValue = minimum; slider.maximumValue = maximum;
    [slider addTarget:self action:@selector(sliderChanged:) forControlEvents:UIControlEventValueChanged];
    UILabel *valueLabel = [UILabel new]; valueLabel.tag = 900 + tag; valueLabel.textAlignment = NSTextAlignmentRight;
    valueLabel.font = [UIFont monospacedDigitSystemFontOfSize:14 weight:UIFontWeightRegular];
    valueLabel.text = tag == 0 ? [NSString stringWithFormat:@"%.2f×", value] : [NSString stringWithFormat:@"%.0f%%", value * 100];
    [valueLabel.widthAnchor constraintEqualToConstant:64].active = YES;
    UIStackView *row = [[UIStackView alloc] initWithArrangedSubviews:@[slider, valueLabel]]; row.spacing = 12;
    return [self stackCell:title control:row];
}
- (UITableViewCell *)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)path {
    TH20ViewController *owner = self.owner;
    if (path.section == 5) {
        UISegmentedControl *control = [[UISegmentedControl alloc] initWithItems:@[@"系统 / 自動", @"日本語", @"简体中文"]];
        control.tag = 3; control.selectedSegmentIndex = th20::ios::language::preference();
        control.enabled = th20::ios::language::available() && self.owner.ready && callbacks.language;
        [control addTarget:self action:@selector(segmentChanged:) forControlEvents:UIControlEventValueChanged];
        return [self stackCell:@"游戏语言 / ゲーム言語" control:control];
    }
    if (path.section == 0) {
        switch (path.row) {
            case 0: return [self segmentCell:@"移动模式" items:@[@"Hybrid", @"Drag", @"Joystick"] tag:0 selected:2 - owner.controlMode];
            case 1: return [self switchCell:@"No Button" detail:@"隐藏 X / S / Z 和摇杆" tag:0 value:!owner.controlsVisible];
            case 2: { UITableViewCell *cell = [self baseCell:@"自由调整按键与摇杆位置" detail:@"拖动摆放，保存后立即生效"]; cell.accessoryType = UITableViewCellAccessoryDisclosureIndicator; cell.selectionStyle = UITableViewCellSelectionStyleDefault; return cell; }
            case 3: return [self switchCell:@"Z 点击保持射击" detail:@"再点一次停止" tag:1 value:owner.shotToggle];
            case 4: return [self switchCell:@"S 点击保持低速" detail:@"再点一次恢复高速" tag:2 value:owner.slowToggle];
            case 5: return [self switchCell:@"自动射击" detail:@"进入战斗后自动按住 Z" tag:3 value:owner.autoShot];
            case 6: return [self switchCell:@"自动低速" detail:@"进入战斗后自动按住 S" tag:4 value:owner.autoSlow];
            case 7: return [self switchCell:@"Auto Bomb / 自动符卡" detail:@"碰撞受击时，在死亡判定与音效前自动释放符卡；消耗现有符卡，无符卡时正常受击。" tag:9 value:owner.autoBomb];
        }
    }
    if (path.section == 1) {
        switch (path.row) {
            case 0: return [self sliderCell:@"按键透明度" tag:1 value:owner.buttonOpacity minimum:0.2 maximum:1];
            case 1: return [self sliderCell:@"按键与摇杆大小" tag:2 value:owner.buttonSize minimum:0.75 maximum:1.4];
            case 2: return [self sliderCell:@"拖动灵敏度" tag:0 value:owner.sensitivity minimum:0.25 maximum:3];
            case 3: return [self sliderCell:@"摇杆中心死区" tag:3 value:owner.joystickDeadzone minimum:0.1 maximum:0.4];
            case 4: return [self switchCell:@"左手按键布局" detail:@"默认摇杆与 X / S / Z 左右互换" tag:5 value:owner.leftHanded];
            case 5: return [self switchCell:@"按键触感反馈" detail:nil tag:6 value:owner.haptics];
        }
    }
    if (path.section == 2) {
        switch (path.row) {
            case 0: return [self segmentCell:@"显示帧率" items:@[@"60 FPS", @"30 FPS"] tag:1 selected:owner.displayFPS == 30 ? 1 : 0];
            case 1: return [self segmentCell:@"显示清晰度" items:@[@"50%", @"75%", @"100%"] tag:2 selected:owner.renderScale < 0.625f ? 0 : owner.renderScale < 0.875f ? 1 : 2];
            case 2: return [self switchCell:@"平滑缩放" detail:@"关闭时使用清晰的像素边缘" tag:7 value:owner.smoothScaling];
            case 3: return [self switchCell:@"显示性能信息" detail:@"帧率、更新频率和内存" tag:8 value:owner.showPerformance];
        }
    }
    if (path.section == 3) {
        if (path.row == 3) return [self switchCell:@"开发者模式" detail:@"战斗画面显示 DEV 入口：无敌、最高分、道具、火力、残机与符卡、清弹。" tag:10 value:owner.developerMode];
        if (path.row == 0) return [self baseCell:@"Z：射击 / 确认 · X：符卡 / 返回 · S：低速" detail:@"轻点菜单选项直接选择；上下连续滑动切换列表，左右滑动切换分页或数值。对话时轻点画面继续。战斗中双指轻按放符卡、三指长按暂停；菜单和设置中双指轻点返回。"];
        UITableViewCell *cell = [self baseCell:path.row == 1 ? @"导出本次诊断日志" : @"恢复默认设置与按键布局" detail:nil];
        cell.textLabel.textColor = UIColor.systemBlueColor; cell.selectionStyle = UITableViewCellSelectionStyleDefault; return cell;
    }
    self.cheatField = [UITextField new]; self.cheatField.borderStyle = UITextBorderStyleRoundedRect;
    self.cheatField.placeholder = @"输入 Cheat Code"; self.cheatField.autocapitalizationType = UITextAutocapitalizationTypeNone;
    self.cheatField.autocorrectionType = UITextAutocorrectionTypeNo; self.cheatField.spellCheckingType = UITextSpellCheckingTypeNo;
    self.cheatField.returnKeyType = UIReturnKeyGo; self.cheatField.clearButtonMode = UITextFieldViewModeWhileEditing;
    self.cheatField.delegate = self;
    [self.cheatField addTarget:self action:@selector(cheatChanged:) forControlEvents:UIControlEventEditingChanged];
    UIButton *submit = [UIButton buttonWithType:UIButtonTypeSystem]; [submit setTitle:@"应用" forState:UIControlStateNormal];
    [submit addTarget:self action:@selector(submitCheat) forControlEvents:UIControlEventTouchUpInside];
    [submit.widthAnchor constraintEqualToConstant:55].active = YES;
    UIStackView *row = [[UIStackView alloc] initWithArrangedSubviews:@[self.cheatField, submit]]; row.spacing = 8;
    self.cheatResult = [UILabel new]; self.cheatResult.font = [UIFont systemFontOfSize:13]; self.cheatResult.numberOfLines = 0;
    self.cheatResult.textColor = UIColor.secondaryLabelColor; self.cheatResult.text = @"输入有效代码后自动应用并保存。";
    UIStackView *content = [[UIStackView alloc] initWithArrangedSubviews:@[row, self.cheatResult]]; content.axis = UILayoutConstraintAxisVertical; content.spacing = 8;
    return [self stackCell:nil control:content];
}
- (void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)path {
    [tableView deselectRowAtIndexPath:path animated:YES];
    if (path.section == 0 && path.row == 2) {
        [self.view endEditing:YES]; __weak TH20ViewController *owner = self.owner;
        [self dismissViewControllerAnimated:YES completion:^{ [owner beginLayoutEditing]; }];
    } else if (path.section == 3 && path.row == 1) [self.owner exportLogs:self];
    else if (path.section == 3 && path.row == 2) [self restoreDefaults];
}
- (void)toggleChanged:(UISwitch *)control {
    [self.owner clearInput];
    NSArray *keys = @[@"controlsVisible", @"shotToggle", @"slowToggle", @"autoShot", @"autoSlow", @"leftHanded", @"haptics", @"smoothScaling", @"showPerformance", @"autoBomb", @"developerMode"];
    BOOL value = control.tag == 0 ? !control.on : control.on;
    [self.owner setValue:@(value) forKey:keys[control.tag]];
    [NSUserDefaults.standardUserDefaults setBool:value forKey:keys[control.tag]];
    [self.owner syncCombatOptions];
    [self.owner updateControlColors]; [self.owner.view setNeedsLayout];
    th20_ios_log("settings %s=%d", [keys[control.tag] UTF8String], value);
}
- (void)segmentChanged:(UISegmentedControl *)control {
    if (control.tag == 3) {
        const NSInteger requested = control.selectedSegmentIndex;
        control.selectedSegmentIndex = th20::ios::language::preference();
        if (requested == th20::ios::language::preference()) return;
        void (^apply)(void) = ^{
            [self.owner clearInput];
            if (callbacks.language && callbacks.language(callbacks.userdata, (int)requested)) {
                control.selectedSegmentIndex = requested;
                self.owner.lastTimestamp = 0; self.owner.accumulator = 0;
                [self.tableView reloadData]; [self done];
            }
        };
        if (self.owner.combatScene) {
            UIAlertController *alert = [UIAlertController alertControllerWithTitle:@"切换语言 / 言語切替" message:@"切换会结束当前战斗并返回主菜单，已保存进度会保留。\n現在のプレイを終了してタイトルへ戻ります。保存済みの記録は保持されます。" preferredStyle:UIAlertControllerStyleAlert];
            [alert addAction:[UIAlertAction actionWithTitle:@"取消 / キャンセル" style:UIAlertActionStyleCancel handler:nil]];
            [alert addAction:[UIAlertAction actionWithTitle:@"切换 / 切替" style:UIAlertActionStyleDefault handler:^(UIAlertAction *action){ apply(); }]];
            [self presentViewController:alert animated:YES completion:nil];
        } else apply();
        return;
    }
    [self.owner clearInput]; NSString *key; NSNumber *value;
    if (control.tag == 0) { self.owner.controlMode = 2 - control.selectedSegmentIndex; key = @"controlMode"; value = @(self.owner.controlMode); }
    else if (control.tag == 1) { self.owner.displayFPS = control.selectedSegmentIndex == 1 ? 30 : 60; key = @"displayFPS"; value = @(self.owner.displayFPS); }
    else { self.owner.renderScale = 0.5f + 0.25f * control.selectedSegmentIndex; key = @"renderScale"; value = @(self.owner.renderScale); }
    [NSUserDefaults.standardUserDefaults setObject:value forKey:key]; [self.owner applyDisplaySettings]; [self.owner.view setNeedsLayout];
    th20_ios_log("settings %s=%s", key.UTF8String, value.description.UTF8String);
}
- (void)sliderChanged:(UISlider *)slider {
    NSArray *keys = @[@"sensitivity", @"buttonOpacity", @"buttonSize", @"joystickDeadzone"];
    NSString *key = keys[slider.tag]; [self.owner setValue:@(slider.value) forKey:key];
    [NSUserDefaults.standardUserDefaults setFloat:slider.value forKey:slider.tag == 0 ? @"touchSensitivity" : key];
    UILabel *label = [self.tableView viewWithTag:900 + slider.tag];
    label.text = slider.tag == 0 ? [NSString stringWithFormat:@"%.2f×", slider.value] : [NSString stringWithFormat:@"%.0f%%", slider.value * 100];
    [self.owner updateControlColors]; [self.owner.view setNeedsLayout];
}
- (void)restoreDefaults {
    [self.owner clearInput];
    for (NSString *key in @[@"autoBomb", @"developerMode"]) [NSUserDefaults.standardUserDefaults removeObjectForKey:key];
    self.owner.autoBomb = self.owner.developerMode = NO; [self.owner syncCombatOptions];
    for (NSString *key in @[@"shotToggle", @"slowToggle", @"controlsVisible", @"leftHanded", @"haptics", @"touchSensitivity", @"buttonOpacity", @"buttonSize", @"renderScale", @"controlMode", @"autoShot", @"autoSlow", @"joystickDeadzone", @"controlHeight", @"displayFPS", @"smoothScaling", @"showPerformance", @"customLayoutsV2"])
        [NSUserDefaults.standardUserDefaults removeObjectForKey:key];
    self.owner.shotToggle = self.owner.slowToggle = self.owner.leftHanded = self.owner.haptics = NO;
    self.owner.autoShot = self.owner.autoSlow = self.owner.showPerformance = NO;
    self.owner.controlsVisible = self.owner.smoothScaling = YES;
    self.owner.sensitivity = 1; self.owner.buttonOpacity = 0.55f; self.owner.buttonSize = 1; self.owner.renderScale = 1;
    self.owner.joystickDeadzone = 0.18f; self.owner.controlMode = 2; self.owner.controlHeight = 0; self.owner.displayFPS = 60;
    [self.owner.customLayouts removeAllObjects]; [self.owner applyDisplaySettings];
    [self.owner updateControlColors]; [self.owner.view setNeedsLayout]; [self.tableView reloadData];
    th20_ios_log("settings restored defaults");
}
- (void)cheatChanged:(UITextField *)field {
    if ([[field.text lowercaseString] isEqualToString:@"ymgjdsh"]) [self submitCheat];
}
- (BOOL)textFieldShouldReturn:(UITextField *)field { [self submitCheat]; return YES; }
- (void)submitCheat {
    NSString *code = [[self.cheatField.text stringByTrimmingCharactersInSet:NSCharacterSet.whitespaceAndNewlineCharacterSet] lowercaseString];
    if (!code.length) return;
    int result = callbacks.cheat_code && self.owner.engineAvailable ? callbacks.cheat_code(callbacks.userdata, code.UTF8String) : -1;
    self.cheatResult.text = result == 1 ? @"全部内容已解锁并保存。" : result == 0 ? @"代码无效，请检查输入。" : result == -1 ? @"游戏尚未就绪，请进入主菜单后重试。" : @"保存失败，请导出日志。";
    self.cheatResult.textColor = result == 1 ? UIColor.systemGreenColor : UIColor.systemRedColor;
    if (result == 1) { self.cheatField.text = @""; [self.cheatField resignFirstResponder]; }
}
- (void)done {
    [self.view endEditing:YES]; __weak TH20ViewController *owner = self.owner;
    [self dismissViewControllerAnimated:YES completion:^{ owner.modalPaused = NO; [owner applyPausedState]; }];
}
@end

@interface TH20AppDelegate : UIResponder <UIApplicationDelegate>
@property(nonatomic, strong) UIWindow *window;
@property(nonatomic, strong) TH20ViewController *controller;
@end
@implementation TH20AppDelegate
- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)options {
    openLog(); application.idleTimerDisabled = YES;
    self.window = [[UIWindow alloc] initWithFrame:UIScreen.mainScreen.bounds];
    self.controller = [TH20ViewController new]; self.window.rootViewController = self.controller;
    [self.window makeKeyAndVisible]; return YES;
}
- (void)applicationWillResignActive:(UIApplication *)application { [self.controller setApplicationActive:NO]; }
- (void)applicationDidEnterBackground:(UIApplication *)application { th20_ios_log("lifecycle background"); th20_ios_flush_log(); }
- (void)applicationDidBecomeActive:(UIApplication *)application { [self.controller setApplicationActive:YES]; }
- (void)applicationWillTerminate:(UIApplication *)application { [self.controller stop]; }
@end

int th20_ios_run_app(int argc, char **argv, const TH20IOSCallbacks *registered) {
    if (!registered || registered->struct_size != sizeof(TH20IOSCallbacks)) {
        std::fprintf(stderr, "TH20 native host: callback ABI mismatch or missing engine.\n"); return 2;
    }
    callbacks = *registered;
    @autoreleasepool { return UIApplicationMain(argc, argv, nil, NSStringFromClass(TH20AppDelegate.class)); }
}
bool th20_ios_graphics_make_current(void) {
    if (![NSThread isMainThread]) { th20_ios_log("ERROR graphics context requested outside main thread"); return false; }
    TH20ViewController *controller = host;
    return controller && controller.appActive && [controller.gameView prepareDrawable];
}
bool th20_ios_graphics_present(unsigned source, int width, int height) {
    if (![NSThread isMainThread]) { th20_ios_log("ERROR graphics present outside main thread"); return false; }
    TH20ViewController *controller = host;
    if (!controller || !controller.appActive) return false;
    if (deferPresentation) {
        pendingFramebuffer = source; pendingWidth = width; pendingHeight = height;
        presentationPending = true; return true;
    }
    BOOL result = [controller.gameView presentFramebuffer:source width:width height:height];
    if (result) ++controller.presentedFrames;
    return result;
}
void th20_ios_log(const char *format, ...) {
    if (!format) return;
    char message[8192]; va_list args; va_start(args, format); std::vsnprintf(message, sizeof(message), format, args); va_end(args);
    std::lock_guard<std::mutex> guard(logMutex);
    double elapsed = logEpoch ? CACurrentMediaTime() - logEpoch : 0;
    if (logFile) std::fprintf(logFile, "[%10.3f] %s\n", elapsed, message);
    std::fprintf(stderr, "TH20 [%10.3f] %s\n", elapsed, message);
}
const char *th20_ios_log_path(void) { return logPath.c_str(); }
void th20_ios_flush_log(void) { std::lock_guard<std::mutex> guard(logMutex); if (logFile) std::fflush(logFile); }
void th20_ios_set_stage(const char *stage) {
    NSString *message = (stage ? [NSString stringWithUTF8String:stage] : nil) ?: @"未知阶段";
    th20_ios_log("stage %s", message.UTF8String); onMain(^{ [host showStage:message]; });
}
void th20_ios_set_error(const char *error) {
    NSString *message = (error ? [NSString stringWithUTF8String:error] : nil) ?: @"未提供错误详情";
    onMain(^{ [host showError:message]; });
}
void th20_ios_set_ready(bool ready) { onMain(^{ [host changeReady:ready]; }); }
void th20_ios_set_input_mode(TH20IOSInputMode mode) { onMain(^{ [host setMode:mode]; }); }
void th20_ios_set_combat_scene(bool active) { onMain(^{ [host setCombatPresentation:active]; }); }
void th20_ios_set_logical_size(int width, int height) {
    if (width <= 0 || height <= 0 || width > 8192 || height > 8192) { th20_ios_log("ERROR invalid logical size %dx%d", width, height); return; }
    onMain(^{ logicalWidth = width; logicalHeight = height; [host clearInput]; [host.view setNeedsLayout]; });
}
void th20_ios_clear_input(void) { onMain(^{ [host clearInput]; }); }

void th20_ios_open_settings(void) { onMain(^{ [host openSettings]; }); }
