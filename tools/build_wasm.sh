#!/bin/sh
# Builds core/*.cpp into web/impakt.wasm (and web/impakt-simd.wasm) with plain clang (no Emscripten).
set -e
cd "$(dirname "$0")/.."
build () {   # $1 = output name, $2 = extra flags, $3 = memory (bytes)
    OUT=build/wasm$2
    OUT=$(echo "$OUT" | tr -d ' -')
    rm -rf "$OUT"
    mkdir -p "$OUT"
    FLAGS="--target=wasm32 -O3 -ffreestanding -fno-exceptions -fno-rtti -mnontrapping-fptoint -msign-ext -mbulk-memory $2"
    for f in sk_engine sk_kit sk_mod sk_glitch sk_halo sk_strum sk_choir sk_alloy sk_chimera sk_cube sk_west sk_howl sk_stencil sk_mantra sk_radio sk_radio_dec sk_spring sk_bank sk_break sk_fx sk_fuzz sk_send sk_box sk_spec sk_org sk_master sk_rev sk_vintage sk_mix sk_ott sk_lfo_tables sk_pat sk_wasm; do
        clang++ $FLAGS -std=c++17 -c core/$f.cpp -o "$OUT/$f.o"
    done
    # Mutable Instruments code (core/mi, MIT): MACRO (Plaits), GRAIN (Beads) and WEFT (Warps), with the small
    # freestanding <algorithm> / <cmath> of core/mi/compat
    MI="-Icore/mi/compat -Icore/mi"
    mkdir -p "$OUT/mi"
    for f in core/sk_macro.cpp core/sk_grain.cpp core/sk_weft.cpp $(find core/mi/plaits core/mi/beads core/mi/warps core/mi/stmlib -name '*.cc' ! -name atan.cc | sort); do
        [ -f "$f" ] || continue
        clang++ $FLAGS -std=c++17 $MI -c "$f" -o "$OUT/mi/$(echo "$f" | tr '/' '_').o"
    done
    clang $FLAGS -fno-builtin -c core/sk_rt.c -o "$OUT/sk_rt.o"
    wasm-ld --no-entry --strip-all --lto-O3 -O3 -z stack-size=262144 \
        --initial-memory=$3 --max-memory=$3 \
        "$OUT/sk_engine.o" "$OUT/sk_kit.o" "$OUT/sk_mod.o" "$OUT/sk_glitch.o" "$OUT/sk_halo.o" "$OUT/sk_strum.o" "$OUT/sk_choir.o" "$OUT/sk_alloy.o" "$OUT/sk_chimera.o" "$OUT/sk_cube.o" "$OUT/sk_west.o" "$OUT/sk_howl.o" "$OUT/sk_stencil.o" "$OUT/sk_mantra.o" "$OUT/sk_radio.o" "$OUT/sk_radio_dec.o" "$OUT/sk_spring.o" "$OUT/sk_bank.o" "$OUT/sk_break.o" "$OUT/sk_fx.o" "$OUT/sk_fuzz.o" "$OUT/sk_send.o" "$OUT/sk_box.o" "$OUT/sk_spec.o" "$OUT/sk_org.o" "$OUT/sk_master.o" "$OUT/sk_rev.o" "$OUT/sk_vintage.o" "$OUT/sk_mix.o" "$OUT/sk_ott.o" "$OUT/sk_lfo_tables.o" "$OUT/sk_pat.o" "$OUT/sk_wasm.o" "$OUT"/mi/*.o "$OUT/sk_rt.o" -o "web/$1"
    ls -l "web/$1"
}
# (224 MB: the engine, 122 MB since v87 (RADIO: 9 MB), and CHIMERA's bank, 84 MB; the lite build, for wasm2js, without
# the bank: 144 MB)
build impakt.wasm "" 234881024
build impakt-simd.wasm "-msimd128" 234881024
build impakt-lite.wasm "-DSK_BANK_LITE" 150994944
