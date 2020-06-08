#include <stdio.h>
#include <stdlib.h>

/*
 * Essentially the same bug as the previous example. The space for the string 
 * is local to assign, and gets returned after leaving that function. 
 *
 */

char* assign() {
  return "hello world!";
}

main() {
  char *ptr = assign();
}
