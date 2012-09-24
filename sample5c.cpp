// sample5b.cpp: solution2: binary search
#include "checkerlib.h"
using namespace checker;

const int MAX_N = 100000;
const int MIN_X = -1000000000;
const int MAX_X =  1000000000;

int main() {
  Reader in(stdin);
  const int N = in.readInt("N").range(1,MAX_N).spc();
  const int K = in.readInt("K").range(1,MAX_N).eol();
  int lo = -1, hi = N;
  for(int i = 0; ; i++) {
    int mid = (lo+hi)/2;
    printf("%d\n", mid);
    fflush(stdout);
    const int result = in.readInt("result[%d]=x[%d]",i,mid).range(MIN_X,MAX_X).eol();
    if(result==0) break;
    if(result<0) lo=mid; else hi=mid;
  }
  // in.readEof();
  in.abortReading();
  return 0;
}
