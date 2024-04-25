#include <cstddef>          // std::size_t/ptrdiff_t
#include <iterator>         // std::iterator_traits/input_iterator_tag
#include <map>              // std::map
#include <tuple>            // std::get/tuple_element_t
#include <type_traits>      // std::remove_reference_t
#include <utility>          // std::declval/get
#include <string>

template <class T>
void __lifetime_type_category_arg(T&&) {}

template <class T>
void __lifetime_contracts(T &&) {}

namespace adl_detail {

using std::begin;
using std::end;

template <typename Rng>
auto adl_begin(Rng&& rng)
{
    return begin(rng);
}

template <typename Rng>
auto adl_end(Rng&& rng)
{
    return end(rng);
}

} // namespace adl_detail

template <typename Rng, std::size_t I>
class elements_view {
    using base_range_type = std::remove_reference_t<Rng>;
    using base_iterator_type =
        decltype(adl_detail::adl_begin(std::declval<Rng>()));
    using base_value_type =
        typename std::iterator_traits<base_iterator_type>::value_type;

public:
    class iterator {
    public:
        using difference_type = std::ptrdiff_t;
        using value_type = std::tuple_element_t<I, base_value_type>;
        using pointer = decltype(&std::get<I>(
            *adl_detail::adl_begin(std::declval<Rng>())));
        using reference = decltype(std::get<I>(
            *adl_detail::adl_begin(std::declval<Rng>())));
        using iterator_category =
            std::input_iterator_tag;  // for simplicity

        iterator() = default;
        explicit iterator(const base_iterator_type& it) : it_(it) { }

        reference operator*() const { return std::get<I>(*it_); }
        pointer operator->() const { return &std::get<I>(*it_); }

        iterator& operator++()
        {
            ++it_;
            return *this;
        }
        iterator operator++(int)
        {
            iterator temp{*this};
            ++it_;
            return temp;
        }

        friend bool operator==(const iterator& lhs, const iterator& rhs)
        {
            return lhs.it_ == rhs.it_;
        }
        friend bool operator!=(const iterator& lhs, const iterator& rhs)
        {
            return lhs.it_ != rhs.it_;
        }

    private:
        base_iterator_type it_;
    };

    elements_view(Rng& rng) : rng_(&rng) {}

    iterator begin() const
    {
        return iterator{adl_detail::adl_begin(*rng_)};
    }
    iterator end() const
    {
        return iterator{adl_detail::adl_end(*rng_)};
    }

private:
    base_range_type* rng_;
};

template <std::size_t I, typename Rng>
auto elements(Rng&& rng)
{
    return elements_view<Rng, I>(rng);
}

template <class T>
void Use(T&&);

using namespace std;

int main()
{
    map<int, string> mp{{1, "one"}, {2, "two"}, {3, "three"}, {4, "four"}};

    Use(elements<0>(mp));
    Use(elements<1>(mp));

    pair<int, string> pairs[]{
        {1, "one"}, {2, "two"}, {3, "three"}, {4, "four"}};
    Use(elements<1>(pairs));

    auto bad_view =
        elements<1>(map<int, string>{{1, "one"}, {2, "two"}, {3, "three"}});
        // expected-note@-1 {{temporary was destroyed at the end of the full expression}}
    Use(bad_view);  // expected-warning {{passing a dangling pointer as argument}}
}
