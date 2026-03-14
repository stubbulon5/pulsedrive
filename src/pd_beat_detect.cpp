/*  pd_beat_detect.cpp  --  Real-time beat detection via spectral flux
 *
 *  Algorithm:
 *    1. Windowed FFT (Hann window) on overlapping frames
 *    2. Spectral flux: sum of positive magnitude differences vs previous frame
 *    3. Adaptive threshold: onset when flux > mean * threshold_mult
 *    4. BPM estimation: autocorrelation of onset function
 *    5. Beat tracking: phase-locked to estimated BPM
 */
#include "pd_internal.h"
#include "kiss_fft.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

/* ================================================================== */
/*  Init                                                               */
/* ================================================================== */

void pd_beat_init(PdBeatDetector *bd, const PulseConfig *cfg)
{
    memset(bd, 0, sizeof(*bd));
    bd->fft_size     = cfg->fft_size;
    bd->sample_rate  = cfg->sample_rate;
    bd->min_bpm      = cfg->min_bpm;
    bd->max_bpm      = cfg->max_bpm;
    bd->onset_threshold = cfg->onset_threshold;
    bd->hop_size     = bd->fft_size / 4;  /* 75% overlap */

    /* Pre-compute Hann window */
    for (int i = 0; i < bd->fft_size; i++) {
        float t = (float)i / (float)(bd->fft_size - 1);
        bd->window[i] = 0.5f * (1.0f - cosf(2.0f * 3.14159265f * t));
    }
}

/* ================================================================== */
/*  Process audio samples                                              */
/* ================================================================== */

void pd_beat_process(PdBeatDetector *bd, const float *samples, int count)
{
    /* Accumulate samples into frame buffer */
    for (int i = 0; i < count; i++) {
        bd->frame[bd->frame_pos++] = samples[i];

        /* When we have a full frame, run FFT */
        if (bd->frame_pos >= bd->fft_size) {
            /* Apply window and prepare FFT input */
            int n = bd->fft_size;
            kiss_fft_cfg fft_cfg = kiss_fft_alloc(n, 0, NULL, NULL);
            if (!fft_cfg) {
                bd->frame_pos = 0;
                continue;
            }

            kiss_fft_cpx *fin  = (kiss_fft_cpx *)calloc(n, sizeof(kiss_fft_cpx));
            kiss_fft_cpx *fout = (kiss_fft_cpx *)calloc(n, sizeof(kiss_fft_cpx));

            for (int j = 0; j < n; j++) {
                fin[j].r = bd->frame[j] * bd->window[j];
                fin[j].i = 0.0f;
            }

            kiss_fft(fft_cfg, fin, fout);

            /* Compute magnitude spectrum */
            int spec_size = n / 2 + 1;
            float spectrum[PD_MAX_FFT / 2 + 1];
            for (int j = 0; j < spec_size; j++) {
                spectrum[j] = sqrtf(fout[j].r * fout[j].r + fout[j].i * fout[j].i);
            }

            /* Spectral flux: sum of positive differences */
            float flux = 0.0f;
            for (int j = 0; j < spec_size; j++) {
                float diff = spectrum[j] - bd->prev_spectrum[j];
                if (diff > 0.0f) flux += diff;
            }

            /* Update onset history */
            bd->onset_values[bd->onset_idx] = flux;
            bd->onset_idx = (bd->onset_idx + 1) % PD_ONSET_HIST;

            /* Running mean of onset values */
            float sum = 0.0f;
            int count_valid = 0;
            for (int j = 0; j < PD_ONSET_HIST; j++) {
                sum += bd->onset_values[j];
                if (bd->onset_values[j] > 0.0f) count_valid++;
            }
            bd->onset_mean = (count_valid > 0) ? sum / (float)count_valid : 0.0f;

            /* Onset detection */
            bd->onset_active = (flux > bd->onset_mean * bd->onset_threshold)
                            && (bd->onset_mean > 0.001f);

            /* Update intensity analyzer with current spectrum */
            pd_intensity_update(&g_pd.intensity, spectrum, spec_size,
                                 bd->sample_rate, (float)bd->hop_size / (float)bd->sample_rate);

            /* Store spectrum for next frame's flux calculation */
            memcpy(bd->prev_spectrum, spectrum, spec_size * sizeof(float));

            free(fin);
            free(fout);
            free(fft_cfg);

            /* Shift frame buffer by hop_size (overlap) */
            int remain = bd->fft_size - bd->hop_size;
            memmove(bd->frame, bd->frame + bd->hop_size, remain * sizeof(float));
            bd->frame_pos = remain;
        }
    }
}

bool pd_beat_check_onset(PdBeatDetector *bd)
{
    return bd->onset_active;
}
