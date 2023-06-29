#ifndef THREAD_H
#define THREAD_H
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <future>
class ThreadPool {
public:
    ThreadPool(std::size_t numThreads);
    template<class F, class... Args>
    auto enqueue (F&& f, Args&&... args) ->std::future<typename std::result_of<F(Args...)>::type>
    {
        using r = typename std::result_of<F(Args...)>::type;
        auto t = std::make_shared<std::packaged_task<r()>>(std::bind(std::forward<F>(f), std::forward<Args>(args)...));
        std::future<r> res = t->get_future();
        {
            std::unique_lock<std::mutex> lock(queue_mutex);

            if (stop) {
                throw std::runtime_error("enqueue on stopped ThreadPool");
            }

            tasks.emplace([t]() { (*t)(); });
        }
        condition.notify_all();
        return res;
    }
    ~ThreadPool();
private:
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;

    std::mutex queue_mutex;
    std::condition_variable condition;
    bool stop = false;
};
#endif
