#pragma once
#include "d3d9.h"
// Texture helpers exposed through the native compatibility export resolver.
extern "C" HRESULT WINAPI D3DXCreateTexture(IDirect3DDevice9*,UINT,UINT,UINT,DWORD,D3DFORMAT,D3DPOOL,IDirect3DTexture9**);
extern "C" HRESULT WINAPI D3DXLoadSurfaceFromFileInMemory(IDirect3DSurface9*,const PALETTEENTRY*,const RECT*,const void*,UINT,const RECT*,DWORD,D3DCOLOR,void*);
extern "C" HRESULT WINAPI D3DXLoadSurfaceFromSurface(IDirect3DSurface9*,const PALETTEENTRY*,const RECT*,IDirect3DSurface9*,const PALETTEENTRY*,const RECT*,DWORD,D3DCOLOR);
