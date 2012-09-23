#include "checkerlib.h"
using namespace checker;

const int MAX_N = 1000000;
const long long MIN_POS = -1000000000000LL;
const long long MAX_POS =  1000000000000LL;

unsigned int xor128(void) {
  static unsigned int x = 123456789U;
  static unsigned int y = 362436069U;
  static unsigned int z = 521288629U;
  static unsigned int w =  88675123U;
  unsigned int t;

  t = x ^ (x << 11);
  x = y; y = z; z = w;
  return w = (w ^ (w >> 19)) ^ (t ^ (t >> 8));
}

inline long long rand_pos() {
  unsigned int upper = xor128();
  unsigned int lower = xor128();
  unsigned long long rawval = (((unsigned long long)upper)<<32) | lower;
  unsigned long long modval = rawval%2000000000001ULL;
  return (long long)modval-1000000000000;
}

int main() {
  const int N = MAX_N;
  printf("%d\n", N);
  for(int i = 0; i < N; i++) {
    long long x = rand_pos();
    long long y = rand_pos();
    printf("%lld %lld\n", x, y);
  }
  return 0;
}


