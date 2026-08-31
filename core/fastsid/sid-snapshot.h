/* K4510 shim: VICE's save-state structures.  The K4510 has its own save
 * states (core/state.c) and does not use these, but fastsid.c compiles its
 * state_read/state_write against them, so the two it names are kept here,
 * copied from VICE 3.3 src/sid/sid-snapshot.h. */
#ifndef K4510_VICE_SID_SNAPSHOT_H
#define K4510_VICE_SID_SNAPSHOT_H
#include <stdint.h>
typedef struct sid_snapshot_state_s {
    uint8_t sid_register[0x20];
    uint8_t bus_value;
    uint32_t bus_value_ttl;
    uint32_t accumulator[3];
    uint32_t shift_register[3];
    uint16_t rate_counter[3];
    uint16_t rate_counter_period[3];
    uint16_t exponential_counter[3];
    uint16_t exponential_counter_period[3];
    uint8_t envelope_counter[3];
    uint8_t envelope_state[3];
    uint8_t hold_zero[3];
    uint8_t envelope_pipeline[3];
    uint8_t shift_pipeline[3];
    uint32_t shift_register_reset[3];
    uint32_t floating_output_ttl[3];
    uint16_t pulse_output[3];
    uint8_t write_pipeline;
    uint8_t write_address;
    uint8_t voice_mask;
} sid_snapshot_state_t;
typedef struct sid_fastsid_snapshot_state_s {
    uint32_t factor;
    uint8_t d[32];
    uint8_t has3;
    uint8_t vol;
    int32_t adrs[16];
    uint32_t sz[16];
    uint32_t speed1;
    uint8_t update;
    uint8_t newsid;
    uint8_t laststore;
    uint8_t laststorebit;
    uint32_t laststoreclk;
    uint32_t emulatefilter;
    float filterDy;
    float filterResDy;
    uint8_t filterType;
    uint8_t filterCurType;
    uint16_t filterValue;

    uint32_t v_nr[3];
    uint32_t v_f[3];
    uint32_t v_fs[3];
    uint8_t v_noise[3];
    uint32_t v_adsr[3];
    int32_t v_adsrs[3];
    uint32_t v_adsrz[3];
    uint8_t v_sync[3];
    uint8_t v_filter[3];
    uint8_t v_update[3];
    uint8_t v_gateflip[3];
    uint8_t v_adsrm[3];
    uint8_t v_attack[3];
    uint8_t v_decay[3];
    uint8_t v_sustain[3];
    uint8_t v_release[3];
    uint32_t v_rv[3];
    uint8_t v_wt[3];
    uint16_t v_wt_offset[3];
    uint32_t v_wtpf[3];
    uint32_t v_wtl[3];
    uint16_t v_wtr[2][3];
    uint8_t v_filtIO[3];
    float v_filtLow[3];
    float v_filtRef[3];
} sid_fastsid_snapshot_state_t;
#endif
