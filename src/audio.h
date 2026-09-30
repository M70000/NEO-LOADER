#pragma once

// ---------------------------------------------------------------------------
//  neo audio - tiny Win32 MCI helper (no external dependencies)
// ---------------------------------------------------------------------------
namespace NeoAudio
{
    // Plays rezerosound.mp3 once, asynchronously, on a fresh thread's behalf.
    // Safe to call multiple times; restarts playback from the beginning.
    void PlayStartupSound();

    // Stops and releases the MCI device.
    void Shutdown();
}
