// IMPAKT — CHIMERA's bank, the codec's side of tools/make_bank.py (core/sk_bank.cpp)
//   bank_tool enc <in.raw> <channels> <out.skc>     a sample (16-bit, interleaved) coded; prints its bytes and crc
//   bank_tool check <bank dir>                     reads chimera.idx and its files, decodes every sample (crc)
// build: clang++ -O2 -std=c++17 -Icore tools/bank_tool.cpp core/sk_bank.cpp -o build/bank_tool
#include "sk_bank.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <chrono>

using namespace sk;

static std::vector<unsigned char> readAll (const std::string& f)
{
    std::vector<unsigned char> v;
    FILE* fp = fopen (f.c_str(), "rb");
    if (! fp) return v;
    fseek (fp, 0, SEEK_END); const long n = ftell (fp); fseek (fp, 0, SEEK_SET);
    v.resize ((size_t) n);
    if (n > 0 && fread (v.data(), 1, (size_t) n, fp) != (size_t) n) v.clear();
    fclose (fp);
    return v;
}

int main (int argc, char** argv)
{
    if (argc == 5 && ! strcmp (argv[1], "enc"))
    {
        const auto raw = readAll (argv[2]);
        const int ch = atoi (argv[3]);
        const int count = (int) (raw.size() / 2), frames = count / ch;
        std::vector<short> pcm ((size_t) count);
        memcpy (pcm.data(), raw.data(), (size_t) count * 2);
        std::vector<u8> out ((size_t) count * 3 + 4096);
        const int n = bankEncode (pcm.data(), frames, ch, out.data(), (int) out.size());
        if (n < 0) { fprintf (stderr, "encode failed\n"); return 1; }
        FILE* fp = fopen (argv[4], "wb"); fwrite (out.data(), 1, (size_t) n, fp); fclose (fp);
        printf ("%d %u\n", n, bankCrc (pcm.data(), count));
        return 0;
    }
    if (argc == 3 && ! strcmp (argv[1], "check"))
    {
        const std::string dir = argv[2];
        const auto idx = readAll (dir + "/chimera.idx");
        if (! bankIndex (idx.data(), (int) idx.size())) { printf ("FAIL index\n"); return 1; }
        std::vector<std::vector<unsigned char>> files;
        for (int k = 0;; ++k) { auto f = readAll (dir + "/chimera." + std::to_string (k) + ".bin"); if (f.empty()) break; files.push_back (std::move (f)); }
        const auto t0 = std::chrono::steady_clock::now();
        int ok = 0; long long values = 0;
        for (int i = 0; i < BANK.n; ++i)
        {
            const BankEntry& e = BANK.e[i];
            if (e.file < 0 || e.file >= (int) files.size() || e.foff + e.coded > (int) files[(size_t) e.file].size()) { printf ("FAIL %d: its file\n", i); continue; }
            if (bankDecodeNow (i, files[(size_t) e.file].data() + e.foff, e.coded)) { ++ok; values += (long long) e.frames * e.ch; }
            else printf ("FAIL %d %s: crc\n", i, e.name);
        }
        const double ms = std::chrono::duration<double, std::milli> (std::chrono::steady_clock::now() - t0).count();
        printf ("%s %d/%d samples, %d categories, %.1f MB in the pool (cap %.1f), decoded in %.0f ms\n", ok == BANK.n ? "PASS" : "FAIL",
                ok, BANK.n, BANK.ncat, values * 2 / 1048576.0, BANK_CAP * 2 / 1048576.0, ms);
        return ok == BANK.n ? 0 : 1;
    }
    printf ("usage: bank_tool enc <in.raw> <ch> <out.skc> | check <dir>\n");
    return 1;
}
