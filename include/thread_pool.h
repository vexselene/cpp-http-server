#pragma once
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <functional>
#include <unordered_set>

class ThreadPool {
private:
    void worker_loop();                               // what each worker therad runs

    std::vector<std::thread> workers;                 // threads (thread_pool) - (fixed during initialization)
    std::queue<std::function<void()>> task_queue;     // pending client's file descriptors
    std::mutex queue_mutex;                           // mutex lock to protect client_queue
    std::condition_variable cv;                       // for sleeping/waking workers
    std::atomic<bool> keep_running;                   // flag for keeping the server running and graceful shutdown

    // track fds currently being handled so we can forcibly close them on shutdown
    std::unordered_set<int> active_fds;
    std::mutex active_fds_mutex;


public:
    ThreadPool(int num_threads);      // creates thread
    ~ThreadPool();                    // destructor - shutsdown cleanly
    
    void enqueue(std::function<void()> task);     // used in main thread to add clients to client_queue for workers 
    void register_fd(int fd);    // called before task starts
    void unregister_fd(int fd);  // called after task finishes
};