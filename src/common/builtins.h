#ifndef SLATE_BUILTINS_H
#define SLATE_BUILTINS_H

// Portable macros for attributes & compiler built-ins.
// Prefer standard options.
// Support MSVC, GCC, Clang.
// Support both C and C++.

#if defined(__cplusplus)
#define SLATE_CPP 1
#if __cplusplus >= 201103L
#define SLATE_CPP11 1
#endif
#if __cplusplus >= 201402L
#define SLATE_CPP14 1
#endif
#if __cplusplus >= 201703L
#define SLATE_CPP17 1
#endif
#if __cplusplus >= 202002L
#define SLATE_CPP20 1
#endif
#if __cplusplus >= 202302L
#define SLATE_CPP23 1
#endif
#else // Not C++
#define SLATE_C 1
#if __STDC_VERSION__ >= 199409L
#define SLATE_C95 1
#endif
#if __STDC_VERSION__ >= 199901L
#define SLATE_C99 1
#endif
#if __STDC_VERSION__ >= 201112L
#define SLATE_C11 1
#endif
#if __STDC_VERSION__ >= 201710L
#define SLATE_C17 1
#endif
#if __STDC_VERSION__ >= 202311L
#define SLATE_C23 1
#endif
#endif

// Unreachable
#if SLATE_CPP23
#include <utility> // std::unreachable
#endif
#if SLATE_C23
#include <stddef.h>
#define SLATE_UNREACHABLE unreachable()
#elif defined(__cpp_lib_unreachable)
#define SLATE_UNREACHABLE std::unreachable()
#elif defined(_MSC_VER) // MSVC
#define SLATE_UNREACHABLE __assume(false)
#else // GCC, Clang
#define SLATE_UNREACHABLE __builtin_unreachable()
#endif

// Assume
#if SLATE_CPP23
#if __has_cpp_attribute(assume) >= 202207L
#define SLATE_ASSUME(COND) [[assume(COND)]]
#endif
#endif
#ifndef SLATE_ASSUME
#if defined(_MSC_VER)
#define SLATE_ASSUME(COND) __assume(COND)
#elif defined(__clang__)
#define SLATE_ASSUME(COND) __builtin_assume(COND)
#elif __GNUC__ >= 13
#define SLATE_ASSUME(COND) __attribute__((assume(COND))) // Requires GCC >= 13
#else
#define SLATE_ASSUME(COND) SLATE_ASSUME_NOT_AVAILABLE
#endif
#endif

// No return
#if SLATE_CPP11
#define SLATE_NORETURN [[noreturn]]
#elif SLATE_C23
#define SLATE_NORETURN [[noreturn]]
#elif SLATE_C11
#define SLATE_NORETURN _Noreturn // deprecated in C23
#elif defined(_MSC_VER)
#define SLATE_NORETURN __declspec(noreturn)
#else
#define SLATE_NORETURN __attribute__((noreturn))
#endif

// Force inline
#if defined(_MSC_VER)
#define SLATE_FORCE_INLINE __forceinline
#else
#define SLATE_FORCE_INLINE __attribute__((always_inline))
#endif

// Deprecated
#if SLATE_CPP14
#define SLATE_DEPRECATED [[deprecated]]
#define SLATE_DEPRECATED_REASON(REASON) [[deprecated(REASON)]]
#elif SLATE_C23
#define SLATE_DEPRECATED [[deprecated]]
#define SLATE_DEPRECATED_REASON(REASON) [[deprecated(REASON)]]
#elif defined(_MSC_VER)
#define SLATE_DEPRECATED __declspec(deprecated)
#define SLATE_DEPRECATED_REASON(REASON) __declspec(deprecated(REASON))
#else
#define SLATE_DEPRECATED __attribute__((deprecated))
#define SLATE_DEPRECATED_REASON(REASON) __attribute__((deprecated(REASON)))
#endif

// Restrict
#if SLATE_C99
#define SLATE_RESTRICT restrict
#elif defined(_MSC_VER)
// When __restrict is used, the compiler won't propagate the no-alias property of a variable.
// That is, if you assign a __restrict variable to a non-__restrict variable,
// the compiler will still allow the non-__restrict variable to be aliased.
// This is different from the behavior of the C99 C language restrict keyword.
#define SLATE_RESTRICT __restrict
#else
#define SLATE_RESTRICT __restrict__
#endif

#endif /* SLATE_BUILTINS_H */
