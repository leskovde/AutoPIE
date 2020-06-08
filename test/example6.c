#include <stdio.h>
#include <stdlib.h>

/*
 * This illustrates some fun with global/local variables. In function foo, 
 * j is local and i is global. Since i is being used as a loop variable, this 
 * is almost certainly wrong. Making j local here may or may not be logically 
 * correct, but it is certainly stylistically incorrect since the semantic 
 * meaning of j is being used in two distinct ways (once as a global, once 
 * as a local, which by definition must be inconsistent). In main, j is local. 
 * So, when it gets set by the call to foo, the global value is not being set. 
 * So, ineedj is out of luck -- the value is still undefined. 
 *
 */

int i = 5;
int j;

int foo(int j) {
  for (i=0; i<j; i++) do_nothing();
  return j;
}

void ineedj(void) {
  cout << "j is " << j << "\n";
}

main() {
  int j;
  j = foo(i);
  ineedj();
}
