#pragma once

#include <chrono>

class Timer
{
public:
    void onRender();
    void onFrameStart();
    void onFrameStop();
    void onKeyP() { paused = !paused; }

    float getFrameTime() const { return frameTimer; }

private:
    std::chrono::time_point<std::chrono::high_resolution_clock> lastTimestamp, tPrevEnd;

    // Frame counter to display fps
    uint32_t frameCounter = 0;
    uint32_t lastFPS = 0;

    /** @brief Last frame time measured using a high performance timer (if available) */
    float frameTimer = 1.0f;

    // Defines a frame rate independent timer value clamped from -1.0...1.0
    // For use in animations, rotations, etc.
    float timer = 0.0f;

    // Multiplier for speeding up (or slowing down) the global timer
    float timerSpeed = 0.25f;

    bool paused = false;

    std::chrono::steady_clock::time_point tStart;
};