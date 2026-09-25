#import <ImageIO/ImageIO.h>
#import <CoreGraphics/CoreGraphics.h>
// Windows BOOL is always 32-bit, while Objective-C BOOL is platform-specific.
#undef TRUE
#undef FALSE
#define BOOL TH20_WINBOOL
#include <d3d9.h>
#undef BOOL
#include "ios_host.h"
#include <OpenGLES/ES3/gl.h>
#include <limits>
#include <set>
#include <string>
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstring>
#include <vector>

namespace {
// Native GLES 3 backend. No browser runtime or JavaScript is linked.
void graphics_error(const char* operation, unsigned value) {
    th20_ios_log("graphics: %s (%u / 0x%x)", operation, value, value);
}
HRESULT unsupported(const char* operation, unsigned value) {
    static std::set<std::pair<std::string, unsigned>> reported;
    if(reported.emplace(operation,value).second) graphics_error(operation,value);
    return E_NOTIMPL;
}
bool valid_rect(const RECT& rect, UINT width, UINT height) {
    return rect.left>=0 && rect.top>=0 && rect.right>rect.left && rect.bottom>rect.top &&
           static_cast<UINT>(rect.right)<=width && static_cast<UINT>(rect.bottom)<=height;
}
GLuint current_backbuffer{};
// All graphics calls run on UIKit's thread. Share one staging allocation;
// retaining a full-size buffer per texture doubles the game's texture memory.
std::vector<std::uint8_t> texture_upload_scratch;
template<class Derived> struct ComObject {
    std::atomic<ULONG> references{1};
    HRESULT query(void** output,Derived* self){if(!output)return E_POINTER;*output=self;self->AddRef();return S_OK;}
    ULONG add(){return ++references;}
    ULONG release(Derived* self){const auto count=--references;if(!count)delete self;return count;}
};

struct NativeTexture;
NativeTexture* current_backbuffer_texture{};
struct NativeSurface final:IDirect3DSurface9,ComObject<NativeSurface> {
    NativeTexture* owner{};UINT width{},height{};D3DFORMAT format{D3DFMT_A8R8G8B8};
    std::vector<std::uint8_t> standalone;RECT locked{};bool read_only{};
    NativeSurface(NativeTexture* texture,UINT w,UINT h,D3DFORMAT f);
    ~NativeSurface();
    std::uint8_t* data();int pitch()const;void upload(const RECT* rectangle=nullptr);GLuint framebuffer()const;
    HRESULT QueryInterface(REFIID,void** output)override{return query(output,this);}
    ULONG AddRef()override{return add();} ULONG Release()override{return release(this);}
    HRESULT GetDesc(D3DSURFACE_DESC* output)override;
    HRESULT LockRect(D3DLOCKED_RECT* output,const RECT* rectangle,DWORD)override;
    HRESULT UnlockRect()override{if(!read_only)upload(&locked);return S_OK;}
};

struct NativeTexture final:IDirect3DTexture9,ComObject<NativeTexture> {
    UINT width{},height{};D3DFORMAT format{};DWORD usage{};GLuint texture{},fbo{},depth_buffer{};
    std::vector<std::uint8_t> pixels;RECT locked{};bool gpu_dirty{},read_only{};
    NativeTexture(UINT w,UINT h,D3DFORMAT f,DWORD use):width(w),height(h),format(f),usage(use),pixels(std::size_t(w)*h*4) {
        GLint previous_texture{},previous_read{},previous_draw{},previous_renderbuffer{};
        glGetIntegerv(GL_TEXTURE_BINDING_2D,&previous_texture);glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,&previous_read);glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&previous_draw);glGetIntegerv(GL_RENDERBUFFER_BINDING,&previous_renderbuffer);
        glGenTextures(1,&texture);glBindTexture(GL_TEXTURE_2D,texture);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,width,height,0,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
        glGenFramebuffers(1,&fbo);glBindFramebuffer(GL_FRAMEBUFFER,fbo);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,texture,0);
        if(usage&1){glGenRenderbuffers(1,&depth_buffer);glBindRenderbuffer(GL_RENDERBUFFER,depth_buffer);glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH_COMPONENT24,width,height);glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,depth_buffer);}
        glBindFramebuffer(GL_READ_FRAMEBUFFER,previous_read);glBindFramebuffer(GL_DRAW_FRAMEBUFFER,previous_draw);glBindRenderbuffer(GL_RENDERBUFFER,previous_renderbuffer);glBindTexture(GL_TEXTURE_2D,previous_texture);
    }
    ~NativeTexture(){if(depth_buffer)glDeleteRenderbuffers(1,&depth_buffer);if(fbo)glDeleteFramebuffers(1,&fbo);if(texture)glDeleteTextures(1,&texture);}
    void readback(){
        if(!gpu_dirty)return;
        GLint previous{};glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,&previous);glBindFramebuffer(GL_READ_FRAMEBUFFER,fbo);
        glReadPixels(0,0,width,height,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());glBindFramebuffer(GL_READ_FRAMEBUFFER,previous);
        for(std::size_t offset=0;offset+3<pixels.size();offset+=4)std::swap(pixels[offset],pixels[offset+2]);
        gpu_dirty=false;
    }
    void upload(const RECT* rectangle=nullptr){
        const RECT region=rectangle?*rectangle:RECT{0,0,LONG(width),LONG(height)};
        if(!valid_rect(region,width,height))return;
        const auto w=region.right-region.left,h=region.bottom-region.top;
        // Text is uploaded one line at a time into a large atlas. Convert and
        // transfer only the locked rectangle, retaining the reusable buffer.
        auto& upload_scratch=texture_upload_scratch;
        upload_scratch.resize(std::size_t(w)*h*4);
        for(LONG y=0;y<h;++y){
            const auto* input=pixels.data()+(std::size_t(y+region.top)*width+region.left)*4;
            auto* output=upload_scratch.data()+std::size_t(y)*w*4;
            for(LONG x=0;x<w;++x){output[4*x]=input[4*x+2];output[4*x+1]=input[4*x+1];output[4*x+2]=input[4*x];output[4*x+3]=input[4*x+3];}
        }
        GLint previous{},alignment{},row_length{},skip_rows{},skip_pixels{};
        glGetIntegerv(GL_TEXTURE_BINDING_2D,&previous);glGetIntegerv(GL_UNPACK_ALIGNMENT,&alignment);
        glGetIntegerv(GL_UNPACK_ROW_LENGTH,&row_length);glGetIntegerv(GL_UNPACK_SKIP_ROWS,&skip_rows);glGetIntegerv(GL_UNPACK_SKIP_PIXELS,&skip_pixels);
        glBindTexture(GL_TEXTURE_2D,texture);glPixelStorei(GL_UNPACK_ALIGNMENT,4);glPixelStorei(GL_UNPACK_ROW_LENGTH,0);glPixelStorei(GL_UNPACK_SKIP_ROWS,0);glPixelStorei(GL_UNPACK_SKIP_PIXELS,0);
        glTexSubImage2D(GL_TEXTURE_2D,0,region.left,region.top,w,h,GL_RGBA,GL_UNSIGNED_BYTE,upload_scratch.data());
        glPixelStorei(GL_UNPACK_ALIGNMENT,alignment);glPixelStorei(GL_UNPACK_ROW_LENGTH,row_length);glPixelStorei(GL_UNPACK_SKIP_ROWS,skip_rows);glPixelStorei(GL_UNPACK_SKIP_PIXELS,skip_pixels);glBindTexture(GL_TEXTURE_2D,previous);
        // A partial file upload can follow a GPU surface copy. In that case
        // the untouched CPU pixels are still stale and need a later readback.
        if(region.left==0&&region.top==0&&region.right==LONG(width)&&region.bottom==LONG(height))gpu_dirty=false;
    }
    HRESULT QueryInterface(REFIID,void** output)override{return query(output,this);}
    ULONG AddRef()override{return add();} ULONG Release()override{return release(this);}
    void PreLoad()override{/* Storage is resident immediately after CreateTexture/UnlockRect. */}
    HRESULT GetLevelDesc(UINT level,D3DSURFACE_DESC* output)override{if(level||!output)return E_INVALIDARG;*output={format,D3DRTYPE_TEXTURE,usage,D3DPOOL_MANAGED,D3DMULTISAMPLE_NONE,0,width,height};return S_OK;}
    HRESULT GetSurfaceLevel(UINT level,IDirect3DSurface9** output)override{if(level||!output)return E_INVALIDARG;*output=new NativeSurface(this,width,height,format);return S_OK;}
    HRESULT LockRect(UINT level,D3DLOCKED_RECT* output,const RECT* rectangle,DWORD flags)override{
        if(level||!output)return E_INVALIDARG;locked=rectangle?*rectangle:RECT{0,0,LONG(width),LONG(height)};
        if(!valid_rect(locked,width,height))return E_INVALIDARG;
        readback();read_only=(flags&D3DLOCK_READONLY)!=0;output->Pitch=width*4;output->pBits=pixels.data()+std::size_t(locked.top)*width*4+locked.left*4;return S_OK;
    }
    HRESULT UnlockRect(UINT level)override{if(level)return E_INVALIDARG;if(!read_only)upload(&locked);return S_OK;}
    void AddDirtyRect(const RECT*)override{
        // D3D uses this to invalidate its managed GPU copy. Ours is updated
        // immediately by UnlockRect and surface copies, so it is already
        // current. Uploading CPU memory here would undo a GPU surface copy.
    }
};

