struct [[gsl::Owner(int)]] Owner {
    Owner() = default;

    int* Get();
};

struct [[gsl::Pointer(int)]] Ptr {
    Ptr() = default;
    Ptr(const Owner&);

    void Use();
};

Owner getOwner();

void foo(bool b)
{
    Ptr x1;
    auto p = b? x1: getOwner();  // expected-note {{temporary was destroyed at the end of the full expression}}

    p.Use();  // expected-warning {{passing a dangling pointer as argument}}
}

const int* returnInternal()
{
  Owner o;
  return o.Get();
  // expected-note@-1 {{pointee 'o' left the scope here}}
  // expected-warning@-2 {{returning a dangling pointer}}
}

const int* returnInternal2()
{
  Owner o;
  int* p = o.Get();
  return p;
  // expected-note@-1 {{pointee 'o' left the scope here}}
  // expected-warning@-2 {{returning a dangling pointer}}
}

struct Node {
    Node* next;
};

void test_list(Node* head) {
    for (Node *p = head; p != nullptr; p = p->next)  // expected-warning {{dereferencing a dangling pointer}}
    {
        delete p;  // expected-note {{deleted here}}
    }
}

void test_list_ok(Node* head) {
    for (Node *p = head; p != nullptr; )
    {
        auto* old = p;
        p = p->next;
        delete old;
    }
}