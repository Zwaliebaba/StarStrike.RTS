#pragma once

class ASyncLoader
{
  public:
    [[nodiscard]] bool IsValid() const { return m_isValid; }

    void WaitForLoad() const
    {
      while (m_isLoading)
        std::this_thread::yield();
    }

    bool IsLoading() const { return m_isLoading; }

  protected:
    void StartLoading()
    {
      DEBUG_ASSERT_TEXT(!m_isLoading, "Already loading");
      m_isLoading = true;
    }

    void FinishLoading()
    {
      DEBUG_ASSERT_TEXT(m_isLoading, "Not loading");
      m_isLoading = false;
      m_isValid = true;
    }

    volatile std::atomic_bool m_isValid{false};
    volatile std::atomic_bool m_isLoading{false};
};

class StaticASyncLoader
{
  public:
    [[nodiscard]] static bool IsValid() { return m_isValid; }

    static void WaitForLoad()
    {
      while (m_isLoading)
        std::this_thread::yield();
    }

  protected:
    static void StartLoading()
    {
      DEBUG_ASSERT_TEXT(!m_isLoading, "Already loading");
      m_isLoading = true;
    }

    static void FinishLoading()
    {
      DEBUG_ASSERT_TEXT(m_isLoading, "Not loading");
      m_isLoading = false;
      m_isValid = true;
    }

    inline static volatile std::atomic_bool m_isValid{false};
    inline static volatile std::atomic_bool m_isLoading{false};
};
