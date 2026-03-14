/*  pd_internal.h  --  Shared internal state and declarations
 *  Not part of the public API.
 */
#pragma once

#include "pulsedrive.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================== */
/*  Ring buffer for audio samples                                      */
/* ================================================================== */
#define PD_RING_SIZE  (1 << 16)  /* 65536 samples (~1.5s at 44.1kHz) */
#define PD_RING_MASK  (PD_RING_SIZE - 1)

typedef struct PdRingBuffer {
    float   samples[PD_RING_SIZE];
    int     write_pos;     /* next write position (atomic from capture thread) */
    int     read_pos;      /* consumer read position */
} PdRingBuffer;

/* ================================================================== */
/*  Beat detector state                                                */
/* ================================================================== */
#define PD_MAX_FFT     4096
#define PD_ONSET_HIST  128
#define PD_BPM_HIST    16

typedef struct PdBeatDetector {
    /* FFT */
    int   fft_size;
    int   sample_rate;
    float window[PD_MAX_FFT];       /* Hann window */
    float prev_spectrum[PD_MAX_FFT / 2 + 1];

    /* Onset detection */
    float onset_values[PD_ONSET_HIST];
    int   onset_idx;
    float onset_threshold;
    float onset_mean;
    bool  onset_active;

    /* BPM estimation */
    float bpm;
    float bpm_confidence;
    float beat_period;      /* seconds per beat */
    float beat_timer;       /* time since last beat */
    float beat_phase;       /* 0.0-1.0 within current beat */
    int   bar_position;     /* 0-3 for 4/4 time */
    float min_bpm;
    float max_bpm;

    /* BPM history for smoothing */
    float bpm_history[PD_BPM_HIST];
    int   bpm_hist_idx;
    int   bpm_hist_count;

    /* Frame buffer for FFT input */
    float frame[PD_MAX_FFT];
    int   frame_pos;
    int   hop_size;
} PdBeatDetector;

/* ================================================================== */
/*  Intensity analyzer                                                 */
/* ================================================================== */

typedef struct PdIntensity {
    float sub_bass;     /* 20-60 Hz    */
    float bass;         /* 60-250 Hz   */
    float mid;          /* 250-4000 Hz */
    float high;         /* 4000+ Hz    */
    float overall;      /* weighted blend */

    /* Smoothed versions (EMA) */
    float smooth_overall;
    float smooth_sub_bass;
    float smooth_bass;
    float smooth_mid;
    float smooth_high;

    /* Section detection */
    float section_energy;
    float prev_section_energy;
    bool  section_changed;
} PdIntensity;

/* ================================================================== */
/*  Global state                                                       */
/* ================================================================== */

typedef struct PdState {
    bool            initialized;
    PulseConfig     config;
    PdRingBuffer    ring;
    PdBeatDetector  beat;
    PdIntensity     intensity;
    PulseState      output;

    /* Audio capture */
    bool            capturing;
    void           *capture_handle;  /* platform-specific handle */

    /* Spotify */
#ifdef PD_SPOTIFY_ENABLED
    int             spotify_status;
    char            spotify_client_id[128];
    int             spotify_redirect_port;
    char            spotify_access_token[512];
    char            spotify_refresh_token[512];
    float           spotify_token_expiry;
    float           spotify_time;
#endif
} PdState;

/* Global state (single instance) */
extern PdState g_pd;

/* ================================================================== */
/*  Internal functions                                                 */
/* ================================================================== */

/* Beat detection */
void pd_beat_init(PdBeatDetector *bd, const PulseConfig *cfg);
void pd_beat_process(PdBeatDetector *bd, const float *samples, int count);
bool pd_beat_check_onset(PdBeatDetector *bd);

/* Intensity */
void pd_intensity_init(PdIntensity *it);
void pd_intensity_update(PdIntensity *it, const float *spectrum,
                          int spec_size, int sample_rate, float dt);

/* Audio capture (platform-specific) */
bool pd_capture_start(PdState *state);
void pd_capture_stop(PdState *state);

/* Timeline (beat tracking + prediction) */
void pd_timeline_update(PdState *state, float dt);

#ifdef __cplusplus
}
#endif
