#include "Timer.h"
#include <iostream>

void Timer::onRender()
{
	lastTimestamp = std::chrono::high_resolution_clock::now();
	tPrevEnd = lastTimestamp;
}

void Timer::onFrameStart()
{
	tStart = std::chrono::high_resolution_clock::now();
}

void Timer::onFrameStop()
{
	++frameCounter;
	auto tEnd = std::chrono::high_resolution_clock::now();
	auto tDiff = std::chrono::duration<double, std::milli>(tEnd - tStart).count();
	frameTimer = (float)tDiff / 1000.0f;

	// Convert to clamped timer value
	if (!paused)
	{
		timer += timerSpeed * frameTimer;
		if (timer > 1.0)
		{
			timer -= 1.0f;
		}
	}

	float fpsTimer = (float)(std::chrono::duration<double, std::milli>(tEnd - lastTimestamp).count());
	if (fpsTimer > 1000.0f)
	{
		lastFPS = static_cast<uint32_t>((float)frameCounter * (1000.0f / fpsTimer));
		frameCounter = 0;
		lastTimestamp = tEnd;
	}

	tPrevEnd = tEnd;
}