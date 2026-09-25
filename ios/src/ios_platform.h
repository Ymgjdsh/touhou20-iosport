#pragma once
#include <stdint.h>
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif
// Engine initialization supplies these exact UIKit sandbox directories once.
void th20_ios_configure_paths(const char* resources,const char* documents);
const char* th20_ios_resource_directory(void);
const char* th20_ios_documents_directory(void);
uint64_t th20_ios_monotonic_microseconds(void);
bool th20_ios_save_bgra_png(const char* path,const void* pixels,int width,int height,int pitch);
bool th20_ios_is_japanese_locale(void);
void th20_ios_log_cp932(const char* text);
void th20_ios_report_cp932_error(const char* text);
#ifdef __cplusplus
}
#endif
