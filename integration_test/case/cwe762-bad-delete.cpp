// expected-no-diagnostics
void Foo()
{
  int *p;
  // TODO
  delete &p;
}