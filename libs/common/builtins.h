#ifndef CLST_BUILTINS_H
#define CLST_BUILTINS_H

// Portable macros for attributes & compiler built-ins.
// Prefer standard options.
// Support MSVC, GCC, Clang.
// Support both C and C++.

#if defined(__cplusplus)
#define CLST_CPP 1
#if __cplusplus >= 201103L
#define CLST_CPP11 1
#endif
#if __cplusplus >= 201402L
#define CLST_CPP14 1
#endif
#if __cplusplus >= 201703L
#define CLST_CPP17 1
#endif
#if __cplusplus >= 202002L
#define CLST_CPP20 1
#endif
#if __cplusplus >= 202302L
#define CLST_CPP23 1
#endif
#else // Not C++
#define CLST_C 1
#if __STDC_VERSION__ >= 199409L
#define CLST_C95 1
#endif
#if __STDC_VERSION__ >= 199901L
#define CLST_C99 1
#endif
#if __STDC_VERSION__ >= 201112L
#define CLST_C11 1
#endif
#if __STDC_VERSION__ >= 201710L
#define CLST_C17 1
#endif
#if __STDC_VERSION__ >= 202311L
#define CLST_C23 1
#endif
#endif

// Unreachable
#if CLST_CPP23
#include <utility> // std::unreachable
#endif
#if CLST_C23
#include <stddef.h>
#define CLST_UNREACHABLE unreachable()
#elif defined(__cpp_lib_unreachable)
#define CLST_UNREACHABLE std::unreachable()
#elif defined(_MSC_VER) // MSVC
#define CLST_UNREACHABLE __assume(false)
#else // GCC, Clang
#define CLST_UNREACHABLE __builtin_unreachable()
#endif

// Assume
#if CLST_CPP23
#if __has_cpp_attribute(assume) >= 202207L
#define CLST_ASSUME(COND) [[assume(COND)]]
#endif
#endif
#ifndef CLST_ASSUME
#if defined(_MSC_VER)
#define CLST_ASSUME(COND) __assume(COND)
#elif defined(__clang__)
#define CLST_ASSUME(COND) __builtin_assume(COND)
#elif __GNUC__ >= 13
#define CLST_ASSUME(COND) __attribute__((assume(COND))) // Requires GCC >= 13
#else
#define CLST_ASSUME(COND) CLST_ASSUME_NOT_AVAILABLE
#endif
#endif

// No return
#if CLST_CPP11
#define CLST_NORETURN [[noreturn]]
#elif CLST_C23
#define CLST_NORETURN [[noreturn]]
#elif CLST_C11
#define CLST_NORETURN _Noreturn // deprecated in C23
#elif defined(_MSC_VER)
#define CLST_NORETURN __declspec(noreturn)
#else
#define CLST_NORETURN __attribute__((noreturn))
#endif

// Force inline
#if defined(_MSC_VER)
#define CLST_FORCE_INLINE __forceinline
#else
#define CLST_FORCE_INLINE __attribute__((always_inline))
#endif

// Deprecated
#if CLST_CPP14
#define CLST_DEPRECATED [[deprecated]]
#define CLST_DEPRECATED_REASON(REASON) [[deprecated(REASON)]]
#elif CLST_C23
#define CLST_DEPRECATED [[deprecated]]
#define CLST_DEPRECATED_REASON(REASON) [[deprecated(REASON)]]
#elif defined(_MSC_VER)
#define CLST_DEPRECATED __declspec(deprecated)
#define CLST_DEPRECATED_REASON(REASON) __declspec(deprecated(REASON))
#else
#define CLST_DEPRECATED __attribute__((deprecated))
#define CLST_DEPRECATED_REASON(REASON) __attribute__((deprecated(REASON)))
#endif

// Restrict
#if CLST_C99
#define CLST_RESTRICT restrict
#elif defined(_MSC_VER)
// When __restrict is used, the compiler won't propagate the no-alias property of a variable.
// That is, if you assign a __restrict variable to a non-__restrict variable,
// the compiler will still allow the non-__restrict variable to be aliased.
// This is different from the behavior of the C99 C language restrict keyword.
#define CLST_RESTRICT __restrict
#else
#define CLST_RESTRICT __restrict__
#endif

#endif /* CLST_BUILTINS_H */
