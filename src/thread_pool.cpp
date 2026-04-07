#include "../include/thread_pool.h"
#include "../include/client_handler.h"
#include <iostream>

ThreadPool::ThreadPool(int num_threads) {
    keep_running = true;

    for(int i = 0; i < num_threads; i++) {
        /*
        all these workers (num_threads threads) are spawned threads in current object (referrenced by "this")
        std::thread thread_name(callable, arg1, arg2, ...);
        
        this = &pool
            So:
                Thread 1 → pool.worker_loop()
                Thread 2 → pool.worker_loop()
                Thread 3 → pool.worker_loop()
            Same object. Same memory.

        use of "this" as argument in std::thread
            std::thread(&ThreadPool::worker_loop, this);
            above line becomes 
                &ThreadPool::worker_loop(this)
            it looks broken but works because member functions are special.

            for member functions, C++ internally turns this into:
                (this->* &ThreadPool::worker_loop)()
            Which is just a fancy way of saying: this->worker_loop();

        */
        workers.push_back(std::thread(&ThreadPool::worker_loop, this));
    }
}

ThreadPool::~ThreadPool() {
    //tell all workers to stop
    keep_running = false;

    cv.notify_all(); // notify all workers to ee keep_running is false now

    // wait for all threads to exit before detructing
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

        //sleep until there is some work to do or we are shutting down.
        cv.wait(lock, [this]{return !client_queue.empty() || !keep_running;});
    
        // awake - check if we are shutting down and nothing left to do
        if(!keep_running && client_queue.empty()) return;
        // else do some work by taking work from client_queue
        int client_fd = client_queue.front();
        client_queue.pop();

        // unlock before handling so hat other workers can grab work from queue simultaneously 
        lock.unlock();
        handle_client(client_fd);
    }
}