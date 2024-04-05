struct Dummy {
    int m_a;
    bool x();
};

void test_buggy1(Dummy* pointer) {
    if (pointer != 0 || pointer->m_a) { }
    // expected-note@-1 {{is compared to null here}}
    // expected-warning@-2 {{dereferencing a null pointer}}
}

void test_buggy2(Dummy* pointer) {
    if (pointer == 0 && pointer->x()) { }
    // expected-note@-1 {{is compared to null here}}
    // expected-warning@-2 {{passing a null pointer as argument where a non-null pointer is expected}}
}

void test_buggy3(Dummy* pointer) {
    if (!pointer && pointer->x()) { }
    // expected-note@-1 {{is compared to null here}}
    // expected-warning@-2 {{passing a null pointer as argument where a non-null pointer is expected}}
}

void test_ok(Dummy* pointer) {
    if (pointer == 0 || pointer->m_a) {  }
    if (pointer != 0 && pointer->x()) {  }
    if (pointer && pointer->x()) {  }
}
