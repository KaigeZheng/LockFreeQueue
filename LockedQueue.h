#ifndef LOCKEDQUEUE_H
#define LOCKEDQUEUE_H

#include <queue>
#include <mutex>

template<typename T>
class LockedQueue {
    private:
        std::queue<T> q;
        std::mutex mtx;

    public:
        LockedQueue() = default;
        ~LockedQueue() = default;
        LockedQueue(const LockedQueue&) = delete;
        LockedQueue& operator=(const LockedQueue&) = delete;

        void enqueue(const T& val) {
            std::lock_guard<std::mutex> lock(mtx);
            q.push(val);
        }

        bool dequeue(T& val) {
            std::lock_guard<std::mutex> lock(mtx);
            if (q.empty()) return false;
            val = q.front();
            q.pop();
            return true;
        }
};

#endif