#include "../../core/sk_fft.h"
#include <cstdio>
#include <cmath>
#include <cstdlib>
#include <initializer_list>
using namespace sk;
static RFFT<8192> F;
int main() {
  for (int n : {16, 128, 1024, 2048, 4096, 8192}) {
    F.init(n);
    static float x[8192], re[4097], im[4097], y[8192];
    for (int i = 0; i < n; ++i) x[i] = (float)rand()/RAND_MAX*2-1;
    F.forward(x, re, im);
    double err = 0, mx = 0;
    for (int k = 0; k <= n/2; k += (n > 1024 ? 37 : 1)) {
      double sr = 0, si = 0;
      for (int i = 0; i < n; ++i) { double a = -2*M_PI*(double)k*i/n; sr += x[i]*cos(a); si += x[i]*sin(a); }
      err = fmax(err, fabs(sr-re[k]) + fabs(si-im[k])); mx = fmax(mx, fabs(sr)+fabs(si));
    }
    F.inverse(re, im, y);
    double e2 = 0; for (int i = 0; i < n; ++i) e2 = fmax(e2, fabs(y[i]-x[i]));
    printf("n=%5d forward rel err %.2e  roundtrip err %.2e\n", n, err/mx, e2);
  }
}
