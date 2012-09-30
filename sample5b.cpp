// sample5b.cpp: solution1: linear search
#include "checkerlib.h"
using namespace checker;

const int MAX_N = 100000;
const int MIN_X = -1000000000;
const int MAX_X =  1000000000;

int main() {
  Reader in(stdin);
  const int N = in.readInt("N").range(1,MAX_N).spc();
  const int K = in.readInt("K").range(1,MAX_N).eol();
  int i = 0;
  while(true) {
    printf("%d\n", i);
    fflush(stdout);
    const int result = in.readInt("result[%d]",i).range(MIN_X,MAX_X).eol();
    if(result==0) break;
    i++;
  }
  in.readEof();
  return 0;
}
