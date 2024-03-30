#include <gsl/assert>

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define CPPSAFE_ASSERT(...) GSL_CONTRACT_CHECK("Assertion", __VA_ARGS__)
