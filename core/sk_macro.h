// IMPAKT — MACRO machine: the 24 models of Mutable Instruments Plaits 1.2 (MIT, © Emilie Gillet; the
// sources are in mi/plaits). One unit per track: it runs at 48 kHz like the module and is resampled to the
// host rate. The unit lives in a raw block owned by the track (MACRO_BYTES), so this header stays free of
// the Plaits headers.
#pragma once

namespace sk {

enum { MACRO_MODELS = 24, MACRO_BYTES = 81920 };

struct MacroControls
{
    int   model;       // 0..23, IMPAKT order: the 16 classic models, then the 8 of version 1.2
    float note;        // MIDI note (69 = 440 Hz)
    float harmonics, timbre, morph;
    float timbreEnv;   // -1..1: the internal envelope on TIMBRE (Plaits' TIMBRE attenuverter)
    float aux;         // 0 = OUT, 1 = AUX
    float decay;       // 0..1 (Plaits' DECAY)
    float colour;      // 0..1 (low-pass gate colour)
    float accent;      // 0..1
    int   hold;        // DEC on HOLD: the low-pass gate follows the gate (open while it is high), not a ping
};

void macroInit (void* mem, float hostSr);                 // mem: MACRO_BYTES, 16-byte aligned
void macroSetRate (void* mem, float hostSr);
void macroTrigger (void* mem, float gateSec);             // rising edge at the next block, gate high for gateSec
void macroRelease (void* mem);                            // the gate falls now
float macroRender (void* mem, const MacroControls& c, float* out, int n);   // n host-rate samples; returns the peak

} // namespace sk
