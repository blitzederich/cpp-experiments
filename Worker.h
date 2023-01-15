#include <iostream>
#include <queue>
#include <thread>
#include <functional>
#include <mutex>
#include <future>

class Worker {
public:
    Worker(std::function<std::queue<std::function<void()>>()> getExternalTasks) {
        this->getExternalTasks = getExternalTasks;

        this->thread = std::thread([this]() {
            this->loop();
        });
    }

    ~Worker() {
        this->Await();
    }

    void Await() {
        this->isEnding = true;

        if (this->thread.joinable()) {
            this->thread.join();
        }
    }

private:
    bool isEnding = false;
    std::thread thread;
    std::queue<std::function<void()>> tasks;
    std::function<std::queue<std::function<void()>>()> getExternalTasks;

    void loop() {
        for (;;) {
            if (this->isEnding && this->tasks.empty()) {
                return;
            }

            if (this->tasks.empty()) {  
                this->tasks = this->getExternalTasks();        
                continue;
            }

            auto task = this->tasks.front();
            this->tasks.pop();

            task();
        }
    }
};