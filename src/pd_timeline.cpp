/*  pd_timeline.cpp  --  Beat tracking, BPM estimation, phase prediction
 *
 *  Combines onset detections into a stable BPM estimate, tracks beat
 *  phase for smooth game integration, and predicts upcoming beats.
 */
#include "pd_internal.h"
#include <math.h>

/* ================================================================== */
/*  BPM estimation via inter-onset intervals                           */
/* ================================================================== */

static void estimate_bpm(PdBeatDetector *bd)
{
    /* Autocorrelation of onset values to find periodicity.
     * Look for peaks in the lag range corresponding to min-max BPM. */
    int n = PD_ONSET_HIST;
    float hop_sec = (float)bd->hop_size / (float)bd->sample_rate;

    /* BPM range → lag range in onset-history indices */
    int min_lag = (int)(60.0f / (bd->max_bpm * hop_sec));
    int max_lag = (int)(60.0f / (bd->min_bpm * hop_sec));
    if (min_lag < 1) min_lag = 1;
    if (max_lag >= n / 2) max_lag = n / 2 - 1;

    float best_corr = 0.0f;
    int   best_lag  = 0;

    for (int lag = min_lag; lag <= max_lag; lag++) {
        float corr = 0.0f;
        for (int i = 0; i < n - lag; i++) {
            int idx_a = (bd->onset_idx + i) % n;
            int idx_b = (bd->onset_idx + i + lag) % n;
            corr += bd->onset_values[idx_a] * bd->onset_values[idx_b];
        }
        if (corr > best_corr) {
            best_corr = corr;
            best_lag  = lag;
        }
    }

    if (best_lag > 0 && best_corr > 0.0f) {
        float beat_period = (float)best_lag * hop_sec;
        float new_bpm = 60.0f / beat_period;

        /* Add to BPM history for smoothing */
        bd->bpm_history[bd->bpm_hist_idx] = new_bpm;
        bd->bpm_hist_idx = (bd->bpm_hist_idx + 1) % PD_BPM_HIST;
        if (bd->bpm_hist_count < PD_BPM_HIST) bd->bpm_hist_count++;

        /* Median BPM from history (robust to outliers) */
        float sorted[PD_BPM_HIST];
        int count = bd->bpm_hist_count;
        for (int i = 0; i < count; i++) sorted[i] = bd->bpm_history[i];

        /* Simple insertion sort (tiny array) */
        for (int i = 1; i < count; i++) {
            float key = sorted[i];
            int j = i - 1;
            while (j >= 0 && sorted[j] > key) {
                sorted[j + 1] = sorted[j];
                j--;
            }
            sorted[j + 1] = key;
        }

        bd->bpm = sorted[count / 2];  /* median */
        bd->beat_period = 60.0f / bd->bpm;

        /* Confidence based on consistency of BPM history */
        if (count >= 4) {
            float variance = 0.0f;
            for (int i = 0; i < count; i++) {
                float d = bd->bpm_history[i] - bd->bpm;
                variance += d * d;
            }
            variance /= (float)count;
            /* Low variance = high confidence */
            bd->bpm_confidence = 1.0f / (1.0f + variance * 0.01f);
        }
    }
}

/* ================================================================== */
/*  Timeline update — called every frame                               */
/* ================================================================== */

void pd_timeline_update(PdState *state, float dt)
{
    PdBeatDetector *bd = &state->beat;

    /* Periodically re-estimate BPM */
    static float bpm_timer = 0.0f;
    bpm_timer += dt;
    if (bpm_timer >= 0.5f) {  /* every 500ms */
        estimate_bpm(bd);
        bpm_timer = 0.0f;
    }

    /* Beat phase tracking */
    if (bd->beat_period > 0.0f) {
        bd->beat_timer += dt;

        /* Phase = position within current beat (0.0-1.0) */
        bd->beat_phase = bd->beat_timer / bd->beat_period;
        if (bd->beat_phase > 1.0f) bd->beat_phase = 1.0f;

        /* Check for beat onset (predicted or detected) */
        bool predicted_onset = (bd->beat_timer >= bd->beat_period);

        /* Snap to detected onset if close to predicted beat */
        bool detected = pd_beat_check_onset(bd);
        if (detected) {
            /* Allow onset to nudge the beat timer for phase correction */
            float phase_error = bd->beat_timer / bd->beat_period;
            if (phase_error > 0.7f || phase_error < 0.3f) {
                /* Close enough to expected beat — sync */
                bd->beat_timer = 0.0f;
                state->output.beat_onset = true;

                /* Advance bar position */
                bd->bar_position = (bd->bar_position + 1) % 4;
                state->output.bar_onset = (bd->bar_position == 0);
                return;
            }
        }

        if (predicted_onset) {
            bd->beat_timer -= bd->beat_period;
            state->output.beat_onset = true;

            bd->bar_position = (bd->bar_position + 1) % 4;
            state->output.bar_onset = (bd->bar_position == 0);
        } else {
            state->output.beat_onset = false;
            state->output.bar_onset  = false;
        }
    } else {
        state->output.beat_onset = false;
        state->output.bar_onset  = false;
        bd->beat_phase = 0.0f;
    }
}
