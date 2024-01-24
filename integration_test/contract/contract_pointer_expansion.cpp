template <class T>
void __lifetime_contracts(T&&);

struct [[gsl::Pointer(char)]] StringView
{
    const char* buffer_;
    int len_;

    StringView(const char* buf, int len)
        : buffer_(buf),
          len_(len)
    {
    }

    const char* data() const
    {
        __lifetime_contracts(&StringView::data);
        // expected-warning@-1 {{pset(Pre(this)) = (*this)}}
        // expected-warning@-2 {{pset(Pre(*this)) = (**this)}}
        // expected-warning@-3 {{pset(Pre((*this).buffer_)) = (**this)}}
        // expected-warning@-4 {{pset(Pre((*this).len_)) = (*this)}}
        // expected-warning@-5 {{pset(Post((return value))) = ((null), **this)}}
        return buffer_;
    }
};