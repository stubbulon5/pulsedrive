/*  pulsedrive.h  --  Music-reactive gameplay engine
 *
 *  Standalone C API for syncing game events to music.  Two modes:
 *
 *    1. Spotify Integration (Mode A)
 *       Pre-computed beat timestamps, section boundaries, tempo data.
 *       Perfect sync, zero latency.  Requires Spotify Premium.
 *
 *    2. System Audio Capture (Mode B)
 *       Real-time FFT + onset detection from whatever is playing.
 *       Works with any audio source.  ~30ms latency.
 *
 *  Both modes produce the same PulseState output.  The game doesn't
 *  need to know which mode is active.
 *
 *  Usage:
 *    pd_init(NULL);                          // defaults
 *    pd_start_audio_capture();               // or pd_spotify_auth()
 *    while (running) {
 *        PulseState s = pd_update(dt);
 *        if (s.beat_onset) spawn_enemies();
 *        fire_rate *= 1.0f + s.intensity;
 *    }
 *    pd_shutdown();
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

/* ================================================================== */
/*  Core state — returned every frame by pd_update()                  */
/* ================================================================== */

typedef struct PulseState {
    float bpm;              /* current detected BPM (0 if unknown)       */
    float intensity;        /* overall energy 0.0-1.0                     */
    float beat_phase;       /* position within current beat 0.0-1.0       */
    bool  beat_onset;       /* true on the exact frame a beat hits         */
    bool  bar_onset;        /* true on beat 1 of a new bar                */
    bool  section_change;   /* true when song section changes              */
    float section_energy;   /* energy of current section 0.0-1.0          */
    float time_remaining;   /* seconds left in current track (-1 if N/A)  */
    int   bar_position;     /* which beat in the bar (0-3 for 4/4 time)   */
    float confidence;       /* how confident we are in the BPM (0.0-1.0)  */

    /* Frequency band energy (for visual effects) */
    float band_sub_bass;    /* 20-60 Hz    */
    float band_bass;        /* 60-250 Hz   */
    float band_mid;         /* 250-4000 Hz */
    float band_high;        /* 4000+ Hz    */
} PulseState;

/* ================================================================== */
/*  Lifecycle                                                          */
/* ================================================================== */

typedef struct PulseConfig {
    int   fft_size;         /* FFT window size (default 2048)             */
    int   sample_rate;      /* expected sample rate (default 44100)       */
    float min_bpm;          /* BPM detection range low (default 60)       */
    float max_bpm;          /* BPM detection range high (default 200)     */
    float onset_threshold;  /* spectral flux threshold (default 1.5)      */
} PulseConfig;

/* Pass NULL for defaults */
void pd_init(const PulseConfig *config);
void pd_shutdown(void);

/* Call every frame.  Returns current music state. */
PulseState pd_update(float dt);

/* ================================================================== */
/*  Mode B: System Audio Capture                                       */
/* ================================================================== */

/* Start/stop capturing system audio output.
 * On macOS: uses Core Audio (may need Screen Recording permission).
 * On Windows: uses WASAPI loopback (no special permissions). */
bool pd_start_audio_capture(void);
void pd_stop_audio_capture(void);
bool pd_is_capturing(void);

/* Feed audio samples directly (alternative to system capture).
 * Call from your audio callback to analyze your own game audio.
 * Samples should be mono float, any sample rate. */
void pd_feed_samples(const float *samples, int count);

/* ================================================================== */
/*  Mode A: Spotify Integration                                        */
/* ================================================================== */

typedef struct PdTrack {
    char  name[256];
    char  artist[256];
    char  uri[128];
    int   duration_ms;
    float bpm;
    float energy;
} PdTrack;

typedef struct PdPlaylist {
    char  name[256];
    char  uri[128];
    int   track_count;
} PdPlaylist;

typedef enum PdSpotifyStatus {
    PD_SPOTIFY_DISCONNECTED = 0,
    PD_SPOTIFY_AUTHENTICATING,
    PD_SPOTIFY_CONNECTED,
    PD_SPOTIFY_ERROR
} PdSpotifyStatus;

/* Initialize Spotify with your app's client ID.
 * redirect_port: localhost port for OAuth callback (e.g. 8765). */
void pd_spotify_init(const char *client_id, int redirect_port);

/* Opens browser for user to authorize.  Non-blocking.
 * Poll pd_spotify_status() to check when auth completes. */
void pd_spotify_auth(void);
PdSpotifyStatus pd_spotify_status(void);

/* Playlist browsing */
int  pd_spotify_get_playlists(PdPlaylist *out, int max_count);
int  pd_spotify_get_playlist_tracks(const char *playlist_uri,
                                      PdTrack *out, int max_count);

/* Playback control */
void pd_spotify_play(void);
void pd_spotify_pause(void);
void pd_spotify_next(void);
void pd_spotify_previous(void);
void pd_spotify_set_playlist(const char *playlist_uri);
void pd_spotify_play_track(const char *track_uri);

/* Current track info */
bool pd_spotify_get_current_track(PdTrack *out);

#ifdef __cplusplus
}
#endif
