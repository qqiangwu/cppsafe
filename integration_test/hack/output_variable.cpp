// expected-no-diagnostics
bool foo(bool cond, int*& out)
{
    if (!cond) {
        return false;
    }

    out = new int{};
    return true;
}