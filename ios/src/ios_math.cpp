#include <Windows.h>
#include <d3d9.h>
#include <cmath>
#include <cstring>
#include <cwchar>

namespace {
void multiply(D3DMATRIX& output,const D3DMATRIX& left,const D3DMATRIX& right) {
    D3DMATRIX result{};
    for(int row=0;row<4;++row)for(int column=0;column<4;++column)
        for(int inner=0;inner<4;++inner)result.m[row][column]+=left.m[row][inner]*right.m[inner][column];
    output=result;
}
struct V3 {float x,y,z;};struct V4 {float x,y,z,w;};
V4 transform4(const V3& input,const D3DMATRIX& matrix) {
    return {
        input.x*matrix.m[0][0]+input.y*matrix.m[1][0]+input.z*matrix.m[2][0]+matrix.m[3][0],
        input.x*matrix.m[0][1]+input.y*matrix.m[1][1]+input.z*matrix.m[2][1]+matrix.m[3][1],
        input.x*matrix.m[0][2]+input.y*matrix.m[1][2]+input.z*matrix.m[2][2]+matrix.m[3][2],
        input.x*matrix.m[0][3]+input.y*matrix.m[1][3]+input.z*matrix.m[2][3]+matrix.m[3][3]};
}
}

extern "C" D3DMATRIX* WINAPI D3DXMatrixRotationX(D3DMATRIX* out,float angle){const auto c=std::cos(angle),s=std::sin(angle);*out={{{1,0,0,0},{0,c,s,0},{0,-s,c,0},{0,0,0,1}}};return out;}
extern "C" D3DMATRIX* WINAPI D3DXMatrixRotationY(D3DMATRIX* out,float angle){const auto c=std::cos(angle),s=std::sin(angle);*out={{{c,0,-s,0},{0,1,0,0},{s,0,c,0},{0,0,0,1}}};return out;}
extern "C" D3DMATRIX* WINAPI D3DXMatrixRotationZ(D3DMATRIX* out,float angle){const auto c=std::cos(angle),s=std::sin(angle);*out={{{c,s,0,0},{-s,c,0,0},{0,0,1,0},{0,0,0,1}}};return out;}
extern "C" D3DMATRIX* WINAPI D3DXMatrixTranslation(D3DMATRIX* out,float x,float y,float z){*out={{{1,0,0,0},{0,1,0,0},{0,0,1,0},{x,y,z,1}}};return out;}
extern "C" D3DMATRIX* WINAPI D3DXMatrixMultiply(D3DMATRIX* out,const D3DMATRIX* a,const D3DMATRIX* b){multiply(*out,*a,*b);return out;}
extern "C" D3DMATRIX* WINAPI D3DXMatrixLookAtLH(D3DMATRIX* out,const V3* eye,const V3* at,const V3* up){
    auto normalize=[](V3 value){const auto length=std::sqrt(value.x*value.x+value.y*value.y+value.z*value.z);return V3{value.x/length,value.y/length,value.z/length};};
    const auto z=normalize({at->x-eye->x,at->y-eye->y,at->z-eye->z});
    const auto x=normalize({up->y*z.z-up->z*z.y,up->z*z.x-up->x*z.z,up->x*z.y-up->y*z.x});
    const V3 y{z.y*x.z-z.z*x.y,z.z*x.x-z.x*x.z,z.x*x.y-z.y*x.x};
    *out={{{x.x,y.x,z.x,0},{x.y,y.y,z.y,0},{x.z,y.z,z.z,0},{-(x.x*eye->x+x.y*eye->y+x.z*eye->z),-(y.x*eye->x+y.y*eye->y+y.z*eye->z),-(z.x*eye->x+z.y*eye->y+z.z*eye->z),1}}};return out;
}
extern "C" D3DMATRIX* WINAPI D3DXMatrixPerspectiveFovLH(D3DMATRIX* out,float fov,float aspect,float near_z,float far_z){const auto y=1/std::tan(fov*.5f),x=y/aspect;*out={{{x,0,0,0},{0,y,0,0},{0,0,far_z/(far_z-near_z),1},{0,0,-near_z*far_z/(far_z-near_z),0}}};return out;}
extern "C" V4* WINAPI D3DXVec3Transform(V4* out,const V3* input,const D3DMATRIX* matrix){*out=transform4(*input,*matrix);return out;}
extern "C" V3* WINAPI D3DXVec3TransformCoord(V3* out,const V3* input,const D3DMATRIX* matrix){const auto value=transform4(*input,*matrix);*out={value.x/value.w,value.y/value.w,value.z/value.w};return out;}
extern "C" V3* WINAPI D3DXVec3Project(V3* out,const V3* input,const D3DVIEWPORT9* viewport,const D3DMATRIX* projection,const D3DMATRIX* view,const D3DMATRIX* world){D3DMATRIX a{},combined{};multiply(a,*world,*view);multiply(combined,a,*projection);const auto value=transform4(*input,combined);const auto x=value.x/value.w,y=value.y/value.w,z=value.z/value.w;*out={viewport->X+(1+x)*viewport->Width*.5f,viewport->Y+(1-y)*viewport->Height*.5f,viewport->MinZ+z*(viewport->MaxZ-viewport->MinZ)};return out;}
extern "C" V3* WINAPI D3DXVec3ProjectArray(V3* output,UINT output_stride,const V3* input,UINT input_stride,const D3DVIEWPORT9* viewport,const D3DMATRIX* projection,const D3DMATRIX* view,const D3DMATRIX* world,UINT count){for(UINT index=0;index<count;++index)D3DXVec3Project(reinterpret_cast<V3*>(reinterpret_cast<std::uint8_t*>(output)+index*output_stride),reinterpret_cast<const V3*>(reinterpret_cast<const std::uint8_t*>(input)+index*input_stride),viewport,projection,view,world);return output;}

