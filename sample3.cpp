// sample3.cpp: read two integers (abort reading)
#include "checkerlib.h"
using namespace checker;

int main() {
  Reader in(stdin);
  const int a = in.readInt("a").range(-100,100).spc();
  const int b = in.readInt("b").range(-100,100).eol();
  printf("(%d) + (%d) = (%d)\n", a, b, a+b);
  in.abortReading();
  return 0;
}

