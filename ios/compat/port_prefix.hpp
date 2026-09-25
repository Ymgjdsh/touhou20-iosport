#pragma once
#if !defined(TH20_IOS)
#error This prefix is only for the native iOS port.
#endif
#ifndef __cdecl
#define __cdecl
#endif
#ifndef __stdcall
#define __stdcall
#endif
#ifndef __fastcall
#define __fastcall
#endif
#include <cstdint>
#include <memory_resource>
