struct [[gsl::Owner(int)]] Owner {
    Owner(int* p);
};

// expected-no-diagnostics
// TODO
void from_new()
{
    int* p = new int{};

    Owner o1(p);
    Owner o2(p);
}

void from_param(int* p)
{
    Owner o1(p);
    Owner o2(p);
}