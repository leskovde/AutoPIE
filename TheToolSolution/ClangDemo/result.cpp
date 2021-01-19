#include <stdexcept>

void do_math(int *x) {
  (*x)++;
}

int main(void) {
  auto x = 0;
  auto y = 1;
  auto z = 4;

  do_math(&x);
  do_math(&y);

  y += x;

  if (y + 1 == z) {
    throw std::invalid_argument("asdasdasd");
  }

  x = 0;
  y = 0;
  z = 0;
}