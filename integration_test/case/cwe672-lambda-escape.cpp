auto foo()
{
    auto obj = 0;
    return [&obj]() { };  // expected-warning {{returning a dangling pointer}}
    // expected-note@-1 {{pointee 'obj' left the scope here}}
}
