#include <queue>
#include <thread>
#include <mutex>
#include <functional>
#include <future>

#include "Worker.h"

class ThreadPool {
public:
    ThreadPool(int initialWorkersCount, size_t tasksBatchSize = 1) {
        if (initialWorkersCount < 1) {
            throw std::invalid_argument("");
        }

        if (tasksBatchSize < 1) {
            throw std::invalid_argument("");
        }

        this->workersCount = initialWorkersCount;

        this->workersTasks.resize(initialWorkersCount);
        this->workers.reserve(initialWorkersCount);

        for (int i = 0; i < initialWorkersCount; ++i) {
            int workerId = i;

            auto worker = std::make_unique<Worker>([this, workerId, tasksBatchSize]() {
                return this->putTasks(workerId, tasksBatchSize);
            });

            this->workers.push_back(std::move(worker));
            this->workersTasks[workerId] = std::queue<std::function<void()>>();
        }
    }

    ~ThreadPool() {
        this->wait();
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

    static size_t GetOptimalWorkersCount() {
        return std::thread::hardware_concurrency();
    }

private:
    bool isEnding = false;
    int nextWorkerId = 0;
    int workersCount;

    std::mutex mu;
    std::vector<std::unique_ptr<Worker>> workers;
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

    void wait() {
        this->isEnding = true;

        for (auto& w: this->workers) {
            w->Await();
        }
    }
};

int main() {
    ThreadPool pool(8);

    const auto tasksCount = 10'000'000LL;
    std::vector<std::future<int>> futures(tasksCount);

    for (auto i = 0; i < tasksCount; ++i) {
        futures[i] = pool.Async<int>([]() {
            return 34 + 19;
        });
    }

    for (auto& f: futures) {
        auto res = f.get();
    }
}
