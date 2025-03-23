#include <iostream>
#include <atomic>
#include <thread>
#include <vector>
#include <chrono>
#include <emmintrin.h> // SSE2 for CPU pause

// 简单的环形缓冲区实现（单生产者、单消费者）
template<typename T, size_t Size>
class RingBuffer {
public:
    bool push(const T& item) {
        size_t current_write = _write_pos.load(std::memory_order_relaxed);
        size_t next_write = (current_write + 1) % Size;
        if (next_write == _read_pos.load(std::memory_order_acquire)) {
            return false; // 缓冲区满
        }
        _buffer[current_write] = item;
        _write_pos.store(next_write, std::memory_order_release);
        return true;
    }

    bool pop(T& item) {
        size_t current_read = _read_pos.load(std::memory_order_relaxed);
        if (current_read == _write_pos.load(std::memory_order_acquire)) {
            return false; // 缓冲区空
        }
        item = _buffer[current_read];
        _read_pos.store((current_read + 1) % Size, std::memory_order_release);
        return true;
    }
private:
    T _buffer[Size]{0};
    std::atomic<size_t> _write_pos{0};
    std::atomic<size_t> _read_pos{0};
};

// 消息结构
struct Message {
    double price;
    int64_t timestamp;
};

// 生产者函数
void producer(RingBuffer<Message, 1024>& buffer, const std::atomic<bool>& running) {
    int count = 0;
    while (running.load()) {
        Message msg{count * 1.01, std::chrono::high_resolution_clock::now().time_since_epoch().count()};
        while (!buffer.push(msg)) {
            _mm_pause(); // CPU暂停，避免忙等待浪费资源
        }
        count++;
        // 模拟一些工作
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }
    std::cout << "Producer generated " << count << " messages\n";
}

// 消费者函数
void consumer(RingBuffer<Message, 1024>& buffer, const std::atomic<bool>& running) {
    Message msg{};
    int processed = 0;
    auto start = std::chrono::high_resolution_clock::now();

    while (running.load() || buffer.pop(msg)) {
        if (buffer.pop(msg)) {
            processed++;
            // 模拟处理（例如计算移动平均）
            if (processed % 1000 == 0) {
                auto now = std::chrono::high_resolution_clock::now();
                auto latency = std::chrono::duration_cast<std::chrono::microseconds>(now - start).count();
                std::cout << "Processed " << processed << " messages, last price: " << msg.price << ", latency: " << latency / 1000.0 << " ms\n";
                start = now;
            }
        } else {
            _mm_pause();
        }
    }
    std::cout << "Consumer processed " << processed << " messages\n";
}

// 设置线程亲和性（绑定到CPU核心）
void set_thread_affinity(std::thread& t, int cpu) {
#ifdef __linux__
    cpu_set_t cpu_set;
    CPU_ZERO(&cpu_set);
    CPU_SET(cpu, &cpu_set);
    pthread_setaffinity_np(t.native_handle(), sizeof(cpu_set_t), &cpu_set);
#endif
}

int main() {
    RingBuffer<Message, 1024> buffer;
    std::atomic<bool> running{true};

    // 启动生产者和消费者线程
    std::thread prod_thread(producer, std::ref(buffer), std::ref(running));
    std::thread cons_thread(consumer, std::ref(buffer), std::ref(running));

    // 绑定线程到CPU核心
    set_thread_affinity(prod_thread, 0); // 生产者绑定到核心0
    set_thread_affinity(cons_thread, 1); // 消费者绑定到核心1

    // 运行一段时间后停止
    std::this_thread::sleep_for(std::chrono::seconds(5));
    running.store(false);

    prod_thread.join();
    cons_thread.join();

    return 0;
}