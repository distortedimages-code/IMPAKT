// IMPAKT — WebAssembly exports (no libc). Built with clang --target=wasm32.
#include "sk_engine.h"

static sk::Engine E;

#define EXPORT(name) extern "C" __attribute__((export_name(#name)))

EXPORT(sk_init)        void   sk_init (float sr)                         { E.init (sr); }
EXPORT(sk_init_lite)   void   sk_init_lite (float sr)                    { E.init (sr, 1); }
EXPORT(sk_set_param)   void   sk_set_param (int t, int p, float v)       { E.setParam (t, p, v); }
EXPORT(sk_set_trig)    void   sk_set_trig (int t, int s, int on)         { E.setTrig (t, s, on); }
EXPORT(sk_set_tlk)     void   sk_set_tlk (int t, int s, int on)          { E.setTrigless (t, s, on); }
EXPORT(sk_set_lock)    void   sk_set_lock (int t, int s, int p, float v) { E.setLock (t, s, p, v); }
EXPORT(sk_clear_locks) void   sk_clear_locks (int t, int s)              { E.clearLocks (t, s); }
EXPORT(sk_set_ratchet) void   sk_set_ratchet (int t, int s, int n)       { E.setRatchet (t, s, n); }
EXPORT(sk_set_len)     void   sk_set_len (int t, int l)                  { E.setLen (t, l); }
EXPORT(sk_set_speed)   void   sk_set_speed (int t, int i)                { E.setSpeed (t, i); }
EXPORT(sk_set_mute)    void   sk_set_mute (int t, int m)                 { E.setMute (t, m); }
EXPORT(sk_set_global)  void   sk_set_global (int g, float v)             { E.setGlobal (g, v); }
EXPORT(sk_set_play)    void   sk_set_play (int on)                       { E.setPlay (on); }
EXPORT(sk_audition)    void   sk_audition (int t, float v, float n)      { E.audition (t, v, n); }
EXPORT(sk_reset)       void   sk_reset()                                 { E.resetPattern(); }
EXPORT(sk_release)     void   sk_release (int t, float n)                { E.release (t, n); }
EXPORT(sk_ribbon)      void   sk_ribbon (int t, float s, int g, float e) { E.ribbon (t, s, g, e); }
EXPORT(sk_touch)       void   sk_touch (int t, int k, float p)          { E.mantraTouch (t, k, p); }   // (v84: MANTRA's pads)
// the notes the quantiser holds to now (its scale, or the chord of the track it follows): pitch classes, bit 0 = C
EXPORT(sk_qmask)       int    sk_qmask()                                 { return (int) E.quantMask(); }
EXPORT(sk_side_def)    float  sk_side_def (int m, int i)                 { float d[sk::NUM_SIDES]; const int k = sk::machineSideDefaults (m, d); return i >= 0 && i < k ? d[i] : -1.0f; }
EXPORT(sk_af_def)      float  sk_af_def (int m, int i)                   { float d[6]; const int k = sk::machineAFDefaults (m, d); return i >= 0 && i < k ? d[i] : -1.0f; }
// patterns (sk_pat.cpp): the one playing, all emptied, the one edited, one emptied, a change, the chain, CHG; the state
EXPORT(sk_pat_cur)     void   sk_pat_cur (int i)                         { E.patCur (i); }
EXPORT(sk_pat_all)     void   sk_pat_all()                               { E.patAll(); }
EXPORT(sk_pat_edit)    void   sk_pat_edit (int i)                        { E.patEdit (i); }
EXPORT(sk_pat_clear)   void   sk_pat_clear (int i)                       { E.patClear (i); }
EXPORT(sk_pat_queue)   void   sk_pat_queue (int i, int mode)             { E.patQueue (i, mode); }
EXPORT(sk_pat_chain)   void   sk_pat_chain (int k, int i)                { E.patChain (k, i); }
EXPORT(sk_pat_chainlen) void  sk_pat_chainlen (int n)                    { E.patChainLen (n); }
EXPORT(sk_pat_chg)     void   sk_pat_chg (int n)                         { E.patChg (n); }
EXPORT(sk_pat_info)    const float* sk_pat_info()                        { return E.getPatInfo(); }
EXPORT(sk_panic)       void   sk_panic()                                 { E.panic(); }
// the page writes its input (or silence) into sk_in_l / sk_in_r before each sk_process
EXPORT(sk_process)     void   sk_process (int n)                         { E.setInput (E.inBufL, E.inBufR); E.process (E.outL, E.outR, n > 4096 ? 4096 : n, 0); }
EXPORT(sk_out_l)       float* sk_out_l()                                 { return E.outL; }
EXPORT(sk_out_r)       float* sk_out_r()                                 { return E.outR; }
EXPORT(sk_in_l)        float* sk_in_l()                                  { return E.inBufL; }
EXPORT(sk_in_r)        float* sk_in_r()                                  { return E.inBufR; }
// RAM machine: REC trigs, a take now, the buffers (16-bit, RAM_LEN samples each) and what the page shows of them
EXPORT(sk_set_rec)     void   sk_set_rec (int t, int s, int on)          { E.setRec (t, s, on); }
EXPORT(sk_ram_start)   void   sk_ram_start (int t)                       { E.ramStart (t, -1); }
EXPORT(sk_ram_mem)     short* sk_ram_mem (int t)                         { return E.ramMem[t < 0 ? 0 : (t >= sk::NUM_TRACKS ? sk::NUM_TRACKS - 1 : t)]; }
EXPORT(sk_ram_cap)     int    sk_ram_cap()                               { return sk::RAM_LEN; }
EXPORT(sk_ram_loaded)  void   sk_ram_loaded (int t, int len, float rate) { E.ramLoaded (t, len, rate); }
EXPORT(sk_ram_clear)   void   sk_ram_clear (int t)                       { E.ramClear (t); }
EXPORT(sk_ram_copy)    void   sk_ram_copy (int a, int b)                 { E.ramCopy (a, b); }
EXPORT(sk_ram_swap)    void   sk_ram_swap (int a, int b)                 { E.ramSwap (a, b); }
static float RAMINFO[sk::NUM_TRACKS * sk::Engine::RAM_INFO];
EXPORT(sk_ram_info)    const float* sk_ram_info()                        { E.ramInfo (RAMINFO); return RAMINFO; }
EXPORT(sk_prism_info)  const float* sk_prism_info (int t)                { return E.prismInfo (t); }
EXPORT(sk_ram_peaks)   const float* sk_ram_peaks (int t)                 { return E.ram[t < 0 ? 0 : (t >= sk::NUM_TRACKS ? sk::NUM_TRACKS - 1 : t)].pk; }
EXPORT(sk_get_step)    int    sk_get_step (int t)                        { return E.getStep (t); }
EXPORT(sk_get_hits)    int    sk_get_hits (int t)                        { return E.getHits (t); }
EXPORT(sk_running)     int    sk_running()                               { return E.running; }
EXPORT(sk_meter_gr)    float  sk_meter_gr()                              { return E.meterGr(); }
EXPORT(sk_meter_out)   float  sk_meter_out()                             { return E.meterOut(); }
EXPORT(sk_meter_reset) void   sk_meter_reset()                           { E.meterReset(); }
EXPORT(sk_set_lite)    void   sk_set_lite (int on)                       { E.lite = on ? 1 : 0; }
EXPORT(sk_set_scale)   void   sk_set_scale (int mask)                    { E.setScale ((unsigned) mask); }
EXPORT(sk_set_qtrk)    void   sk_set_qtrk (int t, int on)                { E.setQuant (t, on); }
static float MIXINFO[2 * sk::CALM_K + sk::NUM_TRACKS * sk::UM_BANDS];
EXPORT(sk_mix_info)    float* sk_mix_info()                              { E.mixInfo (MIXINFO); return MIXINFO; }
EXPORT(sk_loop_info)   const float* sk_loop_info()                       { return E.getLoopInfo(); }
EXPORT(sk_set_spec)    void   sk_set_spec (int t)                        { E.setSpecTrack (t); }
EXPORT(sk_spec_info)   const float* sk_spec_info()                       { return E.specInfo(); }
// the screen's modulation view: per track, LFO 1 / 2 (phase, value) and EF 1 / 2 (value, band input level)
static float MODINFO[8 * sk::NUM_TRACKS];
EXPORT(sk_mod_info)    const float* sk_mod_info()                        { E.modInfo (MODINFO); return MODINFO; }
// GRAIN's LFO (phase, value) and OTT's gain change per band (dB), GRAIN on, OTT on
static float FXINFO[7];
EXPORT(sk_fx_info)     const float* sk_fx_info()
{
    FXINFO[0] = (float) E.glfoPh; FXINFO[1] = E.glfoVal;
    for (int b = 0; b < 3; ++b) FXINFO[2 + b] = E.ott.gainDb[b];
    FXINFO[5] = E.g[sk::G_GON] > 0.5f ? 1.0f : 0.0f; FXINFO[6] = E.g[sk::G_OON] > 0.5f ? 1.0f : 0.0f;
    return FXINFO;
}
// GLITCH: its last 16 divisions, the current one's progress and detail, the codec's bandwidth, on
static float GLINFO[22];
static float HALOINFO[sk::HALO_INFO];
static float BRKINFO[sk::BRK_INFO];
// BREAK's slots: the page writes a loop into its slot (sk_break_mem, at most sk_break_cap samples), then hands it over
static float BRKSLOTS[1 + sk::BRK_SLOTS * sk::BRK_SLOT_INFO];
static float BRKPEAKS[sk::BRK_NPEAK];
EXPORT(sk_break_mem)   short* sk_break_mem (int s)                       { return E.brk.mem (s < 0 ? 0 : (s >= sk::BRK_SLOTS ? sk::BRK_SLOTS - 1 : s)); }
EXPORT(sk_break_cap)   int    sk_break_cap()                             { return sk::BRK_CAP; }
EXPORT(sk_break_set)   void   sk_break_set (int s, int n, int bars)      { E.breakSet (s, n, bars); }
EXPORT(sk_break_clear) void   sk_break_clear (int s)                     { E.breakClear (s); }
EXPORT(sk_break_bars)  void   sk_break_bars (int s, int bars)            { E.breakBars (s, bars); }
EXPORT(sk_break_ver)   int    sk_break_ver()                             { return E.brk.ver; }
EXPORT(sk_break_slots) const float* sk_break_slots()                     { E.breakSlots (BRKSLOTS); return BRKSLOTS; }
EXPORT(sk_break_peaks) const float* sk_break_peaks (int s)               { E.breakPeaks (s, BRKPEAKS); return BRKPEAKS; }
EXPORT(sk_glitch_info) const float* sk_glitch_info()                     { E.glitchInfo (GLINFO); return GLINFO; }
EXPORT(sk_halo_info)   const float* sk_halo_info (int t)                 { E.haloInfo (t, HALOINFO); return HALOINFO; }
EXPORT(sk_break_info)  const float* sk_break_info (int t)                { E.breakInfo (t, BRKINFO); return BRKINFO; }
// an FX track: its machine, what it takes, its gate and gesture, its levels (peaks since the last call), its readouts
static float FXMINFO[sk::FX_INFO];
EXPORT(sk_fxm_info)    const float* sk_fxm_info (int t)                  { E.fxInfo (t, FXMINFO); return FXMINFO; }

