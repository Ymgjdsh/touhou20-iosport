// Console runner for the same input probe used by native platform diagnostics.
// This supplies diagnostic output only; no game or OS behavior is stubbed.
#include <cstdarg>
#include <cstdio>
#include "ios_host.h"
extern "C" int th20_ios_input_probe();
void th20_ios_log(const char *format, ...) {
    va_list arguments; va_start(arguments, format);
    std::vfprintf(stdout, format, arguments);
    va_end(arguments); std::fputc('\n', stdout);
}
int main() { return th20_ios_input_probe(); }
