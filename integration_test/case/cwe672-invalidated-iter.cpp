template <class T>
struct vector
{
    ~vector();

    using value_type = T;

    T* begin();
    T* end();

    void push_back(const T& t);
};

void foo()
{
    vector<int> numbers;
    for (int num : numbers)  // expected-warning {{dereferencing a possibly dangling pointer}}
    {
        numbers.push_back(num * 2);  // expected-note {{modified here}}
    }
}