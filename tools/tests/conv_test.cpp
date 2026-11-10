// Checks the two-stage convolver against direct convolution (wet = (x*h) delayed by 64).
#include "../../core/sk_box.h"
#include <cstdio>
#include <cmath>
#include <vector>
#include <cstdlib>
using namespace sk;
static IrBank bank; static RFFT<128> fa; static RFFT<1024> fb;
static UConv<64, 8> A; static UConv<512, 100> B;
int main() {
  fa.init(128); fb.init(1024); bank.sr = 48000; bank.used = 0;
  for (int L : {300, 512, 600, 5000, 30000}) {
    for (int i = 0; i < L; ++i) bank.h[i] = (float)rand()/RAND_MAX*2-1;
    std::vector<float> h(bank.h, bank.h + L);
    bank.store(1, L, fa, fb);
    const IrEntry& E = bank.e[1];
    float gA[8], gB[100]; for (auto& g : gA) g = 1; for (auto& g : gB) g = 1;
    A.reset(0); B.reset(192);
    A.setIR(bank.pool + E.offA, E.pa, E.pa, gA); B.setIR(bank.pool + E.offB, E.pb, E.pb, gB);
    const int T = 40000; std::vector<float> x(T), y(T);
    for (int i = 0; i < T; ++i) x[i] = (i < 20000) ? (float)rand()/RAND_MAX*2-1 : 0;
    float wa[32], wb[32];
    for (int i = 0; i < T; i += 32) { A.peek(wa, 32); B.peek(wb, 32); for (int k = 0; k < 32; ++k) y[i+k] = wa[k] + wb[k]; A.push(&x[i], 32, fa); B.push(&x[i], 32, fb); }
    double err = 0, mx = 0;
    for (int t = 0; t < T; t += 7) { double r = 0; for (int k = 0; k < L && k <= t - 64; ++k) r += (double)x[t-64-k]*h[k]; err = fmax(err, fabs(r - y[t])); mx = fmax(mx, fabs(r)); }
    printf("L=%5d pa=%d pb=%2d  max err %.2e (signal max %.1f)\n", L, E.pa, E.pb, err, mx);
    bank.used = 0;
  }
}
