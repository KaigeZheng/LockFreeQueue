#ifndef LOCKFREEQUEUE_H
#define LOCKFREEQUEUE_H

#include <atomic>
#include <cstdint>
#include <cassert>
#include <iostream>

/*
  Double-Width CAS Pointer
*/
struct Pointer {
    void* ptr;
    uint64_t count;
};

template<typename T>
struct Node {
    T data;
    std::atomic<Node*> next;
    
    Node(T val) : data(val), next(nullptr) {}
    Node() : next(nullptr) {} // dummy node
};

template<typename T>
class LockFreeQueue {
    private:
        // 使用__int128作为双宽CAS数据载体
        typedef __int128 AtomicPointerType;

        // 头尾指针封装
        struct PtrCount {
            Node<T>* ptr;
            uint64_t count;
        };

        std::atomic<AtomicPointerType> head;
        std::atomic<AtomicPointerType> tail;

        // 将PtrCount打包成__int128
        static AtomicPointerType pack(PtrCount pc) {
            AtomicPointerType val = 0;
            // 低64位放指针， 高64位放计数器
            uint64_t ptrVal = reinterpret_cast<uint64_t>(pc.ptr);
            val = ptrVal;
            val |= (AtomicPointerType)pc.count << 64;
            return val;
        }

        // 将__int128拆成PtrCount
        static PtrCount unpack(AtomicPointerType val) {
            PtrCount pc;
            pc.ptr = reinterpret_cast<Node<T>*>(static_cast<uint64_t>(val & 0xFFFFFFFFFFFFFFFF));
            pc.count = static_cast<uint64_t>(val >> 64);
            return pc;
        }
    public:
        LockFreeQueue() {
            Node<T>* dummy = new Node<T>();
            PtrCount pc{dummy, 0};
            head.store(pack(pc));
            tail.store(pack(pc));
        }

        ~LockFreeQueue() {
            PtrCount h = unpack(head.load());
            while(h.ptr != nullptr) {
                Node<T>* next = h.ptr->next.load();
                delete h.ptr;
                h.ptr = next;
            }
        }

        void enqueue(const T& value) {
            Node<T>* newNode = new Node<T>(value);
            PtrCount tailOld;
            while (true) {
                // 加载当前的tail并解包为tailOld
                AtomicPointerType tailVal = tail.load(std::memory_order_acquire);
                tailOld = unpack(tailVal);
                // 查看当前尾节点的下一个节点
                Node<T>* tailPtr = tailOld.ptr;
                Node<T>* nextPtr = tailPtr->next.load(std::memory_order_acquire);
                // 检查tail是否在这段时间被其他线程更改
                if (tailVal == tail.load(std::memory_order_acquire)) {
                    // tail是尾节点
                    if (nextPtr == nullptr) {
                        // 尾节点next为空，尝试插入
                        if (tailPtr->next.compare_exchange_weak(nextPtr, newNode,
                                                                std::memory_order_release,
                                                                std::memory_order_relaxed)) {
                            PtrCount newTail{newNode, tailOld.count + 1};
                            tail.compare_exchange_strong(tailVal, pack(newTail),
                                                         std::memory_order_release,
                                                         std::memory_order_relaxed);
                            return;
                    }
                } else {
                    // tail不是尾节点，落后了 -> 帮助推进tail
                    PtrCount newTail{nextPtr, tailOld.count + 1};
                    tail.compare_exchange_strong(tailVal, pack(newTail),
                                                 std::memory_order_release,
                                                 std::memory_order_relaxed);
                    }
                }
            }
        }

        bool dequeue(T& result) {
            PtrCount headOld;
            while(true) {
                AtomicPointerType headVal = head.load(std::memory_order_acquire);
                AtomicPointerType tailVal = tail.load(std::memory_order_acquire);
                headOld = unpack(headVal);
                PtrCount tailOld = unpack(tailVal);

                Node<T>* headPtr = headOld.ptr;
                Node<T>* tailPtr = tailOld.ptr;
                Node<T>* nextPtr = headPtr->next.load(std::memory_order_acquire);

                if (headVal == head.load(std::memory_order_acquire)) {
                    if (headPtr == tailPtr) {
                        if(nextPtr == nullptr) {
                            // head与tail指向同一节点，且dummy的next为空 -> 队列空
                            return false;
                        }
                        // 尾指针落后了，帮助推进tail
                        PtrCount newTail{nextPtr, tailOld.count + 1};
                        tail.compare_exchange_strong(tailVal, pack(newTail),
                                                     std::memory_order_release,
                                                     std::memory_order_relaxed);
                    } else {
                        // 读取数据准备出队
                        result = nextPtr->data;
                        PtrCount newHead{nextPtr, headOld.count + 1};
                        if (head.compare_exchange_strong(headVal, pack(newHead),
                                                         std::memory_order_release,
                                                         std::memory_order_relaxed)) {
                            delete headPtr; // 释放旧dummy节点
                            return true;
                        }
                    }
                }
            }
        }
};

#endif