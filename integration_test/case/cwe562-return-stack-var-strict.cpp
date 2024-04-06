// ARGS: -Wlifetime-output

void Get(float **x)
{
  float f;
  *x = &f;
}  // expected-note {{pointee 'f' left the scope here}}
// expected-warning@-1 {{returning a dangling pointer as output value '*x'}}

struct CVariable {
  char  name[64];
};

void test(int n, char *tokens[])
{
  for (int i=0;i<n;i++)
  {
    CVariable  var;
    tokens[i]  =  var.name;
  }
}
