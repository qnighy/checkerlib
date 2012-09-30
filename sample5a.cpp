// sample5a.cpp: reactive
#include "checkerlib.h"
using namespace checker;

const int MAX_N = 100000;
const int MIN_X = -1000000000;
const int MAX_X =  1000000000;

int main(int argc, char *argv[]) {
  Reader in(stdin);
  in.enableIODump();
  const int N = in.readInt("N").range(1,MAX_N).spc();
  const int K = in.readInt("K").range(1,MAX_N).eol();
  static int xs[MAX_N];
  int last_val = MIN_X;
  for(int i = 0; i < N; i++) {
    xs[i] = in.readInt("xs[%d]", i).range(last_val,MAX_X).ary(i,N);
    last_val = xs[i];
  }
  in.readEof();
  bool isCorrect = true;
  try {
    Process p;
    p.push(argv+1).execute();
    p.enableIODump();
    p.printf("%d %d\n", N, K);
    p.flush();
    int count = 0;
    while(true) {
      if(count == K) {
        isCorrect = false;
        break;
      }
      const int guess = p.readInt("guess[%d]",count).range(0,N-1).eol();
      p.printf("%d\n", xs[guess]);
      p.flush();
      if(xs[guess] == 0) {
        break;
      }
    }
    p.closeWriting();
    p.readEof();
    p.closeProcess();
  } catch(const ParseError& err) {
    fprintf(stderr, "%s\n", err.what());
    printf("Incorrect.\n");
    return 0;
  }
  if(isCorrect) {
    printf("Correct.\n");
  } else {
    printf("Incorrect.\n");
  }
  return 0;
}