NativeSurface::NativeSurface(NativeTexture* texture,UINT w,UINT h,D3DFORMAT f):owner(texture),width(w),height(h),format(f){if(owner)owner->AddRef();else standalone.resize(std::size_t(w)*h*4);}
NativeSurface::~NativeSurface(){if(owner)owner->Release();}
std::uint8_t* NativeSurface::data(){return owner?owner->pixels.data():standalone.data();}
int NativeSurface::pitch()const{return int(width*4);}
void NativeSurface::upload(const RECT* rectangle){
    if(owner){owner->upload(rectangle);return;}
    if(!current_backbuffer_texture)return;
    const RECT region=rectangle?*rectangle:RECT{0,0,LONG(width),LONG(height)};
    for(LONG y=region.top;y<region.bottom;++y)std::memcpy(current_backbuffer_texture->pixels.data()+std::size_t(height-1-y)*pitch()+region.left*4,standalone.data()+std::size_t(y)*pitch()+region.left*4,(region.right-region.left)*4);
    const RECT flipped{region.left,LONG(height)-region.bottom,region.right,LONG(height)-region.top};
    current_backbuffer_texture->upload(&flipped);
}
GLuint NativeSurface::framebuffer()const{return owner?owner->fbo:current_backbuffer;}
HRESULT NativeSurface::GetDesc(D3DSURFACE_DESC* output){if(!output)return E_POINTER;*output={format,D3DRTYPE_SURFACE,owner?owner->usage:0,D3DPOOL_DEFAULT,D3DMULTISAMPLE_NONE,0,width,height};return S_OK;}
HRESULT NativeSurface::LockRect(D3DLOCKED_RECT* output,const RECT* rectangle,DWORD flags){
    if(!output)return E_POINTER;locked=rectangle?*rectangle:RECT{0,0,LONG(width),LONG(height)};
    if(!valid_rect(locked,width,height))return E_INVALIDARG;
    read_only=(flags&D3DLOCK_READONLY)!=0;
    if(owner)owner->readback();
    else if(current_backbuffer_texture){
        current_backbuffer_texture->readback();
        for(UINT y=0;y<height;++y)std::memcpy(standalone.data()+std::size_t(y)*pitch(),current_backbuffer_texture->pixels.data()+std::size_t(height-1-y)*pitch(),pitch());
    }
    output->Pitch=pitch();output->pBits=data()+std::size_t(locked.top)*pitch()+locked.left*4;return S_OK;
}

struct NativeVertexBuffer final:IDirect3DVertexBuffer9,ComObject<NativeVertexBuffer> {
    std::vector<std::uint8_t> bytes;explicit NativeVertexBuffer(UINT size):bytes(size){}
    HRESULT QueryInterface(REFIID,void** output)override{return query(output,this);}
    ULONG AddRef()override{return add();} ULONG Release()override{return release(this);}
    HRESULT Lock(UINT offset,UINT size,void** output,DWORD)override{if(!output||offset>bytes.size())return E_INVALIDARG;if(!size)size=bytes.size()-offset;if(size>bytes.size()-offset)return E_INVALIDARG;*output=bytes.data()+offset;return S_OK;}
    HRESULT Unlock()override{return S_OK;}
};

GLuint compile_shader(GLenum type,const char* source){
    const auto shader=glCreateShader(type);glShaderSource(shader,1,&source,nullptr);glCompileShader(shader);
    GLint status{};glGetShaderiv(shader,GL_COMPILE_STATUS,&status);
    if(!status){GLint length{};glGetShaderiv(shader,GL_INFO_LOG_LENGTH,&length);std::vector<char> message(std::max(length,1));glGetShaderInfoLog(shader,message.size(),nullptr,message.data());th20_ios_log("graphics: shader compile failed type=%u: %s",type,message.data());glDeleteShader(shader);return 0;}
    return shader;
}

