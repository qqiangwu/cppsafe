#include <gsl/assert>

#define CPPSAFE_ASSERT(...) GSL_CONTRACT_CHECK("Assertion", __VA_ARGS__)
