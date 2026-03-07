#pragma once

namespace Neuron::Timer
{
  // Helper for animation and simulation timing.
  class Core
  {
    public:
      static void Startup()
      {
        if (!QueryPerformanceFrequency(&m_qpcFrequency))
          throw_hresult(E_FAIL);

        if (!QueryPerformanceCounter(&m_qpcLastTime))
          throw_hresult(E_FAIL);

        // Initialize max delta to 1/10 of a second.
        m_qpcMaxDelta = m_qpcFrequency.QuadPart / 10;
      }

      // Get elapsed time since the previous Update call.
      static uint64_t GetElapsedTicks() { return m_elapsedTicks; }
      static float GetElapsedSeconds() { return static_cast<float>(TicksToSeconds(m_elapsedTicks)); }

      // Get total time since the start of the program.
      static uint64_t GetTotalTicks() { return m_totalTicks; }
      static double GetTotalSeconds() { return TicksToSeconds(m_totalTicks); }

      // Get total number of updates since start of the program.
      static uint32_t GetFrameCount() { return m_frameCount; }

      // Get the current framerate.
      static uint32_t GetFramesPerSecond() { return m_framesPerSecond; }

      // Integer format represents time using 10,000,000 ticks per second.
      static constexpr uint64_t TicksPerSecond = 10000000;

      static double TicksToSeconds(uint64_t ticks) { return static_cast<double>(ticks) / TicksPerSecond; }
      static uint64_t SecondsToTicks(double seconds) { return static_cast<uint64_t>(seconds * TicksPerSecond); }

      // After an intentional timing discontinuity (for instance a blocking IO operation)
      // call this to avoid fixed timestep logic attempting a string of catch-up Update calls.
      static void ResetElapsedTime()
      {
        if (!QueryPerformanceCounter(&m_qpcLastTime))
          throw_hresult(E_FAIL);

        m_leftOverTicks = 0;
        m_framesPerSecond = 0;
        m_framesThisSecond = 0;
        m_qpcSecondCounter = 0;
      }

      // Update timer state, calling the specified Update function the appropriate number of times.
      static float Update()
      {
        // Query the current time.
        LARGE_INTEGER currentTime;

        if (!QueryPerformanceCounter(&currentTime))
          throw_hresult(E_FAIL);

        uint64_t timeDelta = currentTime.QuadPart - m_qpcLastTime.QuadPart;

        m_qpcLastTime = currentTime;
        m_qpcSecondCounter += timeDelta;

        // Clamp excessively large time deltas (eg. after paused in the debugger).
        if (timeDelta > m_qpcMaxDelta)
          timeDelta = m_qpcMaxDelta;

        // Convert QPC units into our own canonical tick format. Cannot overflow due to the previous clamp.
        timeDelta *= TicksPerSecond;
        timeDelta /= m_qpcFrequency.QuadPart;

        const uint32_t lastFrameCount = m_frameCount;

        // Variable timestep update logic.
        m_elapsedTicks = timeDelta;
        m_totalTicks += timeDelta;
        m_leftOverTicks = 0;
        m_frameCount++;

        // Track the current framerate.
        if (m_frameCount != lastFrameCount)
          m_framesThisSecond++;

        if (m_qpcSecondCounter >= static_cast<uint64_t>(m_qpcFrequency.QuadPart))
        {
          m_framesPerSecond = m_framesThisSecond;
          m_framesThisSecond = 0;
          m_qpcSecondCounter %= m_qpcFrequency.QuadPart;
        }

        return GetElapsedSeconds();
      }

      // Fixed-step accumulator for deterministic simulation (e.g. server tick loop).
      // Accumulates elapsed wall time, then invokes fixedUpdate N times at fixedDtSec intervals.
      // Returns the interpolation alpha (0..1) representing how far into the next fixed step
      // the remaining time extends — useful for client-side rendering interpolation.
      template <typename Fn>
      static float UpdateFixedStep(double fixedDtSec, Fn&& fixedUpdate)
      {
        LARGE_INTEGER currentTime;

        if (!QueryPerformanceCounter(&currentTime))
          throw_hresult(E_FAIL);

        uint64_t timeDelta = currentTime.QuadPart - m_qpcLastTime.QuadPart;

        m_qpcLastTime = currentTime;
        m_qpcSecondCounter += timeDelta;

        if (timeDelta > m_qpcMaxDelta)
          timeDelta = m_qpcMaxDelta;

        timeDelta *= TicksPerSecond;
        timeDelta /= m_qpcFrequency.QuadPart;

        m_totalTicks += timeDelta;

        // Accumulate time and run fixed steps.
        const uint64_t fixedDtTicks = SecondsToTicks(fixedDtSec);
        m_leftOverTicks += timeDelta;

        while (m_leftOverTicks >= fixedDtTicks)
        {
          m_elapsedTicks = fixedDtTicks;
          m_leftOverTicks -= fixedDtTicks;
          m_frameCount++;
          fixedUpdate(static_cast<float>(fixedDtSec));
        }

        // Track the current framerate.
        m_framesThisSecond++;

        if (m_qpcSecondCounter >= static_cast<uint64_t>(m_qpcFrequency.QuadPart))
        {
          m_framesPerSecond = m_framesThisSecond;
          m_framesThisSecond = 0;
          m_qpcSecondCounter %= m_qpcFrequency.QuadPart;
        }

        // Return interpolation alpha for rendering between fixed steps.
        return fixedDtTicks > 0
          ? static_cast<float>(static_cast<double>(m_leftOverTicks) / static_cast<double>(fixedDtTicks))
          : 0.0f;
      }

    private:
      // Source timing data uses QPC units.
      inline static LARGE_INTEGER m_qpcFrequency;
      inline static LARGE_INTEGER m_qpcLastTime;
      inline static uint64_t m_qpcMaxDelta;

      // Derived timing data uses our own canonical tick format.
      inline static uint64_t m_elapsedTicks{0};
      inline static uint64_t m_totalTicks{0};
      inline static uint64_t m_leftOverTicks{0};

      // For tracking the framerate.
      inline static uint32_t m_frameCount{0};
      inline static uint32_t m_framesPerSecond{0};
      inline static uint32_t m_framesThisSecond{0};
      inline static uint64_t m_qpcSecondCounter{0};
  };
}