struct NativeDevice final:IDirect3DDevice9,ComObject<NativeDevice> {
    bool context_ready{};NativeTexture* backbuffer{};D3DPRESENT_PARAMETERS presentation{};
    GLuint program{},vao{},vbo{};GLint texture_location{},use_texture_location{},texture_factor_location{};
    GLint color_op_location{},color_arg1_location{},color_arg2_location{};
    GLint alpha_op_location{},alpha_arg1_location{},alpha_arg2_location{};
    GLint alpha_test_location{},alpha_ref_location{},alpha_func_location{};
    GLint fog_enabled_location{},fog_color_location{},fog_table_mode_location{},fog_parameters_location{};
    D3DMATRIX world{},view{},projection{},texture_matrix{};D3DVIEWPORT9 viewport{};DWORD fvf{};
    NativeTexture* bound_texture{};NativeVertexBuffer* stream{};UINT stream_offset{},stream_stride{};NativeSurface* render_target{};
    DWORD texture_factor{0xffffffffU},src_blend{5},dst_blend{6};
    DWORD src_blend_alpha{2},dst_blend_alpha{1},blend_op{1},blend_op_alpha{1},texture_transform_flags{};
    DWORD alpha_ref{},alpha_func{8};bool separate_alpha{},alpha_test{};
    bool fog_enabled{},range_fog{};DWORD fog_color{},fog_vertex_mode{},fog_table_mode{};
    float fog_start{},fog_end{1.f},fog_density{1.f};
    GLenum wrap_s{GL_REPEAT},wrap_t{GL_REPEAT},min_filter{GL_NEAREST},mag_filter{GL_NEAREST};
    DWORD color_op{D3DTOP_MODULATE},color_arg1{D3DTA_TEXTURE},color_arg2{D3DTA_DIFFUSE};
    DWORD alpha_op{D3DTOP_MODULATE},alpha_arg1{D3DTA_TEXTURE},alpha_arg2{D3DTA_DIFFUSE};
    bool alpha_blend=true,depth_write=true;
    unsigned frame_draws{};float frame_min_x{1e30f},frame_max_x{-1e30f},frame_min_y{1e30f},frame_max_y{-1e30f},frame_max_alpha{};int program_linked{};
    struct Vertex{float x,y,z,w;float r,g,b,a;float u,v;float fog{1.f};};
    std::vector<Vertex> transformed_vertices;
    bool resize_backbuffer(){
        const UINT width=presentation.BackBufferWidth?presentation.BackBufferWidth:640;
        const UINT height=presentation.BackBufferHeight?presentation.BackBufferHeight:480;
        if(render_target){render_target->Release();render_target=nullptr;}
        if(backbuffer)backbuffer->Release();
        backbuffer=new NativeTexture(width,height,D3DFMT_A8R8G8B8,D3DUSAGE_RENDERTARGET);
        current_backbuffer=backbuffer->fbo;current_backbuffer_texture=backbuffer;
        glBindFramebuffer(GL_FRAMEBUFFER,current_backbuffer);
        const auto status=glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if(status!=GL_FRAMEBUFFER_COMPLETE){graphics_error("backbuffer incomplete",status);return false;}
        th20_ios_log("graphics: native backbuffer %u x %u",width,height);
        return true;
    }
    explicit NativeDevice(D3DPRESENT_PARAMETERS* parameters){
        if(parameters)presentation=*parameters;
        context_ready=th20_ios_graphics_make_current();
        if(!context_ready){th20_ios_log("graphics: EAGL context unavailable");return;}
        if(!resize_backbuffer()){context_ready=false;return;}
        th20_ios_log("graphics: GLES vendor=%s renderer=%s version=%s",glGetString(GL_VENDOR),glGetString(GL_RENDERER),glGetString(GL_VERSION));
        static constexpr char vertex_source[]=R"(#version 300 es
layout(location=0) in vec4 a_position;layout(location=1) in vec4 a_color;layout(location=2) in vec2 a_uv;layout(location=3) in float a_fog;
out vec4 v_color;out vec2 v_uv;out float v_fog;void main(){gl_Position=a_position;gl_PointSize=1.0;v_color=a_color;v_uv=a_uv;v_fog=a_fog;})";
        static constexpr char fragment_source[]=R"(#version 300 es
precision highp float;
in vec4 v_color;in vec2 v_uv;in float v_fog;
uniform sampler2D u_texture;uniform bool u_use_texture;uniform vec4 u_texture_factor;
uniform int u_color_op;uniform int u_color_arg1;uniform int u_color_arg2;
uniform int u_alpha_op;uniform int u_alpha_arg1;uniform int u_alpha_arg2;
uniform bool u_alpha_test;uniform float u_alpha_ref;uniform int u_alpha_func;
uniform bool u_fog_enabled;uniform vec3 u_fog_color;uniform int u_fog_table_mode;uniform vec3 u_fog_parameters;
out vec4 output_color;
vec4 argument_value(int argument,vec4 texture_color){
    int source=argument&15;vec4 value=v_color;
    if(source==2)value=texture_color;else if(source==3)value=u_texture_factor;
    if((argument&32)!=0)value=vec4(value.a);
    if((argument&16)!=0)value=vec4(1.0)-value;
    return value;
}
vec4 combine_value(int operation,vec4 first,vec4 second){
    if(operation==1)return v_color;
    if(operation==2)return first;
    if(operation==3)return second;
    if(operation==5)return 2.0*first*second;
    if(operation==6)return 4.0*first*second;
    if(operation==7)return first+second;
    if(operation==8)return first+second-0.5;
    if(operation==10)return first-second;
    return first*second;
}
void main(){
    vec4 texture_color=u_use_texture?texture(u_texture,v_uv):vec4(1.0);
    vec4 color_result=combine_value(u_color_op,argument_value(u_color_arg1,texture_color),argument_value(u_color_arg2,texture_color));
    vec4 alpha_result=combine_value(u_alpha_op,argument_value(u_alpha_arg1,texture_color),argument_value(u_alpha_arg2,texture_color));
    vec4 color=clamp(vec4(color_result.rgb,alpha_result.a),0.0,1.0);
    if(u_alpha_test){
        float a=floor(color.a*255.0+0.5),b=u_alpha_ref;
        bool pass=u_alpha_func==8||(u_alpha_func==2&&a<b)||(u_alpha_func==3&&a==b)||(u_alpha_func==4&&a<=b)||(u_alpha_func==5&&a>b)||(u_alpha_func==6&&a!=b)||(u_alpha_func==7&&a>=b);
        if(!pass)discard;
    }
    if(u_fog_enabled){
        float factor=v_fog;
        if(u_fog_table_mode!=0){
            float distance=gl_FragCoord.z;
            if(u_fog_table_mode==1)factor=exp(-u_fog_parameters.z*distance);
            else if(u_fog_table_mode==2){float d=u_fog_parameters.z*distance;factor=exp(-d*d);}
            else if(u_fog_table_mode==3)factor=(u_fog_parameters.y-distance)/(u_fog_parameters.y-u_fog_parameters.x);
        }
        color.rgb=mix(u_fog_color,color.rgb,clamp(factor,0.0,1.0));
    }
    output_color=color;
})";
        const auto vs=compile_shader(GL_VERTEX_SHADER,vertex_source),fs=compile_shader(GL_FRAGMENT_SHADER,fragment_source);program=glCreateProgram();glAttachShader(program,vs);glAttachShader(program,fs);glLinkProgram(program);glDeleteShader(vs);glDeleteShader(fs);
        glGetProgramiv(program,GL_LINK_STATUS,&program_linked);
        if(!program_linked){GLint length{};glGetProgramiv(program,GL_INFO_LOG_LENGTH,&length);std::vector<char> message(std::max(length,1));glGetProgramInfoLog(program,message.size(),nullptr,message.data());th20_ios_log("graphics: shader link failed: %s",message.data());return;}
        texture_location=glGetUniformLocation(program,"u_texture");use_texture_location=glGetUniformLocation(program,"u_use_texture");texture_factor_location=glGetUniformLocation(program,"u_texture_factor");
        color_op_location=glGetUniformLocation(program,"u_color_op");color_arg1_location=glGetUniformLocation(program,"u_color_arg1");color_arg2_location=glGetUniformLocation(program,"u_color_arg2");
        alpha_op_location=glGetUniformLocation(program,"u_alpha_op");alpha_arg1_location=glGetUniformLocation(program,"u_alpha_arg1");alpha_arg2_location=glGetUniformLocation(program,"u_alpha_arg2");
        alpha_test_location=glGetUniformLocation(program,"u_alpha_test");alpha_ref_location=glGetUniformLocation(program,"u_alpha_ref");alpha_func_location=glGetUniformLocation(program,"u_alpha_func");
        fog_enabled_location=glGetUniformLocation(program,"u_fog_enabled");fog_color_location=glGetUniformLocation(program,"u_fog_color");fog_table_mode_location=glGetUniformLocation(program,"u_fog_table_mode");fog_parameters_location=glGetUniformLocation(program,"u_fog_parameters");
        glGenVertexArrays(1,&vao);glGenBuffers(1,&vbo);glBindVertexArray(vao);glBindBuffer(GL_ARRAY_BUFFER,vbo);
        glEnableVertexAttribArray(0);glVertexAttribPointer(0,4,GL_FLOAT,GL_FALSE,sizeof(Vertex),reinterpret_cast<void*>(offsetof(Vertex,x)));
        glEnableVertexAttribArray(1);glVertexAttribPointer(1,4,GL_FLOAT,GL_FALSE,sizeof(Vertex),reinterpret_cast<void*>(offsetof(Vertex,r)));
        glEnableVertexAttribArray(2);glVertexAttribPointer(2,2,GL_FLOAT,GL_FALSE,sizeof(Vertex),reinterpret_cast<void*>(offsetof(Vertex,u)));
        glEnableVertexAttribArray(3);glVertexAttribPointer(3,1,GL_FLOAT,GL_FALSE,sizeof(Vertex),reinterpret_cast<void*>(offsetof(Vertex,fog)));
        for(int row=0;row<4;++row){world.m[row][row]=view.m[row][row]=projection.m[row][row]=texture_matrix.m[row][row]=1;}
        viewport={0,0,presentation.BackBufferWidth?presentation.BackBufferWidth:640,presentation.BackBufferHeight?presentation.BackBufferHeight:480,0,1};
        glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);glEnable(GL_DEPTH_TEST);glDepthFunc(GL_LEQUAL);
        SetViewport(&viewport);
    }
    ~NativeDevice(){
        // Release current bindings before their objects. Besides avoiding stale
        // state, this works around an iOS simulator software-driver crash when
        // a host blits after an active shader program has been delete-marked.
        GLint bound{};glGetIntegerv(GL_CURRENT_PROGRAM,&bound);if(GLuint(bound)==program)glUseProgram(0);
        glGetIntegerv(GL_VERTEX_ARRAY_BINDING,&bound);if(GLuint(bound)==vao)glBindVertexArray(0);
        glGetIntegerv(GL_ARRAY_BUFFER_BINDING,&bound);if(GLuint(bound)==vbo)glBindBuffer(GL_ARRAY_BUFFER,0);
        glBindTexture(GL_TEXTURE_2D,0);glBindFramebuffer(GL_FRAMEBUFFER,0);glBindRenderbuffer(GL_RENDERBUFFER,0);
        if(render_target)render_target->Release();if(stream)stream->Release();if(bound_texture)bound_texture->Release();
        if(vbo)glDeleteBuffers(1,&vbo);if(vao)glDeleteVertexArrays(1,&vao);if(program)glDeleteProgram(program);
        if(backbuffer)backbuffer->Release();current_backbuffer_texture=nullptr;current_backbuffer=0;
    }
    HRESULT QueryInterface(REFIID,void** output)override{return query(output,this);}ULONG AddRef()override{return add();}ULONG Release()override{return release(this);}
    HRESULT TestCooperativeLevel()override{return context_ready?S_OK:E_FAIL;}UINT GetAvailableTextureMem()override{return 0; /* iOS uses unified memory and exposes no available texture-memory query. */}HRESULT EvictManagedResources()override{/* The native backend has no separate managed-resource cache to evict. */return S_OK;}
    HRESULT GetDeviceCaps(D3DCAPS9* caps)override{if(!caps)return E_POINTER;std::memset(caps,0,sizeof(*caps));GLint limit{};glGetIntegerv(GL_MAX_TEXTURE_SIZE,&limit);caps->MaxTextureWidth=caps->MaxTextureHeight=limit;caps->TextureOpCaps=0x2ff;return S_OK;}
    HRESULT GetRasterStatus(UINT,D3DRASTER_STATUS* status)override{if(!status)return E_POINTER;*status={};return unsupported("raster scanline query; pacing uses CADisplayLink",0);}
    HRESULT Reset(D3DPRESENT_PARAMETERS* parameters)override{if(parameters)presentation=*parameters;if(!resize_backbuffer())return E_FAIL;const D3DVIEWPORT9 full{0,0,presentation.BackBufferWidth?presentation.BackBufferWidth:640,presentation.BackBufferHeight?presentation.BackBufferHeight:480,0,1};return SetViewport(&full);}
    HRESULT Present(const RECT*,const RECT*,HWND,const void*)override{
        // The host makes only its drawable opaque. Preserve game-backbuffer
        // alpha across Present: subsequent surface copies can still use it.
        GLint framebuffer{},saved_viewport[4],saved_scissor[4];GLfloat clear_color[4];GLboolean color_mask[4];
        glGetIntegerv(GL_FRAMEBUFFER_BINDING,&framebuffer);glGetIntegerv(GL_VIEWPORT,saved_viewport);glGetIntegerv(GL_SCISSOR_BOX,saved_scissor);glGetFloatv(GL_COLOR_CLEAR_VALUE,clear_color);glGetBooleanv(GL_COLOR_WRITEMASK,color_mask);
        const bool scissor=glIsEnabled(GL_SCISSOR_TEST);
        static unsigned presents=0;
        const GLenum error=glGetError();if(error!=GL_NO_ERROR)graphics_error("frame error",error);
        if(presents<3 || presents%300==0)th20_ios_log("graphics: frame=%u draws=%u linked=%d bounds=(%.3f,%.3f)-(%.3f,%.3f)",presents,frame_draws,program_linked,frame_min_x,frame_min_y,frame_max_x,frame_max_y);
        ++presents;
        const bool presented=th20_ios_graphics_present(current_backbuffer,backbuffer->width,backbuffer->height);
        glBindFramebuffer(GL_FRAMEBUFFER,framebuffer);glViewport(saved_viewport[0],saved_viewport[1],saved_viewport[2],saved_viewport[3]);
        glScissor(saved_scissor[0],saved_scissor[1],saved_scissor[2],saved_scissor[3]);if(scissor)glEnable(GL_SCISSOR_TEST);else glDisable(GL_SCISSOR_TEST);
        glClearColor(clear_color[0],clear_color[1],clear_color[2],clear_color[3]);glColorMask(color_mask[0],color_mask[1],color_mask[2],color_mask[3]);glDepthMask(depth_write);
        if(!presented)return E_FAIL;
        frame_draws=0;frame_min_x=frame_min_y=1e30f;frame_max_x=frame_max_y=-1e30f;frame_max_alpha=0;return S_OK;
    }
    HRESULT GetBackBuffer(UINT,UINT,D3DBACKBUFFER_TYPE,IDirect3DSurface9** output)override{if(!output)return E_POINTER;*output=new NativeSurface(nullptr,presentation.BackBufferWidth?presentation.BackBufferWidth:640,presentation.BackBufferHeight?presentation.BackBufferHeight:480,presentation.BackBufferFormat);return S_OK;}
    HRESULT CreateTexture(UINT width,UINT height,UINT levels,DWORD usage,D3DFORMAT format,D3DPOOL,IDirect3DTexture9** output,HANDLE*)override{
        if(!output)return E_POINTER;*output=nullptr;GLint limit{};glGetIntegerv(GL_MAX_TEXTURE_SIZE,&limit);
        if(!width||!height||width>UINT(limit)||height>UINT(limit))return E_INVALIDARG;
        if(levels>1)return unsupported("mip levels",levels);
        if(format!=D3DFMT_A8R8G8B8&&format!=D3DFMT_X8R8G8B8&&format!=D3DFMT_UNKNOWN)return unsupported("texture format",format);
        auto* texture=new NativeTexture(width,height,format==D3DFMT_UNKNOWN?D3DFMT_A8R8G8B8:format,usage);
        const GLenum error=glGetError();if(error!=GL_NO_ERROR){graphics_error("texture allocation",error);delete texture;return E_FAIL;}
        *output=texture;return S_OK;
    }
    HRESULT CreateVertexBuffer(UINT length,DWORD,DWORD,D3DPOOL,IDirect3DVertexBuffer9** output,HANDLE*)override{if(!output)return E_POINTER;*output=new NativeVertexBuffer(length);return S_OK;}
    bool offscreen()const{return render_target&&render_target->owner;}
    UINT target_height()const{return render_target?render_target->height:(presentation.BackBufferHeight?presentation.BackBufferHeight:480);}
    HRESULT SetRenderTarget(DWORD index,IDirect3DSurface9* surface)override{
        if(index)return E_INVALIDARG;auto* next=dynamic_cast<NativeSurface*>(surface);if(surface&&!next)return E_INVALIDARG;if(next)next->AddRef();if(render_target)render_target->Release();render_target=next;
        glBindFramebuffer(GL_FRAMEBUFFER,render_target?render_target->framebuffer():current_backbuffer);
        // D3D9 resets the viewport whenever render target 0 changes.
        const D3DVIEWPORT9 full{0,0,render_target?render_target->width:(presentation.BackBufferWidth?presentation.BackBufferWidth:640),target_height(),0,1};return SetViewport(&full);
    }
    void mark_target_dirty(){if(offscreen())render_target->owner->gpu_dirty=true;else if(backbuffer)backbuffer->gpu_dirty=true;}
    HRESULT BeginScene()override{return S_OK;}HRESULT EndScene()override{return S_OK;}
    HRESULT Clear(DWORD count,const D3DRECT* rectangles,DWORD flags,D3DCOLOR color,float depth,DWORD)override{
        mark_target_dirty();GLbitfield mask=0;if(flags&D3DCLEAR_TARGET){glClearColor(((color>>16)&255)/255.f,((color>>8)&255)/255.f,(color&255)/255.f,((color>>24)&255)/255.f);mask|=GL_COLOR_BUFFER_BIT;}if(flags&D3DCLEAR_ZBUFFER){glClearDepthf(depth);glDepthMask(GL_TRUE);mask|=GL_DEPTH_BUFFER_BIT;}
        const bool scissor=glIsEnabled(GL_SCISSOR_TEST);GLint previous[4];glGetIntegerv(GL_SCISSOR_BOX,previous);glEnable(GL_SCISSOR_TEST);
        const auto clear_rectangle=[&](LONG x1,LONG y1,LONG x2,LONG y2){
            x1=std::max<LONG>(x1,viewport.X);y1=std::max<LONG>(y1,viewport.Y);x2=std::min<LONG>(x2,viewport.X+viewport.Width);y2=std::min<LONG>(y2,viewport.Y+viewport.Height);
            if(x2>x1&&y2>y1){glScissor(x1,offscreen()?y1:LONG(target_height())-y2,x2-x1,y2-y1);glClear(mask);}
        };
        if(count&&rectangles)for(DWORD i=0;i<count;++i)clear_rectangle(rectangles[i].x1,rectangles[i].y1,rectangles[i].x2,rectangles[i].y2);
        else clear_rectangle(viewport.X,viewport.Y,viewport.X+viewport.Width,viewport.Y+viewport.Height);
        glScissor(previous[0],previous[1],previous[2],previous[3]);if(!scissor)glDisable(GL_SCISSOR_TEST);glDepthMask(depth_write);return S_OK;
    }
    HRESULT SetTransform(D3DTRANSFORMSTATETYPE state,const D3DMATRIX* matrix)override{if(!matrix)return E_POINTER;if(state==D3DTS_WORLD)world=*matrix;else if(state==D3DTS_VIEW)view=*matrix;else if(state==D3DTS_PROJECTION)projection=*matrix;else if(state==D3DTS_TEXTURE0)texture_matrix=*matrix;else return unsupported("transform state",state);return S_OK;}
    HRESULT SetViewport(const D3DVIEWPORT9* value)override{if(!value)return E_POINTER;if(!value->Width||!value->Height)return E_INVALIDARG;viewport=*value;glViewport(value->X,offscreen()?value->Y:target_height()-(value->Y+value->Height),value->Width,value->Height);glDepthRangef(value->MinZ,value->MaxZ);return S_OK;}
    static GLenum blend(DWORD value){switch(value){case 1:return GL_ZERO;case 2:return GL_ONE;case 3:return GL_SRC_COLOR;case 4:return GL_ONE_MINUS_SRC_COLOR;case 5:return GL_SRC_ALPHA;case 6:return GL_ONE_MINUS_SRC_ALPHA;case 7:return GL_DST_ALPHA;case 8:return GL_ONE_MINUS_DST_ALPHA;case 9:return GL_DST_COLOR;case 10:return GL_ONE_MINUS_DST_COLOR;default:return GL_ONE;}}
    static GLenum equation(DWORD value){switch(value){case 2:return GL_FUNC_SUBTRACT;case 3:return GL_FUNC_REVERSE_SUBTRACT;case 4:return GL_MIN;case 5:return GL_MAX;default:return GL_FUNC_ADD;}}
    void apply_blend(){glBlendFuncSeparate(blend(src_blend),blend(dst_blend),blend(separate_alpha?src_blend_alpha:src_blend),blend(separate_alpha?dst_blend_alpha:dst_blend));glBlendEquationSeparate(equation(blend_op),equation(separate_alpha?blend_op_alpha:blend_op));}
    HRESULT SetRenderState(D3DRENDERSTATETYPE state,DWORD value)override{
        if(state==7){if(value)glEnable(GL_DEPTH_TEST);else glDisable(GL_DEPTH_TEST);}
        else if(state==D3DRS_ZWRITEENABLE){depth_write=value!=0;glDepthMask(depth_write);}
        else if(state==23){static const GLenum functions[]{GL_NEVER,GL_NEVER,GL_LESS,GL_EQUAL,GL_LEQUAL,GL_GREATER,GL_NOTEQUAL,GL_GEQUAL,GL_ALWAYS};if(value<=8)glDepthFunc(functions[value]);}
        else if(state==27){alpha_blend=value!=0;if(alpha_blend)glEnable(GL_BLEND);else glDisable(GL_BLEND);}
        else if(state==D3DRS_SRCBLEND){src_blend=value;apply_blend();}else if(state==D3DRS_DESTBLEND){dst_blend=value;apply_blend();}
        else if(state==171){blend_op=value;apply_blend();}else if(state==206){separate_alpha=value!=0;apply_blend();}
        else if(state==207){src_blend_alpha=value;apply_blend();}else if(state==208){dst_blend_alpha=value;apply_blend();}else if(state==209){blend_op_alpha=value;apply_blend();}
        else if(state==15)alpha_test=value!=0;else if(state==24)alpha_ref=value&255;else if(state==25)alpha_func=value;
        else if(state==28)fog_enabled=value!=0;else if(state==34)fog_color=value;else if(state==35)fog_table_mode=value;
        else if(state==36)std::memcpy(&fog_start,&value,4);else if(state==37)std::memcpy(&fog_end,&value,4);else if(state==38)std::memcpy(&fog_density,&value,4);
        else if(state==140)fog_vertex_mode=value;else if(state==48)range_fog=value!=0;
        else if(state==D3DRS_TEXTUREFACTOR)texture_factor=value;
        else if(state==22){if(value==1)glDisable(GL_CULL_FACE);else return unsupported("cull mode",value);}
        else if(state==137&&value==0){/* All TH20 vertices are prelit. */}
        else if(state==9&&value==2){/* GLES interpolates Gouraud vertex colors. */}
        else if(state==161&&value==0){/* The native target is single-sampled. */}
        else return unsupported("render state",state);
        return S_OK;
    }
    HRESULT SetTexture(DWORD stage,IDirect3DBaseTexture9* texture)override{if(stage)return E_INVALIDARG;auto* next=dynamic_cast<NativeTexture*>(texture);if(texture&&!next)return E_INVALIDARG;if(next)next->AddRef();if(bound_texture)bound_texture->Release();bound_texture=next;return S_OK;}
    HRESULT SetTextureStageState(DWORD stage,D3DTEXTURESTAGESTATETYPE state,DWORD value)override{
        if(stage)return unsupported("texture stage",stage);
        if((state==D3DTSS_COLOROP||state==D3DTSS_ALPHAOP)&&!((value>=1&&value<=8)||value==10))return unsupported("texture combine operation",value);
        if(state==D3DTSS_COLOROP)color_op=value;else if(state==D3DTSS_COLORARG1)color_arg1=value;else if(state==D3DTSS_COLORARG2)color_arg2=value;
        else if(state==D3DTSS_ALPHAOP)alpha_op=value;else if(state==D3DTSS_ALPHAARG1)alpha_arg1=value;else if(state==D3DTSS_ALPHAARG2)alpha_arg2=value;
        else if(state==24)texture_transform_flags=value;
        else if(state==11&&value==0){/* The only supplied UV set is index zero. */}
        else return unsupported("texture stage state",state);
        return S_OK;
    }
    HRESULT SetSamplerState(DWORD stage,D3DSAMPLERSTATETYPE state,DWORD value)override{
        if(stage)return unsupported("sampler stage",stage);
        const auto wrap=value==1?GL_REPEAT:(value==2?GL_MIRRORED_REPEAT:GL_CLAMP_TO_EDGE);
        if(state==1||state==2){if(value<1||value>3)return unsupported("address mode",value);if(state==1)wrap_s=wrap;else wrap_t=wrap;}
        else if(state==5||state==6){if(value!=1&&value!=2)return unsupported("texture filter",value);if(state==5)mag_filter=value==1?GL_NEAREST:GL_LINEAR;else min_filter=value==1?GL_NEAREST:GL_LINEAR;}
        else if(state==3){/* W addressing cannot affect a 2D texture. */}
        else if(state==7&&value==0){/* Mipmapping is disabled for one-level textures. */}
        else return unsupported("sampler state",state);
        return S_OK;
    }
    HRESULT SetFVF(DWORD value)override{fvf=value;return S_OK;}HRESULT SetVertexShader(void* shader)override{return shader?unsupported("custom vertex shader",1):S_OK;}
    HRESULT SetStreamSource(UINT stream_number,IDirect3DVertexBuffer9* buffer,UINT offset,UINT stride)override{if(stream_number)return E_INVALIDARG;auto* next=dynamic_cast<NativeVertexBuffer*>(buffer);if(buffer&&!next)return E_INVALIDARG;if(next)next->AddRef();if(stream)stream->Release();stream=next;stream_offset=offset;stream_stride=stride;return S_OK;}
    static unsigned vertex_count(D3DPRIMITIVETYPE type,UINT primitives){if(type==D3DPT_POINTLIST)return primitives;if(type==D3DPT_LINELIST)return primitives*2;if(type==D3DPT_LINESTRIP)return primitives+1;if(type==D3DPT_TRIANGLELIST)return primitives*3;return primitives+2;}
    static GLenum topology(D3DPRIMITIVETYPE type){switch(type){case D3DPT_POINTLIST:return GL_POINTS;case D3DPT_LINELIST:return GL_LINES;case D3DPT_LINESTRIP:return GL_LINE_STRIP;case D3DPT_TRIANGLELIST:return GL_TRIANGLES;case D3DPT_TRIANGLEFAN:return GL_TRIANGLE_FAN;default:return GL_TRIANGLE_STRIP;}}
    static bool uses_texture(DWORD operation,DWORD first,DWORD second){
        if(operation==1)return false;
        return (operation!=3&&(first&15)==2)||(operation!=2&&(second&15)==2);
    }
    void transform(Vertex& output,const std::uint8_t* input,UINT stride){
        const bool rhw=(fvf&D3DFVF_XYZRHW)!=0;const auto* coordinates=reinterpret_cast<const float*>(input);
        float fog_distance=0.f;output.fog=1.f;
        if(rhw){output.w=coordinates[3]!=0.f?1.f/coordinates[3]:1.f;output.x=((coordinates[0]+.5f-viewport.X)/viewport.Width*2.f-1.f)*output.w;output.y=(1.f-(coordinates[1]+.5f-viewport.Y)/viewport.Height*2.f)*output.w;output.z=(coordinates[2]*2.f-1.f)*output.w;}
        else {float vector[4]{coordinates[0],coordinates[1],coordinates[2],1};auto apply=[](const float* v,const D3DMATRIX& m,float* o){for(int column=0;column<4;++column)o[column]=v[0]*m.m[0][column]+v[1]*m.m[1][column]+v[2]*m.m[2][column]+v[3]*m.m[3][column];};float a[4],b[4],c[4];apply(vector,world,a);apply(a,view,b);apply(b,projection,c);output={c[0],c[1],2*c[2]-c[3],c[3],1,1,1,1,0,0};fog_distance=range_fog?std::sqrt(b[0]*b[0]+b[1]*b[1]+b[2]*b[2]):std::abs(b[2]);}
        unsigned offset=rhw?16:12;if(fvf&D3DFVF_DIFFUSE){std::uint32_t color;std::memcpy(&color,input+offset,4);output.a=((color>>24)&255)/255.f;output.r=((color>>16)&255)/255.f;output.g=((color>>8)&255)/255.f;output.b=(color&255)/255.f;offset+=4;}else output.r=output.g=output.b=output.a=1;
        if(fvf&0x80){std::uint32_t specular;std::memcpy(&specular,input+offset,4);output.fog=(specular>>24)/255.f;offset+=4;}
        if(fog_enabled&&!rhw&&fog_vertex_mode){
            if(fog_vertex_mode==1)output.fog=std::exp(-fog_density*fog_distance);
            else if(fog_vertex_mode==2){const float distance=fog_density*fog_distance;output.fog=std::exp(-distance*distance);}
            else if(fog_vertex_mode==3)output.fog=fog_start==fog_end?(fog_distance<fog_end?1.f:0.f):(fog_end-fog_distance)/(fog_end-fog_start);
            output.fog=std::clamp(output.fog,0.f,1.f);
        }
        if(fvf&D3DFVF_TEX1 && offset+8<=stride){std::memcpy(&output.u,input+offset,4);std::memcpy(&output.v,input+offset+4,4);}else output.u=output.v=0;
        // Two-component D3D texture coordinates are extended to (u,v,1,0).
        // Transformed/lit vertices already contain final texture coordinates.
        if(!rhw&&(texture_transform_flags&255)){
            const float u=output.u,v=output.v;float uv[4];for(int column=0;column<4;++column)uv[column]=u*texture_matrix.m[0][column]+v*texture_matrix.m[1][column]+texture_matrix.m[2][column];
            output.u=uv[0];output.v=uv[1];const auto count=texture_transform_flags&255;if((texture_transform_flags&256)&&count>=2&&count<=4&&uv[count-1]!=0.f){output.u/=uv[count-1];output.v/=uv[count-1];}
        }
        // D3D9 pixel centers are at integers; OpenGL ES's are at half integers.
        if(!rhw){output.x+=output.w/viewport.Width;output.y-=output.w/viewport.Height;}
        // Keep texture row zero at the D3D top edge, including render textures.
        if(offscreen())output.y=-output.y;
    }
    HRESULT draw(D3DPRIMITIVETYPE type,UINT primitives,const void* data,UINT stride){if(type<1||type>6)return E_INVALIDARG;if(!primitives)return S_OK;const auto count=vertex_count(type,primitives);const UINT minimum_stride=((fvf&D3DFVF_XYZRHW)?16:12)+((fvf&D3DFVF_DIFFUSE)?4:0)+((fvf&0x80)?4:0)+((fvf&D3DFVF_TEX1)?8:0);if(!data||stride<minimum_stride||count>16777216)return E_INVALIDARG;mark_target_dirty();auto& vertices=transformed_vertices;vertices.resize(count);auto* bytes=static_cast<const std::uint8_t*>(data);for(unsigned index=0;index<count;++index)transform(vertices[index],bytes+std::size_t(index)*stride,stride);
        ++frame_draws;for(const auto& vertex:vertices){const auto w=vertex.w!=0.f?vertex.w:1.f;const auto x=vertex.x/w,y=vertex.y/w;frame_min_x=std::min(frame_min_x,x);frame_max_x=std::max(frame_max_x,x);frame_min_y=std::min(frame_min_y,y);frame_max_y=std::max(frame_max_y,y);frame_max_alpha=std::max(frame_max_alpha,vertex.a);}
        glUseProgram(program);glBindVertexArray(vao);glBindBuffer(GL_ARRAY_BUFFER,vbo);glBufferData(GL_ARRAY_BUFFER,vertices.size()*sizeof(Vertex),vertices.data(),GL_STREAM_DRAW);
        const bool sample_texture=bound_texture&&(uses_texture(color_op,color_arg1,color_arg2)||uses_texture(alpha_op,alpha_arg1,alpha_arg2));
        glUniform1i(texture_location,0);glUniform1i(use_texture_location,sample_texture);glUniform4f(texture_factor_location,((texture_factor>>16)&255)/255.f,((texture_factor>>8)&255)/255.f,(texture_factor&255)/255.f,((texture_factor>>24)&255)/255.f);
        glUniform1i(color_op_location,color_op);glUniform1i(color_arg1_location,color_arg1);glUniform1i(color_arg2_location,color_arg2);
        glUniform1i(alpha_op_location,alpha_op);glUniform1i(alpha_arg1_location,alpha_arg1);glUniform1i(alpha_arg2_location,alpha_arg2);
        glUniform1i(alpha_test_location,alpha_test);glUniform1f(alpha_ref_location,float(alpha_ref));glUniform1i(alpha_func_location,alpha_func);
        glUniform1i(fog_enabled_location,fog_enabled);glUniform3f(fog_color_location,((fog_color>>16)&255)/255.f,((fog_color>>8)&255)/255.f,(fog_color&255)/255.f);glUniform1i(fog_table_mode_location,fog_table_mode);glUniform3f(fog_parameters_location,fog_start,fog_end,fog_density);
        // A diffuse-only mask must not sample a still-bound render texture:
        // D3D ignores that stage argument, while OpenGL ES would reject feedback.
        glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,sample_texture?bound_texture->texture:0);
        if(sample_texture){glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,wrap_s);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,wrap_t);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,min_filter);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,mag_filter);}glDrawArrays(topology(type),0,count);return S_OK;}
    HRESULT DrawPrimitive(D3DPRIMITIVETYPE type,UINT start,UINT primitives)override{if(!stream)return E_FAIL;const auto offset=std::size_t(stream_offset)+std::size_t(start)*stream_stride;const auto bytes=std::size_t(vertex_count(type,primitives))*stream_stride;if(offset>stream->bytes.size()||bytes>stream->bytes.size()-offset)return E_INVALIDARG;return draw(type,primitives,stream->bytes.data()+offset,stream_stride);}
    HRESULT DrawPrimitiveUP(D3DPRIMITIVETYPE type,UINT primitives,const void* data,UINT stride)override{return draw(type,primitives,data,stride);}
};

