void foo(int* p)
{
    *reinterpret_cast<double*>(p) = 0;
    // expected-warning@-1 {{unsafe cast disables lifetime analysis}}
}