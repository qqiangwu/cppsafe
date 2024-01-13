template <class T> void __lifetime_contracts(T&& t);

struct ColumnFamilyHandle {};
struct Status {};

class [[gsl::Pointer(ColumnFamilyHandle)]] PinnableAttributeGroup {
 public:
  ColumnFamilyHandle* column_family() const { return column_family_; }
  const Status& status() const
  {
    __lifetime_contracts(&PinnableAttributeGroup::status);
    // expected-warning@-1 {{pset(Pre(this)) = (*this)}}
    // expected-warning@-2 {{pset(Pre(*this)) = ((null), **this)}}
    // expected-warning@-3 {{pset(Pre((*this).column_family_)) = (**this)}}
    // expected-warning@-4 {{pset(Pre((*this).status_)) = (*this)}}
    // expected-warning@-5 {{pset(Post((return value))) = (*this)}}
    return status_;
  }

  explicit PinnableAttributeGroup(ColumnFamilyHandle* column_family)
      : column_family_(column_family) {}

  void SetStatus(const Status& status);

  void Reset();

 private:
  ColumnFamilyHandle* column_family_;
  Status status_;
};