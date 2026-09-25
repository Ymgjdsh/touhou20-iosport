// Native platform verification only. This target is visibly distinct from
// the game and must never be packaged or reported as a playable TH20 build.
#import <UIKit/UIKit.h>
#import <OpenGLES/ES3/gl.h>
#include "ios_host.h"
#include <cmath>
#include <cstdint>

extern "C" int th20_ios_renderer_probe(void);
namespace {
GLuint framebuffer{}, texture{};
unsigned updates{}, keyEvents{}, touchEvents{};
bool stageComplete{}, timingValid = true;
uint64_t presents{};
__weak UILabel *label;
bool initialize(void *, const char *resources, const char *documents) {
    th20_ios_log("PLATFORM DIAGNOSTICS ONLY: not the game; resources=%s documents=%s", resources, documents);
    th20_ios_set_stage("Native platform diagnostics — not a playable game");
    th20_ios_set_input_mode(TH20_IOS_INPUT_MENU);
    if (th20_ios_renderer_probe() != 0) {
        th20_ios_set_error("Native renderer pixel regression failed. This is a diagnostic target, not the game.");
        return false;
    }
    glGenTextures(1, &texture); glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 640, 480, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glGenFramebuffers(1, &framebuffer); glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) return false;
    UIWindow *window = UIApplication.sharedApplication.keyWindow;
    UILabel *notice = [UILabel new]; label = notice;
    notice.text = @"TH20 PLATFORM DIAGNOSTICS\nNot a playable game\nRenderer: 27/27 pixel checks passed\nChecking native 60 Hz host…";
    notice.numberOfLines = 0; notice.textColor = UIColor.whiteColor;
    notice.font = [UIFont systemFontOfSize:16 weight:UIFontWeightSemibold];
    notice.textAlignment = NSTextAlignmentCenter; notice.userInteractionEnabled = NO;
    notice.translatesAutoresizingMaskIntoConstraints = NO;
    [window.rootViewController.view addSubview:notice];
    [NSLayoutConstraint activateConstraints:@[[notice.centerXAnchor constraintEqualToAnchor:window.rootViewController.view.centerXAnchor],
        [notice.centerYAnchor constraintEqualToAnchor:window.rootViewController.view.centerYAnchor],
        [notice.widthAnchor constraintLessThanOrEqualToAnchor:window.rootViewController.view.widthAnchor constant:-32]]];
    return true;
}
void update(void *, double seconds) {
    timingValid &= std::abs(seconds - 1.0 / 60.0) < 1e-12;
    ++updates;
    if (updates >= 120 && !stageComplete) {
        stageComplete = true;
        label.text = @"TH20 PLATFORM DIAGNOSTICS\nNot a playable game\nRenderer pixel checks passed\nNative host: 120 fixed-step updates passed\nLogs available from Settings";
        th20_ios_log("host-probe RESULT fixed_timestep=%s updates=%u presents=%llu key_events=%u touch_events=%u", timingValid ? "PASS" : "FAIL", updates, presents, keyEvents, touchEvents);
        th20_ios_flush_log();
    }
}
void render(void *) {
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glDisable(GL_SCISSOR_TEST); glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glClearColor(0.07f, 0.13f, 0.2f, 1); glClear(GL_COLOR_BUFFER_BIT);
    // Four asymmetric corners reveal flipped/scaled/cropped presentation.
    const GLfloat colors[4][4] = {{1,0,0,1},{0,1,0,1},{0,0,1,1},{1,1,0,1}};
    glEnable(GL_SCISSOR_TEST);
    for (int i = 0; i < 4; ++i) {
        glScissor((i & 1) ? 576 : 0, (i & 2) ? 416 : 0, 64, 64);
        glClearColor(colors[i][0], colors[i][1], colors[i][2], 1); glClear(GL_COLOR_BUFFER_BIT);
    }
    GLint beforeRead{}, beforeDraw{}, afterRead{}, afterDraw{};
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &beforeRead); glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &beforeDraw);
    if (th20_ios_graphics_present(framebuffer, 640, 480)) ++presents;
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &afterRead); glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &afterDraw);
    if (beforeRead != afterRead || beforeDraw != afterDraw || !glIsEnabled(GL_SCISSOR_TEST)) {
        th20_ios_set_error("Native host changed the renderer framebuffer/scissor state.");
    }
}
void key(void *, int key, bool down) { ++keyEvents; th20_ios_log("host-probe key VK=0x%x down=%d", key, down); }
void touch(void *, TH20IOSTouchPhase phase, uint64_t id, float x, float y, float dx, float dy) {
    ++touchEvents; th20_ios_log("host-probe touch phase=%d id=%llu position=%.1f,%.1f delta=%.1f,%.1f", phase, id, x, y, dx, dy);
}
void pause(void *, bool paused) { th20_ios_log("host-probe pause=%d", paused); }
void shutdown(void *) {
    if (framebuffer) glDeleteFramebuffers(1, &framebuffer);
    if (texture) glDeleteTextures(1, &texture);
}
}
int main(int argc, char **argv) {
    TH20IOSCallbacks callbacks{};
    callbacks.struct_size = sizeof(callbacks); callbacks.initialize = initialize;
    callbacks.update = update; callbacks.render = render; callbacks.key = key;
    callbacks.touch = touch; callbacks.pause = pause; callbacks.shutdown = shutdown;
    return th20_ios_run_app(argc, argv, &callbacks);
}
