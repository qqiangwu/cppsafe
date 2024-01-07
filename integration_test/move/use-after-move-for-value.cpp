namespace std {

template <class T>
__remove_reference_t(T)&& move(T&& t);

}

class Value
{
public:
    Value();

    virtual ~Value();

    Value(Value&& o) noexcept = default;
    Value& operator=(Value&& o) noexcept = default;

    [[clang::reinitializes]] void reuse();
};

template <class T>
void __lifetime_pset(T&&) {}

template <class T>
void __lifetime_type_category_arg(T&&) {}

void MoveIt(Value&& v);

void test_move_value()
{
    __lifetime_type_category_arg(Value{});
    // expected-warning@-1 {{lifetime type category is Value}}

    Value v1;
    __lifetime_pset(v1);
    // expected-warning@-1 {{pset(v1) = (v1)}}

    MoveIt(std::move(v1));
    __lifetime_pset(v1);
    // expected-warning@-1 {{pset(v1) = ((invalid))}}

    v1.reuse();
    __lifetime_pset(v1);
    // expected-warning@-1 {{pset(v1) = (v1)}}

    Value v2(std::move(v1));
    __lifetime_pset(v1);
    // expected-warning@-1 {{pset(v1) = ((invalid))}}

    v1 = Value{};
    __lifetime_pset(v1);
    // expected-warning@-1 {{pset(v1) = (v1)}}

    v2 = std::move(v1);
    __lifetime_pset(v1);
    // expected-warning@-1 {{pset(v1) = ((invalid))}}
}