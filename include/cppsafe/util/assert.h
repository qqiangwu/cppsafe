#include <gsl/assert>

#include <cstdio>

// NOLINTBEGIN(cppcoreguidelines-avoid-do-while, cppcoreguidelines-pro-type-vararg, cppcoreguidelines-macro-usage)
#define CPPSAFE_ASSERT(e)                                                                                              \
    do {                                                                                                               \
        if (!(e)) [[unlikely]] {                                                                                       \
            fprintf(stderr, "%s:%d: failed assertion `%s'\n", __FILE__, __LINE__, #e);                                 \
            std::terminate();                                                                                          \
        }                                                                                                              \
    } while (false)
// NOLINTEND(cppcoreguidelines-avoid-do-while, cppcoreguidelines-pro-type-vararg, cppcoreguidelines-macro-usage)
