#pragma once

class ASyncLoader
{
  public:
    [[nodiscard]] bool IsValid() const { return m_isValid; }

    void WaitForLoad() const
    {
      std::unique_lock lock(m_mutex);
      m_cv.wait(lock, [this] { return !m_isLoading.load(std::memory_order_acquire); });
    }

    bool IsLoading() const { return m_isLoading.load(std::memory_order_acquire); }

  protected:
    void StartLoading()
    {
      DEBUG_ASSERT_TEXT(!m_isLoading, "Already loading");
      m_isLoading.store(true, std::memory_order_release);
    }

    void FinishLoading()
    {
      DEBUG_ASSERT_TEXT(m_isLoading, "Not loading");
      {
        std::lock_guard lock(m_mutex);
        m_isLoading.store(false, std::memory_order_release);
        m_isValid.store(true, std::memory_order_release);
      }
      m_cv.notify_all();
    }

    std::atomic_bool m_isValid{false};
    std::atomic_bool m_isLoading{false};
    mutable std::mutex m_mutex;
    mutable std::condition_variable m_cv;
};

class StaticASyncLoader
{
  public:
    [[nodiscard]] static bool IsValid() { return m_isValid.load(std::memory_order_acquire); }

    static void WaitForLoad()
    {
      std::unique_lock lock(m_mutex);
      m_cv.wait(lock, [] { return !m_isLoading.load(std::memory_order_acquire); });
    }

  protected:
    static void StartLoading()
    {
      DEBUG_ASSERT_TEXT(!m_isLoading, "Already loading");
      m_isLoading.store(true, std::memory_order_release);
    }

    static void FinishLoading()
    {
      DEBUG_ASSERT_TEXT(m_isLoading, "Not loading");
      {
        std::lock_guard lock(m_mutex);
        m_isLoading.store(false, std::memory_order_release);
        m_isValid.store(true, std::memory_order_release);
      }
      m_cv.notify_all();
    }

    inline static std::atomic_bool m_isValid{false};
    inline static std::atomic_bool m_isLoading{false};
    inline static std::mutex m_mutex;
    inline static std::condition_variable m_cv;
};
