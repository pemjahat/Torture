#pragma once

#include <Windows.h>

class StepTimer
{
public:
	StepTimer() :
		m_elapsedTicks(0)
	{ 
		QueryPerformanceFrequency(&m_qpcFrequency);
		QueryPerformanceCounter(&m_qpcLastTime);

		// Initialize max delta to 1/10 of second
		m_qpcMaxDelta = m_qpcFrequency.QuadPart / 10;
	}

	double GetElapsedSeconds() const { return TicksToSeconds(m_elapsedTicks); }

	// Integer format represents time using 10,000,000 ticks per second.
	static const UINT64 TicksPerSecond = 10000000;
	
	static double TicksToSeconds(UINT64 ticks) { return static_cast<double>(ticks) / TicksPerSecond; }

	void Tick()
	{
		// Querry current time
		LARGE_INTEGER currentTime;

		QueryPerformanceCounter(&currentTime);

		UINT64 timeDelta = currentTime.QuadPart - m_qpcLastTime.QuadPart;

		m_qpcLastTime = currentTime;

		// clamp excessive large time delta (e.g. after pause in debugger)
		if (timeDelta > m_qpcMaxDelta)
		{
			timeDelta = m_qpcMaxDelta;
		}
		
		// Convert qpc unit into canonical time format
		timeDelta *= TicksPerSecond;
		timeDelta /= m_qpcFrequency.QuadPart;

		m_elapsedTicks = timeDelta;
	}

private:
	// Source data using qpc
	LARGE_INTEGER m_qpcFrequency;
	LARGE_INTEGER m_qpcLastTime;
	UINT64 m_qpcMaxDelta;

	// derived timing data uses canonical tick format
	UINT64 m_elapsedTicks;
};