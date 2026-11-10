// Builds the organic bank, times it, prints per-sound stats and writes an audition WAV.
#include "../core/sk_org.h"
#include <cstdio>
#include <cmath>
#include <vector>
#include <chrono>
using namespace sk;
static const char* NAMES[NUM_ORG] = { "RAMEAU","BRANCHE","BOIS","BAMBOU","FEUILLES","GRAINES","CHIPS","BISCOTTE","CELERI","COQUILLE","NOIX",
  "FEU","ALLUMETTE","GRAVIER","CAILLOUX","SABLE","NEIGE","GLACON","VERRE","GOUTTE","BULLES","PLUIE","PSCHITT","BOUCHON",
  "PAPIER","SACHET","P.BULLE","ZIP","VELCRO","CLAP","DOIGTS","LANGUE","PHALANGE","PINGPONG","PIECES","GRILLON" };
static OrgBank B;
int main (int argc, char** argv)
{
    auto t0 = std::chrono::high_resolution_clock::now();
    B.build();
    auto t1 = std::chrono::high_resolution_clock::now();
    printf ("build %.1f ms, %d floats (%.1f MB) of %d\n", std::chrono::duration<double, std::milli> (t1 - t0).count(), B.used, B.used * 4 / 1e6, (int) ORG_POOL);
    std::vector<float> out;
    for (int id = 0; id < NUM_ORG; ++id)
    {
        printf ("%-10s", NAMES[id]);
        for (int v = 0; v < ORG_VAR; ++v)
        {
            const float* x = B.data (id, v); int L = B.len[id][v];
            double pk = 0, e = 0, num = 0, den = 0; int bad = 0;
            for (int i = 0; i < L; ++i) { pk = fmax (pk, fabs (x[i])); e += x[i] * x[i]; if (! std::isfinite (x[i])) bad++; }
            // zero-crossing rate as a cheap brightness proxy
            int zc = 0; for (int i = 1; i < L; ++i) if ((x[i] >= 0) != (x[i - 1] >= 0)) zc++;
            printf (" | %4dms pk%.2f rms%.3f zc%5.0f%s", L * 1000 / ORG_SR, pk, sqrt (e / L), zc * (double) ORG_SR / L / 2, bad ? " NAN" : "");
            for (int i = 0; i < L; ++i) out.push_back (x[i]);
            for (int i = 0; i < ORG_SR / 8; ++i) out.push_back (0);
        }
        printf ("\n");
        for (int i = 0; i < ORG_SR / 3; ++i) out.push_back (0);
    }
    if (argc > 1)
    {
        FILE* f = fopen (argv[1], "wb"); int n = (int) out.size(), db = n * 2;
        auto w32 = [&] (unsigned v) { fwrite (&v, 4, 1, f); }; auto w16 = [&] (unsigned short v) { fwrite (&v, 2, 1, f); };
        fwrite ("RIFF", 1, 4, f); w32 (36 + db); fwrite ("WAVE", 1, 4, f); fwrite ("fmt ", 1, 4, f); w32 (16); w16 (1); w16 (1);
        w32 (ORG_SR); w32 (ORG_SR * 2); w16 (2); w16 (16); fwrite ("data", 1, 4, f); w32 (db);
        for (float s : out) { int q = (int) lrintf (fmaxf (-1, fminf (1, s)) * 32767); short v = (short) q; fwrite (&v, 2, 1, f); }
        fclose (f);
    }
}
