/*  pd_spotify.cpp  --  Spotify Web API integration
 *
 *  Non-blocking HTTP via libcurl multi interface.  All API calls
 *  are dispatched asynchronously and polled in pd_update().
 *  Zero game-thread blocking.
 */
#include "pd_internal.h"

#ifdef PD_SPOTIFY_ENABLED

#include <nlohmann/json.hpp>
#include <curl/curl.h>
#include <string.h>
#include <stdio.h>

using json = nlohmann::json;

/* ================================================================== */
/*  Spotify API stubs (async HTTP via curl_multi)                      */
/* ================================================================== */

void pd_spotify_init(const char *client_id, int redirect_port)
{
    strncpy(g_pd.spotify_client_id, client_id, sizeof(g_pd.spotify_client_id) - 1);
    g_pd.spotify_redirect_port = redirect_port;
    g_pd.spotify_status = PD_SPOTIFY_DISCONNECTED;
    fprintf(stderr, "[pulsedrive] Spotify initialized (client: %.8s...)\n", client_id);
}

void pd_spotify_auth(void)
{
    /* TODO: Implement PKCE OAuth flow
     * 1. Generate code_verifier + code_challenge
     * 2. Open browser to https://accounts.spotify.com/authorize
     * 3. Start localhost HTTP server on redirect_port
     * 4. Exchange auth code for access_token + refresh_token
     */
    g_pd.spotify_status = PD_SPOTIFY_AUTHENTICATING;
    fprintf(stderr, "[pulsedrive] Spotify auth: TODO — open browser for PKCE flow\n");
}

PdSpotifyStatus pd_spotify_status(void)
{
    return (PdSpotifyStatus)g_pd.spotify_status;
}

int pd_spotify_get_playlists(PdPlaylist *out, int max_count)
{
    (void)out; (void)max_count;
    /* TODO: GET https://api.spotify.com/v1/me/playlists */
    return 0;
}

int pd_spotify_get_playlist_tracks(const char *playlist_uri,
                                     PdTrack *out, int max_count)
{
    (void)playlist_uri; (void)out; (void)max_count;
    /* TODO: GET https://api.spotify.com/v1/playlists/{id}/tracks */
    return 0;
}

void pd_spotify_play(void)
{
    /* TODO: PUT https://api.spotify.com/v1/me/player/play */
}

void pd_spotify_pause(void)
{
    /* TODO: PUT https://api.spotify.com/v1/me/player/pause */
}

void pd_spotify_next(void)
{
    /* TODO: POST https://api.spotify.com/v1/me/player/next */
}

void pd_spotify_previous(void)
{
    /* TODO: POST https://api.spotify.com/v1/me/player/previous */
}

void pd_spotify_set_playlist(const char *playlist_uri)
{
    (void)playlist_uri;
    /* TODO: Start playback of playlist context */
}

void pd_spotify_play_track(const char *track_uri)
{
    (void)track_uri;
    /* TODO: Start playback of specific track */
}

bool pd_spotify_get_current_track(PdTrack *out)
{
    (void)out;
    /* TODO: GET https://api.spotify.com/v1/me/player/currently-playing */
    return false;
}

#else

/* Stubs when Spotify is disabled */
void pd_spotify_init(const char *client_id, int redirect_port)
{ (void)client_id; (void)redirect_port; }
void pd_spotify_auth(void) {}
PdSpotifyStatus pd_spotify_status(void) { return PD_SPOTIFY_DISCONNECTED; }
int  pd_spotify_get_playlists(PdPlaylist *out, int max_count)
{ (void)out; (void)max_count; return 0; }
int  pd_spotify_get_playlist_tracks(const char *uri, PdTrack *out, int max)
{ (void)uri; (void)out; (void)max; return 0; }
void pd_spotify_play(void) {}
void pd_spotify_pause(void) {}
void pd_spotify_next(void) {}
void pd_spotify_previous(void) {}
void pd_spotify_set_playlist(const char *uri) { (void)uri; }
void pd_spotify_play_track(const char *uri) { (void)uri; }
bool pd_spotify_get_current_track(PdTrack *out) { (void)out; return false; }

#endif /* PD_SPOTIFY_ENABLED */
