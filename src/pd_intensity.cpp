/*  pd_intensity.cpp  --  Frequency band energy analysis
 *
 *  Splits the spectrum into 4 bands (sub-bass, bass, mid, high),
 *  computes energy per band, smooths with EMA, and detects section
 *  changes from sustained energy shifts.
 */
#include "pd_internal.h"
#include <math.h>
#include <string.h>

void pd_intensity_init(PdIntensity *it)
{
    memset(it, 0, sizeof(*it));
}

void pd_intensity_update(PdIntensity *it, const float *spectrum,
                          int spec_size, int sample_rate, float dt)
{
    if (spec_size <= 0 || sample_rate <= 0) return;

    float freq_per_bin = (float)sample_rate / (float)((spec_size - 1) * 2);

    /* Compute band energies */
    float e_sub = 0.0f, e_bass = 0.0f, e_mid = 0.0f, e_high = 0.0f;
    int   n_sub = 0,    n_bass = 0,    n_mid = 0,    n_high = 0;

    for (int i = 1; i < spec_size; i++) {
        float freq = (float)i * freq_per_bin;
        float mag  = spectrum[i] * spectrum[i];  /* power spectrum */

        if (freq < 60.0f) {
            e_sub += mag; n_sub++;
        } else if (freq < 250.0f) {
            e_bass += mag; n_bass++;
        } else if (freq < 4000.0f) {
            e_mid += mag; n_mid++;
        } else {
            e_high += mag; n_high++;
        }
    }

    /* Normalize to mean energy per band */
    it->sub_bass = (n_sub > 0)  ? sqrtf(e_sub / (float)n_sub)   : 0.0f;
    it->bass     = (n_bass > 0) ? sqrtf(e_bass / (float)n_bass) : 0.0f;
    it->mid      = (n_mid > 0)  ? sqrtf(e_mid / (float)n_mid)   : 0.0f;
    it->high     = (n_high > 0) ? sqrtf(e_high / (float)n_high) : 0.0f;

    /* Overall intensity: weighted blend (bass-heavy for game feel) */
    it->overall = it->sub_bass * 0.3f + it->bass * 0.3f
                + it->mid * 0.25f + it->high * 0.15f;

    /* Normalize to roughly 0-1 range (auto-gain) */
    static float peak = 0.001f;
    if (it->overall > peak) peak = it->overall;
    peak *= 0.9999f;  /* slowly decay peak for auto-gain */
    if (peak > 0.001f) {
        it->overall  /= peak;
        it->sub_bass /= peak;
        it->bass     /= peak;
        it->mid      /= peak;
        it->high     /= peak;
    }

    /* Clamp */
    if (it->overall > 1.0f)  it->overall  = 1.0f;
    if (it->sub_bass > 1.0f) it->sub_bass = 1.0f;
    if (it->bass > 1.0f)     it->bass     = 1.0f;
    if (it->mid > 1.0f)      it->mid      = 1.0f;
    if (it->high > 1.0f)     it->high     = 1.0f;

    /* EMA smoothing (higher alpha = more responsive) */
    float alpha = 1.0f - expf(-dt * 8.0f);  /* ~8Hz responsiveness */
    it->smooth_overall  += (it->overall  - it->smooth_overall)  * alpha;
    it->smooth_sub_bass += (it->sub_bass - it->smooth_sub_bass) * alpha;
    it->smooth_bass     += (it->bass     - it->smooth_bass)     * alpha;
    it->smooth_mid      += (it->mid      - it->smooth_mid)      * alpha;
    it->smooth_high     += (it->high     - it->smooth_high)     * alpha;

    /* Section detection: sustained energy change */
    float slow_alpha = 1.0f - expf(-dt * 0.5f);  /* ~0.5Hz, very slow */
    it->section_energy += (it->smooth_overall - it->section_energy) * slow_alpha;

    float section_diff = fabsf(it->section_energy - it->prev_section_energy);
    if (section_diff > 0.15f) {
        it->section_changed = true;
        it->prev_section_energy = it->section_energy;
    }
}