struct NativeDirect3D final:IDirect3D9,ComObject<NativeDirect3D> {
    HRESULT QueryInterface(REFIID,void** output)override{return query(output,this);}ULONG AddRef()override{return add();}ULONG Release()override{return release(this);}
    HRESULT GetAdapterDisplayMode(UINT,D3DDISPLAYMODE* output)override{if(!output)return E_POINTER;output->Width=1280;output->Height=960;output->RefreshRate=60;output->Format=D3DFMT_A8R8G8B8;return S_OK;}
    HRESULT CheckDeviceFormat(UINT,D3DDEVTYPE,D3DFORMAT,DWORD,D3DRESOURCETYPE,D3DFORMAT format)override{return format==D3DFMT_A8R8G8B8||format==D3DFMT_X8R8G8B8||format==D3DFMT_D16?S_OK:E_FAIL;}
    HRESULT CreateDevice(UINT,D3DDEVTYPE,HWND,DWORD,D3DPRESENT_PARAMETERS* parameters,IDirect3DDevice9** output)override{if(!output)return E_POINTER;*output=nullptr;auto* device=new NativeDevice(parameters);if(!device->context_ready||!device->program_linked){delete device;return E_FAIL;}*output=device;return S_OK;}
};

int decode_image_to_bgra(const void* bytes,unsigned length,std::uint8_t* output,int pitch,int width,int height,int source_left,int source_top,int source_right,int source_bottom,DWORD filter,D3DCOLOR color_key,void* information) {
    if(!bytes||!length||!output||width<=0||height<=0)return -1;
    {
        CFDataRef data=CFDataCreateWithBytesNoCopy(kCFAllocatorDefault,static_cast<const UInt8*>(bytes),length,kCFAllocatorNull);
        CGImageSourceRef source=data?CGImageSourceCreateWithData(data,nullptr):nullptr;
        CGImageRef full=source?CGImageSourceCreateImageAtIndex(source,0,nullptr):nullptr;
        if(!full){if(source)CFRelease(source);if(data)CFRelease(data);th20_ios_log("graphics: image decode failed length=%u magic=%02x%02x%02x%02x",length,length>0?static_cast<const unsigned char*>(bytes)[0]:0,length>1?static_cast<const unsigned char*>(bytes)[1]:0,length>2?static_cast<const unsigned char*>(bytes)[2]:0,length>3?static_cast<const unsigned char*>(bytes)[3]:0);return -1;}
        const auto original_width=CGImageGetWidth(full),original_height=CGImageGetHeight(full);
        if(information){auto* v=static_cast<std::uint32_t*>(information);v[0]=original_width;v[1]=original_height;v[2]=1;v[3]=1;v[4]=D3DFMT_A8R8G8B8;v[5]=D3DRTYPE_TEXTURE;v[6]=0;}
        const int sw=source_right>source_left?source_right-source_left:int(original_width);
        const int sh=source_bottom>source_top?source_bottom-source_top:int(original_height);
        CGImageRef cropped=CGImageCreateWithImageInRect(full,CGRectMake(source_left,source_top,sw,sh));
        std::vector<std::uint8_t> rgba(std::size_t(width)*height*4);
        CGColorSpaceRef colors=CGColorSpaceCreateDeviceRGB();
        CGContextRef context=CGBitmapContextCreate(rgba.data(),width,height,8,width*4,colors,CGBitmapInfo(kCGImageAlphaPremultipliedLast)|kCGBitmapByteOrder32Big);
        if(context&&cropped){
            CGContextSetBlendMode(context,kCGBlendModeCopy);
            CGContextSetInterpolationQuality(context,(filter&0xff)==1?kCGInterpolationNone:kCGInterpolationHigh);
            CGContextDrawImage(context,CGRectMake(0,0,width,height),cropped);
            for(int y=0;y<height;++y)for(int x=0;x<width;++x){
                const auto* p=rgba.data()+(std::size_t(y)*width+x)*4;auto* q=output+std::size_t(y)*pitch+x*4;
                const unsigned a=p[3];
                // D3D textures contain straight alpha; CoreGraphics produces premultiplied alpha.
                q[0]=a?std::min(255u,(unsigned(p[2])*255+a/2)/a):0;
                q[1]=a?std::min(255u,(unsigned(p[1])*255+a/2)/a):0;
                q[2]=a?std::min(255u,(unsigned(p[0])*255+a/2)/a):0;q[3]=a;
                if(color_key&&((unsigned(q[2])<<16)|(unsigned(q[1])<<8)|q[0])==(color_key&0xffffff))q[3]=0;
            }
        }
        const bool okay=context&&cropped;
        if(context)CGContextRelease(context);CGColorSpaceRelease(colors);if(cropped)CGImageRelease(cropped);CGImageRelease(full);CFRelease(source);CFRelease(data);
        return okay?0:-1;
    }
}

}

