// ARGS: -Wlifetime-output

void Get(float **x)
{  // expected-warning {{returning a dangling pointer as output value '*x'}}
  float f;
  *x = &f;
}  // expected-note {{pointee 'f' left the scope here}}

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
