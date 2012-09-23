// sample4.cpp: read 2N large integers
#include "checkerlib.h"
using namespace checker;

const int MAX_N = 1000000;
const long long MIN_POS = -1000000000000LL;
const long long MAX_POS =  1000000000000LL;

int main() {
  try {
    Reader in(stdin);
    const int N = in.readInt("N").range(1,MAX_N).eol();
    static long long xs[MAX_N], ys[MAX_N];
    for(int i = 0; i < N; i++) {
      xs[i] = in.readLong("xs[%d]", i).range(MIN_POS,MAX_POS).spc();
      ys[i] = in.readLong("ys[%d]", i).range(MIN_POS,MAX_POS).eol();
    }
    in.readEof();
  } catch(const ParseError& err) {
    fprintf(stderr, "%s\n", err.what());
    printf("Incorrect.\n");
    return 0;
  }
  printf("Correct.\n");
  return 0;
}

