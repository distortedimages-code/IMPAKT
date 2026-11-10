// IMPAKT — the GRAIN LFO palette (tools/gen_lfo_tables.py writes sk_lfo_tables.cpp): 64 original waves,
// 8 families of 8, 256 points a cycle, -127..127 (unipolar waves 0..127).
#pragma once

namespace sk {

enum { WLFO_WAVES = 64, WLFO_LEN = 256, WLFO_UNI = 1, WLFO_STEP = 2 };
extern const signed char WLFO_TABLE[WLFO_WAVES][WLFO_LEN];
extern const unsigned char WLFO_FLAGS[WLFO_WAVES];

} // namespace sk