// CHIMERA's bank (sk_bank.h): the page writes the index, then each sample's coded bytes, into the stage; the engine
// decodes a sample over the next blocks (sk_bank_step, a few thousand values a call: the audio goes on meanwhile)
static float CHIMINFO[sk::CHIM_INFO];
EXPORT(sk_bank_stage)  unsigned char* sk_bank_stage()                    { return sk::BANK_STAGE_MEM; }
EXPORT(sk_bank_cap)    int    sk_bank_cap()                              { return sk::BANK_STAGE; }
EXPORT(sk_bank_index)  int    sk_bank_index (int len)                    { return sk::bankIndex (sk::BANK_STAGE_MEM, len) ? sk::BANK.n : -1; }
EXPORT(sk_bank_feed)   void   sk_bank_feed (int i, int len)              { sk::bankFeed (i, sk::BANK_STAGE_MEM, len); }
EXPORT(sk_bank_step)   int    sk_bank_step (int maxValues)               { return sk::bankStep (maxValues); }
EXPORT(sk_chim_info)   const float* sk_chim_info (int t)                 { E.chimInfo (t, CHIMINFO); return CHIMINFO; }

// CUBE's banks (v74, sk_cube.h): the page writes a bank (64 waves of 256 samples, 16-bit) into the stage, then hands it
// over to a bank (Z, 0..7); the engine makes its band-limited versions over the next blocks (FACT's bank plays meanwhile)
static float CUBEINFO[sk::CUBE_INFO];
EXPORT(sk_cube_mem)    short* sk_cube_mem()                              { return E.cube.stage; }
EXPORT(sk_cube_cap)    int    sk_cube_cap()                              { return sk::CUBE_BANK_N; }
EXPORT(sk_cube_set)    void   sk_cube_set (int bank, int n)              { E.cubeSetBank (bank, E.cube.stage, n); }
EXPORT(sk_cube_clear)  void   sk_cube_clear (int bank)                   { E.cubeClear (bank); }
EXPORT(sk_cube_ver)    int    sk_cube_ver()                              { return E.cube.ver; }
EXPORT(sk_cube_info)   const float* sk_cube_info (int t)                 { E.cubeInfo (t, CUBEINFO); return CUBEINFO; }

