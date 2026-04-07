#pragma once
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <functional>

class ThreadPool {
private:
    void worker_loop();               // what each worker therad runs

    std::vector<std::thread> workers; // threads (thread_pool) - (fixed during initialization)
    std::queue<int> client_queue;     // pending client's file descriptors
    std::mutex queue_mutex;           // mutex lock to protect client_queue
    std::condition_variable cv;       // for sleeping/waking workers
    std::atomic<bool> keep_running;   // flag for keeping the server running and graceful shutdown

public:
    ThreadPool(int num_threads);      // creates thread
    ~ThreadPool();                    // destructor - shutsdown cleanly
    
    void enqueue(int client_fd);      // used in main thread to add clients to client_queue for workers 
};