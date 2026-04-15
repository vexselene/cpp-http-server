#include "../include/thread_pool.h"
#include <iostream>
#include <sys/socket.h>
#include <unistd.h>

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

    // forcibly close all sockets that are currently blocked in recv()
    // this causes recv() to return 0/-1 immediately, unblocking the worker
    std::vector<int> fds_to_close;
    {
        std::lock_guard<std::mutex> lock(active_fds_mutex);
        fds_to_close.assign(active_fds.begin(), active_fds.end());
    }

    // shudown withuot holding lock
    for (int fd : fds_to_close) {
        shutdown(fd, SHUT_RDWR); // signal EOF on both directions
        // NOTE: we don't close() here — the worker thread owns the fd
        // and will close() it when handle_client() returns.
        // shutdown() is enough to unblock recv().
    }
    // wake workers sleeping on empty queue
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

void ThreadPool::register_fd(int fd) {
    std::lock_guard<std::mutex> lock(active_fds_mutex);
    active_fds.insert(fd);
}

void ThreadPool::unregister_fd(int fd) {
    std::lock_guard<std::mutex> lock(active_fds_mutex);
    active_fds.erase(fd);
}