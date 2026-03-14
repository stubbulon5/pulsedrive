# pulsedrive

Music-reactive gameplay engine. Standalone C++ library that syncs game events to music in real-time.

Two modes:
- **System Audio Capture** — real-time FFT + beat detection from whatever is playing (any source)
- **Spotify Integration** — pre-computed beat timestamps for perfect sync (WIP)

## What it does

Call `pd_update(dt)` every frame. Get back:

| Field | Description |
|-------|-------------|
| `bpm` | Current detected BPM |
| `intensity` | Overall energy (0.0-1.0) |
| `beat_onset` | True on the exact frame a beat hits |
| `beat_phase` | Position within current beat (0.0-1.0) |
| `bar_onset` | True on beat 1 of a new bar |
| `bar_position` | Which beat in the bar (0-3 for 4/4) |
| `section_change` | True when energy shifts (verse/chorus) |
| `band_sub_bass` | Sub-bass energy (20-60 Hz) |
| `band_bass` | Bass energy (60-250 Hz) |
| `band_mid` | Mid energy (250-4000 Hz) |
| `band_high` | High energy (4000+ Hz) |
| `confidence` | BPM detection confidence (0.0-1.0) |

## Quick start

```cpp
#include "pulsedrive.h"

pd_init(NULL);
pd_start_audio_capture();

while (running) {
    PulseState s = pd_update(dt);

    if (s.beat_onset)           spawn_enemies();
    if (s.section_change)       trigger_boss();
    game_speed = s.bpm / 120.0f;
    fire_rate *= 1.0f + s.intensity;
    glow = s.beat_phase;
}

pd_stop_audio_capture();
pd_shutdown();
```

## Integration via CMake FetchContent

```cmake
include(FetchContent)

set(KISSFFT_TEST OFF CACHE BOOL "" FORCE)
set(KISSFFT_TOOLS OFF CACHE BOOL "" FORCE)
set(PD_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(PD_SPOTIFY OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    pulsedrive
    GIT_REPOSITORY https://github.com/stubbulon5/pulsedrive.git
    GIT_TAG        main
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(pulsedrive)

target_link_libraries(your_game PRIVATE pulsedrive)
```

## Build standalone

```bash
cmake -B build
cmake --build build --parallel
./build/pd_test    # play music, watch beat detections
```

## Platform support

| Platform | Audio Capture | Method |
|----------|--------------|--------|
| macOS | Yes | Core Audio input queue |
| Windows | Yes | WASAPI loopback |
| Linux | Stub (returns false) | Planned: PulseAudio/PipeWire |

On macOS, system audio capture may require **Screen Recording** permission (System Settings > Privacy & Security).

## How it works

```
System Audio (44.1kHz)
    |
    v
Ring Buffer (lock-free, 65K samples)
    |
    v
Windowed FFT (Kiss FFT, 2048-point Hann window, 75% overlap)
    |
    +---> Spectral Flux (onset detection)
    |         |
    |         +---> Autocorrelation (BPM estimation)
    |         |
    |         +---> Beat Phase Tracking (prediction + correction)
    |
    +---> Frequency Band Energy (sub-bass, bass, mid, high)
    |         |
    |         +---> EMA Smoothing (8Hz responsiveness)
    |         |
    |         +---> Section Detection (sustained energy shifts)
    |
    v
PulseState output (one struct, every frame)
```

## Dependencies

| Library | License | Purpose |
|---------|---------|---------|
| [Kiss FFT](https://github.com/mborgerding/kissfft) | Public domain | FFT |
| libcurl | MIT | Spotify API (optional) |
| nlohmann/json | MIT | JSON parsing (optional) |

No GPL dependencies.

## Performance

- Audio capture: separate thread, lock-free ring buffer
- FFT: ~0.1ms per frame (Kiss FFT)
- `pd_update()`: non-blocking, <0.01ms
- Total CPU: ~2% for real-time analysis at 44.1kHz

## License

MIT