// WEST (v76): the track's light, envelope, step, pulses, folder, gate (sk_west.h)
static float WSINFO[sk::WS_INFO];
EXPORT(sk_west_info) const float* sk_west_info (int t)                   { E.westInfo (t, WSINFO); return WSINFO; }

// RADIO (v87, sk_radio.h): the page hands each receiver what it fetched (an MP3 stream's bytes, a shortwave receiver's
// IMA ADPCM or 16-bit sound) through the stage; the decoders here make it mono at RADIO_RATE for the engine
#include "sk_radio_dec.h"
static sk::RadioDec RDEC[sk::RADIO_RX];
static int RDEC_READY = 0;
static unsigned char RSTAGE[65536];
static short RPCM[4096];
static float RADINFO[sk::RADIO_INFO];
static float RADRX[sk::RADIO_RX * sk::RADIO_RX_INFO];
static sk::RadioDec& rdec (int k)
{
    if (! RDEC_READY) { for (auto& d : RDEC) d.reset(); RDEC_READY = 1; }
    return RDEC[k < 0 ? 0 : (k >= sk::RADIO_RX ? sk::RADIO_RX - 1 : k)];
}
static void rdrain (int k)
{
    sk::RadioDec& d = rdec (k);
    for (;;)
    {
        const int n = d.take (RPCM, 4096);
        if (n <= 0) break;
        E.radioPush (k, RPCM, n);           // (what does not fit is dropped: the engine waits for nothing)
    }
}
EXPORT(sk_radio_stage) unsigned char* sk_radio_stage()                   { return RSTAGE; }
EXPORT(sk_radio_cap)   int    sk_radio_cap()                             { return (int) sizeof (RSTAGE); }
EXPORT(sk_radio_mp3)   void   sk_radio_mp3 (int k, int n)
{
    sk::RadioDec& d = rdec (k);
    int off = 0;
    n = n < 0 ? 0 : (n > (int) sizeof (RSTAGE) ? (int) sizeof (RSTAGE) : n);
    while (off < n)
    {
        const int m = n - off < d.inRoom() ? n - off : d.inRoom();
        if (m > 0) { d.mp3Feed (RSTAGE + off, m); off += m; }
        rdrain (k);
        while (d.mp3Work()) rdrain (k);
        if (m <= 0 && d.inRoom() <= 0) break;
    }
}
EXPORT(sk_radio_adpcm) void   sk_radio_adpcm (int k, int n, float rate)  { rdec (k).adpcmFeed (RSTAGE, n, rate); rdrain (k); }
EXPORT(sk_radio_pcm)   void   sk_radio_pcm (int k, int n, int ch, float rate) { rdec (k).pcmFeed ((const short*) RSTAGE, n, ch, rate); rdrain (k); }
EXPORT(sk_radio_flush) void   sk_radio_flush (int k)                     { rdec (k).restart(); E.radioFlush (k); }
EXPORT(sk_radio_state) void   sk_radio_state (int k, int st, int kind)   { E.radioSetState (k, st, kind); }
EXPORT(sk_radio_meter) void   sk_radio_meter (int k, float dbm)          { E.radioMeter (k, dbm); }
EXPORT(sk_radio_use)   int    sk_radio_use()                             { return E.radioUse(); }
EXPORT(sk_radio_info)  const float* sk_radio_info (int t)                { E.radioInfo (t, RADINFO); return RADINFO; }
EXPORT(sk_radio_rx)    const float* sk_radio_rx()                        { E.radioRxInfo (RADRX); return RADRX; }
// (what the decoder found: rate, channels, kbit/s)
EXPORT(sk_radio_fmt)   int    sk_radio_fmt (int k, int what)             { sk::RadioDec& d = rdec (k); return what == 0 ? d.rate : what == 1 ? d.chans : d.kbps; }
