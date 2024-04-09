// expected-no-diagnostics
struct Value {
    int* p;

    ~Value()
    {
        delete p;
    }
};
