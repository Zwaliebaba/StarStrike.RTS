#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <thread>
#include <vector>

namespace Neuron
{

/// Lightweight thread pool for parallel work dispatch.
///
/// Enqueue tasks; worker threads process them in FIFO order.
class ThreadPool
{
public:
    /// Create a pool with the given number of worker threads.
    /// Pass 0 to use std::thread::hardware_concurrency().
    explicit ThreadPool(uint32_t numThreads = 0);
    ~ThreadPool();

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    /// Submit a task for execution. Thread-safe.
    void enqueue(std::function<void()> task);

    /// Block until all queued tasks have completed.
    void waitAll();

    /// Number of worker threads.
    [[nodiscard]] uint32_t threadCount() const noexcept;

    /// Graceful shutdown: finish pending tasks, then join threads.
    void shutdown();

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace Neuron
