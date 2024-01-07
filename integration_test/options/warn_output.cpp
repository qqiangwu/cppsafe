// ARGS: --Wlifetime-output

bool foo(bool x, int*& out)  // expected-note {{it was never initialized here}}
{
    if (x) {
        return false;  // expected-warning {{returning a dangling pointer as output value '*out'}}
    }

    out = new int{};
    return true;
}