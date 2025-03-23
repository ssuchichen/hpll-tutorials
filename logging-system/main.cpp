#include <iostream>
#include <atomic>
#include <thread>
#include <vector>
#include <chrono>
#include <cstring>
#include <emmintrin.h>

// 固定大小的内存缓冲区
class LogBuffer
{
public:
    LogBuffer()
    {
        _buffer = new char[BUFFER_SIZE];
    }

    ~LogBuffer()
    {
        delete[] _buffer;
    }

    bool write(const char* msg, size_t len)
    {
        size_t pos = _write_pos.load(std::memory_order_relaxed);
        size_t new_pos = pos + len + 1;
        if (new_pos >= BUFFER_SIZE)
        {
            // 缓冲区满
            return false;
        }
        memcpy(_buffer+pos, msg, len);
        _buffer[pos + len] = '\n';
        _write_pos.store(new_pos, std::memory_order_release);
        return true;
    }

    void flush()
    {
        if (_flushing.exchange(true))
        {
            // 避免重复flush
            return;
        }
        size_t pos = _write_pos.load(std::memory_order_acquire);
        if (pos > 0)
        {
            // 模拟磁盘写入（实际可以用 fwrite 或 asio）
            std::cout.write(_buffer, pos);
            _write_pos.store(0, std::memory_order_release);
        }
        _flushing.store(false);
    }
private:
    static constexpr size_t BUFFER_SIZE = 1024 * 1024; // 1MB
    char* _buffer;
    std::atomic<size_t> _write_pos{0};
    std::atomic<bool> _flushing{false};
};

// 日志写入线程
void logger(LogBuffer& buffer, std::atomic<bool>& running)
{
    int count = 0;
    while (running.load())
    {
        std::string msg = "Log entry " + std::to_string(count++);
        while (!buffer.write(msg.c_str(), msg.size()))
        {
            // 等待缓冲区空间
            _mm_pause();
        }
        std::this_thread::yield(); // 模拟工作
    }
}

// 异步刷新线程
void flusher(LogBuffer& buffer, std::atomic<bool>& running)
{
    while (running.load())
    {
        buffer.flush();
        // 批量刷新
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    buffer.flush();// 最后一次刷新
}

int main()
{
    LogBuffer buffer;
    std::atomic<bool> running{true};

    std::thread log_thread(logger, std::ref(buffer), std::ref(running));
    std::thread flush_thread(flusher, std::ref(buffer), std::ref(running));

    std::this_thread::sleep_for(std::chrono::seconds(3));
    running.store(false);

    log_thread.join();
    flush_thread.join();

    return 0;
}