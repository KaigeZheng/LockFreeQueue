#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>

#include "LockFreeQueue.h"
#include "LockedQueue.h"

// 生产者函数模板
template<typename QueueType>
void producer(QueueType& q, int start, int count) {
    for (int i = 0; i < count; ++i) {
        q.enqueue(start + i);
    }
}

// 消费者函数模板，退出条件改成消费够所有元素
template<typename QueueType>
void consumer(QueueType& q, std::atomic<int>& consumed_count, int total_count) {
    int val;
    while (true) {
        if (q.dequeue(val)) {
            consumed_count.fetch_add(1, std::memory_order_relaxed);
        } else {
            // 如果已经消费够全部数据，退出
            if (consumed_count.load(std::memory_order_relaxed) >= total_count) {
                break;
            }
            std::this_thread::yield();
        }
    }
}

int main() {
    constexpr int producer_thread_num = 4;
    constexpr int consumer_thread_num = 4;
    constexpr int items_per_producer = 100000;

    int total_items = producer_thread_num * items_per_producer;

    // 测试带锁队列
    {
        LockedQueue<int> locked_q;
        std::atomic<int> consumed_count{0};

        std::vector<std::thread> producers;
        std::vector<std::thread> consumers;

        auto start_time = std::chrono::steady_clock::now();

        // 启动消费者线程
        for (int i = 0; i < consumer_thread_num; ++i) {
            consumers.emplace_back(consumer<LockedQueue<int>>, std::ref(locked_q), std::ref(consumed_count), total_items);
        }

        // 启动生产者线程
        for (int i = 0; i < producer_thread_num; ++i) {
            producers.emplace_back(producer<LockedQueue<int>>, std::ref(locked_q), i * items_per_producer, items_per_producer);
        }

        // 等待生产者线程结束
        for (auto& p : producers) p.join();
        // 等待消费者线程结束
        for (auto& c : consumers) c.join();

        auto end_time = std::chrono::steady_clock::now();
        double elapsed = std::chrono::duration<double>(end_time - start_time).count();

        std::cout << "[LockedQueue] Total consumed: " << consumed_count.load() << ", time: " << elapsed << " seconds\n";
    }

    // 测试无锁队列
    {
        LockFreeQueue<int> lf_q;
        std::atomic<int> consumed_count{0};

        std::vector<std::thread> producers;
        std::vector<std::thread> consumers;

        auto start_time = std::chrono::steady_clock::now();

        // 启动消费者线程
        for (int i = 0; i < consumer_thread_num; ++i) {
            consumers.emplace_back(consumer<LockFreeQueue<int>>, std::ref(lf_q), std::ref(consumed_count), total_items);
        }

        // 启动生产者线程
        for (int i = 0; i < producer_thread_num; ++i) {
            producers.emplace_back(producer<LockFreeQueue<int>>, std::ref(lf_q), i * items_per_producer, items_per_producer);
        }

        // 等待生产者线程结束
        for (auto& p : producers) p.join();
        // 等待消费者线程结束
        for (auto& c : consumers) c.join();

        auto end_time = std::chrono::steady_clock::now();
        double elapsed = std::chrono::duration<double>(end_time - start_time).count();

        std::cout << "[LockFreeQueue] Total consumed: " << consumed_count.load() << ", time: " << elapsed << " seconds\n";
    }

    return 0;
}
