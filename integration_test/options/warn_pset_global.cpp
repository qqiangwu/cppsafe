struct Test
{
    int* p;
};

Test* Get();

void test()
{
    auto* p = Get();

    int d;
    p->p = &d;
    // expected-warning@-1 {{the pset of '&d' must be a subset of {(global), (null)}, but is {(d)}}}
}