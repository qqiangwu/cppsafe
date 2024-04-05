// ARGS: -Wlifetime-null

void test_unsafe_use_after_null_check(int* p)
    // expected-note@-1 {{the parameter is assumed to be potentially null. Consider using gsl::not_null<>, a reference instead of a pointer or an assert() to explicitly remove null}}
{
    if (p != nullptr)
    {
        *p = 42;
    }

    *p += 33;  // expected-warning {{dereferencing a possibly null pointer}}
}