extern "C" IDirect3D9* WINAPI Direct3DCreate9(UINT){return new NativeDirect3D;}
extern "C" HRESULT WINAPI D3DXCreateTexture(IDirect3DDevice9* device,UINT width,UINT height,UINT levels,DWORD usage,D3DFORMAT format,D3DPOOL pool,IDirect3DTexture9** output){return device?device->CreateTexture(width,height,levels,usage,format,pool,output,nullptr):E_POINTER;}
extern "C" HRESULT WINAPI D3DXLoadSurfaceFromFileInMemory(IDirect3DSurface9* destination,const PALETTEENTRY*,const RECT* destination_rectangle,const void* bytes,UINT length,const RECT* source_rectangle,DWORD filter,D3DCOLOR color_key,void* information){
    auto* surface=dynamic_cast<NativeSurface*>(destination);if(!surface||!bytes)return E_INVALIDARG;const RECT target=destination_rectangle?*destination_rectangle:RECT{0,0,LONG(surface->width),LONG(surface->height)};const RECT source=source_rectangle?*source_rectangle:RECT{0,0,0,0};
    if(!valid_rect(target,surface->width,surface->height))return E_INVALIDARG;
    auto* output=surface->data()+std::size_t(target.top)*surface->pitch()+target.left*4;const auto result=decode_image_to_bgra(bytes,length,output,surface->pitch(),target.right-target.left,target.bottom-target.top,source.left,source.top,source.right,source.bottom,filter,color_key,information);if(!result)surface->upload(&target);
    return result?E_FAIL:S_OK;
}
extern "C" HRESULT WINAPI D3DXLoadSurfaceFromSurface(IDirect3DSurface9* destination,const PALETTEENTRY*,const RECT* destination_rectangle,IDirect3DSurface9* source,const PALETTEENTRY*,const RECT* source_rectangle,DWORD filter,D3DCOLOR color_key){
    auto* out=dynamic_cast<NativeSurface*>(destination);auto* in=dynamic_cast<NativeSurface*>(source);if(!out||!in)return E_INVALIDARG;if(color_key)return unsupported("surface-copy color key",color_key);
    const RECT dr=destination_rectangle?*destination_rectangle:RECT{0,0,LONG(out->width),LONG(out->height)},sr=source_rectangle?*source_rectangle:RECT{0,0,LONG(in->width),LONG(in->height)};
    if(!valid_rect(dr,out->width,out->height)||!valid_rect(sr,in->width,in->height))return E_INVALIDARG;
    GLint previous_read{},previous_draw{};glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,&previous_read);glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&previous_draw);
    glBindFramebuffer(GL_READ_FRAMEBUFFER,in->framebuffer());glBindFramebuffer(GL_DRAW_FRAMEBUFFER,out->framebuffer());
    glBlitFramebuffer(sr.left,in->owner?sr.top:in->height-sr.top,sr.right,in->owner?sr.bottom:in->height-sr.bottom,dr.left,out->owner?dr.top:out->height-dr.top,dr.right,out->owner?dr.bottom:out->height-dr.bottom,GL_COLOR_BUFFER_BIT,(filter&0xff)<=2?GL_NEAREST:GL_LINEAR);
    glBindFramebuffer(GL_READ_FRAMEBUFFER,previous_read);glBindFramebuffer(GL_DRAW_FRAMEBUFFER,previous_draw);if(out->owner)out->owner->gpu_dirty=true;else if(current_backbuffer_texture)current_backbuffer_texture->gpu_dirty=true;return S_OK;
}