extern "C" HRESULT WINAPI D3DXCreateTexture(IDirect3DDevice9*,UINT,UINT,UINT,DWORD,D3DFORMAT,D3DPOOL,IDirect3DTexture9**);
extern "C" HRESULT WINAPI D3DXLoadSurfaceFromFileInMemory(IDirect3DSurface9*,const PALETTEENTRY*,const RECT*,const void*,UINT,const RECT*,DWORD,D3DCOLOR,void*);
extern "C" HRESULT WINAPI D3DXLoadSurfaceFromSurface(IDirect3DSurface9*,const PALETTEENTRY*,const RECT*,IDirect3DSurface9*,const PALETTEENTRY*,const RECT*,DWORD,D3DCOLOR);


namespace {
int native_d3dx_library;
thread_local DWORD library_error=0;
}
HMODULE LoadLibraryW(const wchar_t* name) {
    if(name && std::wcscmp(name,L"d3dx9_43.dll")==0){library_error=0;return &native_d3dx_library;}
    library_error=126;return nullptr;
}
BOOL FreeLibrary(HMODULE module){if(module==&native_d3dx_library)return TRUE;library_error=6;return FALSE;}
DWORD GetLastError(){return library_error;}
void* GetProcAddress(HMODULE module,const char* name){
    if(module!=&native_d3dx_library||!name){library_error=6;return nullptr;}
#define TH20_PROC(symbol) if(std::strcmp(name,#symbol)==0){library_error=0;return reinterpret_cast<void*>(&symbol);}
    TH20_PROC(D3DXMatrixRotationX);TH20_PROC(D3DXMatrixRotationY);TH20_PROC(D3DXMatrixRotationZ);
    TH20_PROC(D3DXMatrixTranslation);TH20_PROC(D3DXMatrixMultiply);TH20_PROC(D3DXMatrixLookAtLH);
    TH20_PROC(D3DXMatrixPerspectiveFovLH);TH20_PROC(D3DXVec3Transform);TH20_PROC(D3DXVec3TransformCoord);
    TH20_PROC(D3DXVec3Project);TH20_PROC(D3DXVec3ProjectArray);
    TH20_PROC(D3DXCreateTexture);TH20_PROC(D3DXLoadSurfaceFromFileInMemory);TH20_PROC(D3DXLoadSurfaceFromSurface);
#undef TH20_PROC
    library_error=ERROR_PROC_NOT_FOUND;return nullptr;
}
