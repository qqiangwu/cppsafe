#ifndef __has_feature
#define __has_feature(feature) 0
#endif

#if __has_feature(address_sanitizer) || defined(__SANITIZE_ADDRESS__)
// NOLINTNEXTLINE: llvm lib is compiled without asan
extern "C" const char* __asan_default_options() { return "detect_container_overflow=0"; }
#endif
