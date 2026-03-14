/*  pd_oauth.cpp  --  PKCE OAuth flow (placeholder)
 *
 *  Will implement:
 *  1. Generate code_verifier (random 128 bytes → base64url)
 *  2. SHA-256 hash → code_challenge
 *  3. Open browser to Spotify authorize URL
 *  4. Localhost HTTP server to catch redirect
 *  5. Exchange auth code for access + refresh tokens
 */
#include "pd_internal.h"

/* TODO: Full PKCE implementation */
