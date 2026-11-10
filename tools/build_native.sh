#!/bin/sh
# Native builds of the test tools (tools/ram_test.cpp, tools/render_test.cpp) into build/: the core once in build/nat
# (objects rebuilt only when their source is newer), then each tool linked against it. clang++ by default: the
# null tests compare renders bit for bit, and another compiler rounds differently.
set -e
cd "$(dirname "$0")/.."
OUT=build/nat
mkdir -p "$OUT/mi"
CXX=${CXX:-clang++}
FLAGS="-O2 -std=c++17 -Icore"
MI="-Icore/mi/compat -Icore/mi"
objs=""
for f in core/sk_engine.cpp core/sk_kit.cpp core/sk_mod.cpp core/sk_glitch.cpp core/sk_halo.cpp core/sk_strum.cpp core/sk_choir.cpp core/sk_alloy.cpp core/sk_chimera.cpp core/sk_cube.cpp core/sk_west.cpp core/sk_howl.cpp core/sk_stencil.cpp core/sk_mantra.cpp core/sk_radio.cpp core/sk_radio_dec.cpp core/sk_spring.cpp core/sk_weft.cpp core/sk_bank.cpp core/sk_break.cpp core/sk_fx.cpp core/sk_fuzz.cpp core/sk_send.cpp core/sk_box.cpp core/sk_spec.cpp core/sk_org.cpp \
         core/sk_master.cpp core/sk_rev.cpp core/sk_vintage.cpp core/sk_mix.cpp core/sk_macro.cpp core/sk_grain.cpp \
         core/sk_ott.cpp core/sk_lfo_tables.cpp core/sk_pat.cpp; do
    o="$OUT/$(basename "$f" .cpp).o"
    # (rebuilt when its source or any header of core/ is newer: the structs are shared)
    if [ ! -f "$o" ] || [ "$f" -nt "$o" ] || [ -n "$(find core -maxdepth 1 -name '*.h' -newer "$o" | head -1)" ]; then $CXX $FLAGS $MI -c "$f" -o "$o"; fi
    objs="$objs $o"
done
for f in $(find core/mi/plaits core/mi/beads core/mi/warps core/mi/stmlib -name '*.cc' ! -name atan.cc | sort); do
    o="$OUT/mi/$(echo "$f" | tr '/' '_').o"
    [ -f "$o" ] || $CXX $FLAGS $MI -w -c "$f" -o "$o"
    objs="$objs $o"
done
for t in ${@:-ram_test render_test}; do
    $CXX $FLAGS -w "tools/$t.cpp" $objs -o "build/$t"
    echo "build/$t"
done
