/*  pd_core.cpp  --  Pulsedrive lifecycle and main update loop
 */
#include "pd_internal.h"
#include <string.h>
#include <math.h>

/* Single global instance */
PdState g_pd;

/* ================================================================== */
/*  Init / Shutdown                                                    */
/* ================================================================== */

void pd_init(const PulseConfig *config)
{
    memset(&g_pd, 0, sizeof(g_pd));

    /* Apply config or defaults */
    if (config) {
        g_pd.config = *config;
    } else {
        g_pd.config.fft_size         = 2048;
        g_pd.config.sample_rate      = 44100;
        g_pd.config.min_bpm          = 60.0f;
        g_pd.config.max_bpm          = 200.0f;
        g_pd.config.onset_threshold  = 1.5f;
    }

    pd_beat_init(&g_pd.beat, &g_pd.config);
    pd_intensity_init(&g_pd.intensity);

    g_pd.output.time_remaining = -1.0f;
    g_pd.initialized = true;
}

void pd_shutdown(void)
{
    if (!g_pd.initialized) return;

    if (g_pd.capturing) {
        pd_capture_stop(&g_pd);
    }

    memset(&g_pd, 0, sizeof(g_pd));
}

/* ================================================================== */
/*  Main update — call every frame                                     */
/* ================================================================== */

PulseState pd_update(float dt)
{
    if (!g_pd.initialized) {
        PulseState empty;
        memset(&empty, 0, sizeof(empty));
        empty.time_remaining = -1.0f;
        return empty;
    }

    /* Drain ring buffer → beat detector */
    int write = g_pd.ring.write_pos;
    int read  = g_pd.ring.read_pos;
    int avail = (write - read) & PD_RING_MASK;

    if (avail > 0) {
        /* Process in chunks up to fft_size */
        float chunk[PD_MAX_FFT];
        while (avail > 0) {
            int n = avail > g_pd.config.fft_size ? g_pd.config.fft_size : avail;
            for (int i = 0; i < n; i++) {
                chunk[i] = g_pd.ring.samples[(read + i) & PD_RING_MASK];
            }
            pd_beat_process(&g_pd.beat, chunk, n);
            read = (read + n) & PD_RING_MASK;
            avail -= n;
        }
        g_pd.ring.read_pos = read;
    }

    /* Update timeline (beat tracking, prediction, phase) */
    pd_timeline_update(&g_pd, dt);

    /* Build output state */
    PulseState *out = &g_pd.output;
    out->bpm           = g_pd.beat.bpm;
    out->confidence    = g_pd.beat.bpm_confidence;
    out->beat_phase    = g_pd.beat.beat_phase;
    out->bar_position  = g_pd.beat.bar_position;
    out->intensity     = g_pd.intensity.smooth_overall;
    out->band_sub_bass = g_pd.intensity.smooth_sub_bass;
    out->band_bass     = g_pd.intensity.smooth_bass;
    out->band_mid      = g_pd.intensity.smooth_mid;
    out->band_high     = g_pd.intensity.smooth_high;
    out->section_energy = g_pd.intensity.section_energy;
    out->section_change = g_pd.intensity.section_changed;

    /* Reset one-frame flags after reading */
    g_pd.intensity.section_changed = false;

    return *out;
}

/* ================================================================== */
/*  Audio capture public API                                           */
/* ================================================================== */

bool pd_start_audio_capture(void)
{
    if (!g_pd.initialized) return false;
    if (g_pd.capturing) return true;

    bool ok = pd_capture_start(&g_pd);
    g_pd.capturing = ok;
    return ok;
}

void pd_stop_audio_capture(void)
{
    if (!g_pd.initialized || !g_pd.capturing) return;
    pd_capture_stop(&g_pd);
    g_pd.capturing = false;
}

bool pd_is_capturing(void)
{
    return g_pd.capturing;
}
