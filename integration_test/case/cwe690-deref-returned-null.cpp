// ARGS: -Wlifetime-null

int* get();

int foo1()
{
    return *get();  // expected-warning {{dereferencing a possibly null pointer}}
}
