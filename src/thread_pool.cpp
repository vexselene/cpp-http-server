#include "../include/thread_pool.h"
#include <iostream>

ThreadPool::ThreadPool(int num_threads) {
    keep_running = true;

    for(int i = 0; i < num_threads; i++) {
        // create a thread pool
        workers.push_back(std::thread(&ThreadPool::worker_loop, this));
    }
}

ThreadPool::~ThreadPool() {
    //tell all workers to stop
    keep_running = false;

    cv.notify_all(); // notify all workers that keep_running is now false

    // wait for all threads to exit before destroying
    for(std::thread& t : workers) {
        t.join(); 
    }
}

void ThreadPool::enqueue(std::function<void()> task) {
    {
        std::lock_guard<std::mutex> lock(queue_mutex);
        task_queue.push(task);
    } // automatically unlocked here

    cv.notify_one(); // wake one worker thread
}

void ThreadPool::worker_loop() {
    while(true) {
        // unique_lock works similar to lock_guard but is more flexible as it allows for custom unlocking
        std::unique_lock<std::mutex> lock(queue_mutex);

        // Wait until either: queue has work, OR server is shutting down
        cv.wait(lock, [this]{return !task_queue.empty() || !keep_running;});
    
        // If shutting down and queue is empty, exit this worker
        if(!keep_running && task_queue.empty()) return;

        // Take work from queue
        auto task = task_queue.front();
        task_queue.pop();
        // Unlock before handling so other workers can grab work from queue simultaneously
        lock.unlock();
        task(); // just call it, no handle_client directly
    }
}