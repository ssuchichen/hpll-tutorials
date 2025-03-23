#include <iostream>
#include <vector>
#include <chrono>
#include <functional>
#include <atomic>
#include <thread>

class TimeWheel
{
public:
    TimeWheel(): _wheel(WHEEL_SIZE) {}

    void add_task(int delay_ms, const std::function<void()>& task)
    {
        size_t slot = (_current_slot+delay_ms) % WHEEL_SIZE;
        _wheel[slot].push_back(task);
    }

    void run()
    {
        _running.store(true);
        while(_running.load())
        {
            auto& tasks = _wheel[_current_slot];
            for(auto& task : tasks)
            {
                task();
            }
            tasks.clear();
            _current_slot = (_current_slot + 1) % WHEEL_SIZE;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    void stop()
    {
        _running.store(false);
    }
private:
    static constexpr size_t WHEEL_SIZE = 1000;
    std::vector<std::vector<std::function<void()>>> _wheel;
    size_t _current_slot{0};
    std::atomic<bool> _running{false};
};

int main()
{
    TimeWheel tw;
    std::thread tw_thread(&TimeWheel::run, &tw);

    // 添加任务
    tw.add_task(100, []() { std::cout << "Task 1 executed\n"; });
    tw.add_task(500, []() { std::cout << "Task 2 executed\n"; });

    std::this_thread::sleep_for(std::chrono::seconds(1));
    tw.stop();
    tw_thread.join();

    return 0;
}