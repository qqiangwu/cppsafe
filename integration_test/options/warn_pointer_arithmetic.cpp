// ARGS: --Wlifetime-disabled

void foo(int* p)
{
    ++p;  // expected-warning {{pointer arithmetic disables lifetime analysis}}
    *p = 0;
}