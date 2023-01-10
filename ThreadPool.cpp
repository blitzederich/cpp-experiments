#include <iostream>
#include <queue>
#include <thread>
#include <mutex>
#include <functional>
#include <future>

class ThreadPool {
public:
    ThreadPool(int workersCount) {
        this->workers.reserve(workersCount);

        while (workersCount--) {
            auto worker = std::thread([&](){
                this->loop();
            });
            this->workers.push_back(std::move(worker));
        }
    }

    ~ThreadPool() {
        this->await();
    }

    void AddTask(std::function<void()> task) {
        auto lock = std::lock_guard(this->mu);
        this->tasks.push(task);
    }

    template <typename T>
    std::future<T> Async(std::function<T()> task) {
        auto promise = std::make_shared<std::promise<T>>();
        std::future<T> future = promise->get_future();

        this->AddTask([promise, t = std::move(task)]() {
            try {
                auto res = t();
                promise->set_value(std::move(res));
            } catch (...) {
                promise->set_exception(std::current_exception());
            }
        });

        return future;
    }

private:
    bool isEnding = false;
    std::mutex mu;
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;

    void loop() {
        for (;;) {
            if (!this->mu.try_lock()) {
                continue;
            }

            if (this->isEnding && this->tasks.empty()) {
                this->mu.unlock();
                return;
            }

            if (this->tasks.empty()) {
                this->mu.unlock();
                continue;
            }

            auto task = this->tasks.front();
            this->tasks.pop();
            mu.unlock();

            task();
        }
    }

    void await() {
        this->isEnding = true;

        for (auto& w: this->workers) {
            w.join();
        }
    }
};

int main() {
    auto pool = new ThreadPool(8);

    for (;;) {
        auto sum = pool->Async<int>([]() {
            return 34 + 19;
        });
    }
}
