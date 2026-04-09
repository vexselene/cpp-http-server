#include "../include/thread_pool.h"
#include "../include/client_handler.h"
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

void ThreadPool::enqueue(int client_fd) {
    {
        std::lock_guard<std::mutex> lock(queue_mutex);
        client_queue.push(client_fd);
    } // automatically unlocked here

    cv.notify_one(); // wake one worker thread
}

void ThreadPool::worker_loop() {
    while(true) {
        // unique_lock works similar to lock_guard but is more flexible as it allows for custom unlocking
        std::unique_lock<std::mutex> lock(queue_mutex);

        // Wait until either: queue has work, OR server is shutting down
        cv.wait(lock, [this]{return !client_queue.empty() || !keep_running;});
    
        // If shutting down and queue is empty, exit this worker
        if(!keep_running && client_queue.empty()) return;

        // Take work from queue
        int client_fd = client_queue.front();
        client_queue.pop();

        // Unlock before handling so other workers can grab work from queue simultaneously
        lock.unlock();
        handle_client(client_fd);
    }
}