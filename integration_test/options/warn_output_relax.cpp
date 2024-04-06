void __lifetime_pmap();

bool foo1(bool x, int*& out)
{
    if (x) {
        return false;
    }

    out = new int{};
    return true;
}

bool foo2(bool x, int y, int*& out)
{
    out = &y;

    if (x) {
        return false;  // expected-warning {{returning a dangling pointer as output value '*out'}}
        // expected-note@-1 {{pointee 'y' left the scope here}}
    }

    out = new int{};
    return true;
}