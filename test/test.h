#ifndef TEST_H_F7C82FF2_3F80_48A1_9E15_7D2A4DDB7FE8
#define TEST_H_F7C82FF2_3F80_48A1_9E15_7D2A4DDB7FE8

// Macros for tests

#include <stdio.h>
#include <stdlib.h>

#define SLATE_QUOTE(str) #str
#define SLATE_STR(str) SLATE_QUOTE(str)

#define SLATE_ASSERT(COND)                                                                                                    \
    do {                                                                                                                      \
        if (!(COND)) {                                                                                                        \
            fprintf(stderr, "%s:%s: %s: Assertion `%s' failed!\n", __FILE__, SLATE_STR(__LINE__), __func__, SLATE_STR(COND)); \
            exit(EXIT_FAILURE);                                                                                               \
        }                                                                                                                     \
    } while (0)

#define SLATE_ASSERT_EQ(L, R) SLATE_ASSERT((L) == (R))


#ifdef __cplusplus

#define SLATE_EXPECT_THROW(EXPR, EXCEPTION)                                                                                       \
    do {                                                                                                                          \
        try {                                                                                                                     \
            EXPR;                                                                                                                 \
        }                                                                                                                         \
        catch (const EXCEPTION&) {                                                                                                \
            break;                                                                                                                \
        }                                                                                                                         \
        fprintf(stderr, "%s:%s: Expected exception `%s' was not thrown!\n", __FILE__, SLATE_STR(__LINE__), SLATE_STR(EXCEPTION)); \
        exit(EXIT_FAILURE);                                                                                                       \
    } while (0)

#define SLATE_EXPECT_NOTHROW(EXPR)                                                                   \
    do {                                                                                             \
        try {                                                                                        \
            EXPR;                                                                                    \
        }                                                                                            \
        catch (...) {                                                                                \
            fprintf(stderr, "%s:%s: Unexpected exception caught!\n", __FILE__, SLATE_STR(__LINE__)); \
            exit(EXIT_FAILURE);                                                                      \
        }                                                                                            \
    } while (0)

#endif // __cplusplus

#endif /* TEST_H_F7C82FF2_3F80_48A1_9E15_7D2A4DDB7FE8 */
