/*  pd_test.cpp  --  Pulsedrive test: capture system audio and print beats
 *
 *  Usage:
 *    ./build/pd_test
 *
 *  Play music on your computer, run this, and watch beat detections.
 */
#include "pulsedrive.h"
#include <stdio.h>
#include <math.h>

#ifdef _WIN32
#include <windows.h>
#define pd_sleep_ms(ms) Sleep(ms)
#else
#include <unistd.h>
#define pd_sleep_ms(ms) usleep((ms) * 1000)
#endif

int main(int argc, char *argv[])
{
    (void)argc; (void)argv;

    printf("=== Pulsedrive Test ===\n");
    printf("Play music on your computer. Beats will be detected.\n");
    printf("Press Ctrl+C to stop.\n\n");

    pd_init(NULL);

    if (!pd_start_audio_capture()) {
        printf("ERROR: Failed to start audio capture.\n");
        printf("On macOS, you may need to grant 'Screen Recording' permission.\n");
        pd_shutdown();
        return 1;
    }

    printf("Capturing audio... waiting for beats.\n\n");

    float dt = 1.0f / 60.0f;  /* simulate 60fps game loop */
    int beat_count = 0;

    for (;;) {
        PulseState s = pd_update(dt);

        if (s.beat_onset) {
            beat_count++;
            printf("BEAT #%d  |  BPM: %.1f  |  Intensity: %.2f  |  "
                   "Confidence: %.2f  |  Bar: %d/4  |  "
                   "Bass: %.2f  Mid: %.2f  High: %.2f\n",
                   beat_count, s.bpm, s.intensity, s.confidence,
                   s.bar_position + 1,
                   s.band_bass, s.band_mid, s.band_high);
        }

        if (s.section_change) {
            printf(">>> SECTION CHANGE  |  Energy: %.2f → %.2f <<<\n",
                   s.section_energy, s.intensity);
        }

        pd_sleep_ms(16);  /* ~60fps */
    }

    pd_stop_audio_capture();
    pd_shutdown();
    return 0;
}
