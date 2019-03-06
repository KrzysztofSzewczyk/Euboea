
#include <stdio.h>

void pi(void) {
  const int N = 14*5000;
  const int NM = N - 14;
  int * a = malloc(sizeof(int) * N);
  int d = 0, e = 0, g = 0, h = 0, f = 10000, cnt = 1, c = NM, b;
  if(!a) {
    fputs("Out of memory.", stderr);
    return;
  }
  for(; c; c -= 14) {
    d = d % f;
    e = d;
    for(b = c - 1; b > 0; b--) {
      g = 2 * b - 1;
      if(c != NM)
        d = d * b + f * a[b];
      else
        d = d * b + f * (f / 5);
      a[b] = d % g;
      d = d / g;
    }
    printf("%04d", e + d / f);

    if(cnt % 16 == 0)
      putchar('\n');
    cnt++;
  }
}

int main(void) {
    pi();
}
