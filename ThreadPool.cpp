#include <iostream>
#include <queue>
#include <thread>
#include <mutex>
#include <functional>
#include <future>
#include <chrono>

#include "Worker.h"

class ThreadPool {
public:
    ThreadPool(int initialWorkersCount, int tasksBatchSize = 1) {
        this->workersCount = initialWorkersCount;

        this->workersTasks.resize(initialWorkersCount);
        this->workers.reserve(initialWorkersCount);

        for (int i = 0; i < initialWorkersCount; ++i) {
            int workerId = i;

            auto worker = new Worker([&, workerId, tasksBatchSize]() {
                return this->putTasks(workerId, tasksBatchSize);
            });

            this->workers.push_back(std::move(worker));
            this->workersTasks[workerId] = std::queue<std::function<void()>>();
        }
    }

    ~ThreadPool() {
        this->await();
    }

    void AddTask(std::function<void()> task) {
        auto lock = std::lock_guard(this->mu);

        this->workersTasks[this->nextWorkerId].push(task);
        this->nextWorkerId = (this->nextWorkerId + 1) % this->workersCount;
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
    int nextWorkerId = 0;
    int workersCount;

    std::mutex mu;
    std::vector<Worker*> workers;
    std::vector<std::queue<std::function<void()>>> workersTasks;

    std::queue<std::function<void()>> putTasks(int workerId, int tasksQueueSize) {
        std::queue<std::function<void()>> externalTasks;
        auto lock = std::lock_guard(this->mu);

        while (tasksQueueSize--) {
            if (this->workersTasks[workerId].empty()) {
                return externalTasks;
            }

            auto task = this->workersTasks[workerId].front();
            this->workersTasks[workerId].pop();

            externalTasks.push(task);
        }

        return externalTasks;
    }

    void await() {
        this->isEnding = true;

        for (auto& w: this->workers) {
            w->Await();
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
