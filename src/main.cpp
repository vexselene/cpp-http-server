#include "../include/thread_pool.h"
#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netdb.h>
#include <unistd.h>
#include <string>
#include <cstring>
#include <cerrno>

#include <signal.h> // used for catching termination signal
#include <atomic>

std::atomic<bool> keep_running(true);

constexpr int PORT = 8080;

// handle signal
void handle_sigint(int) {keep_running = false;}

int main() {

    // The sigaction() system call is used to change the action taken by a process on receipt of a specific signal.
    struct sigaction sa; // sigaction is a struct that describes how to handle a signal.
    sa.sa_handler = handle_sigint; // tells which function to call
    
    // The sigemptyset() function initializes the signal set pointed to by set, such that all signals defined in POSIX.1‐2008 are excluded.
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0; // Extra options for behavior.

    sigaction(SIGINT, &sa, nullptr);

    // create a socket
    // int socket(int domain, int type, int protocol);
    // AF_INET = IPv4, SOCK_STREAM = TCP (as opposed to UDP which is SOCK_DGRAM)
    int server_sfd = socket(AF_INET, SOCK_STREAM, 0);
    if(server_sfd == -1) {
        perror("socket() failed");
        return 1;
    }
    std::cout << "Socket created with file descripter: " << server_sfd << std::endl;

    int optval = 1; // enable 
    // without this the OS holds the port for ~60 seconds after the server exits
    if(setsockopt(server_sfd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &optval, sizeof(optval)) == -1) {
        std::cerr << "Setsockopt error\n";
        return 1;
    }

    struct sockaddr_in socket_addr;
    socket_addr.sin_family = AF_INET;
    socket_addr.sin_port = htons(PORT); 
    socket_addr.sin_addr.s_addr = INADDR_ANY;
    socklen_t socketaddr_len = sizeof(socket_addr);

    // bind to the port
    if(bind(server_sfd, (struct sockaddr*)&socket_addr, socketaddr_len) == -1) {
        perror("bind() failed");
        return 1;
    }
    std::cout << "Socket binding successful" << std::endl;

    // listen : listen for connections on a socket --- Only for connection-oriented sockets (TCP, not UDP: UDP is connectionless)
    // tell the os only once to start listening 
    if(listen(server_sfd, 3) == -1) {
        perror("listen() failed");
        return 1;
    }
    std::cout << "Server listening on port " << PORT << std::endl;
    

    // create a threadpool
    ThreadPool pool(4);

    while(keep_running) { // keep the server running

        // accept connection
        int new_socket = accept(server_sfd, (struct sockaddr*)&socket_addr, &socketaddr_len);
        if (new_socket == -1) {
            if (errno == EINTR) { 
                // interrupted by signal, not a real error
                break;
            }
            perror("accept() failed");
            return 1;
        }
        
        // set 5 second timeout on the clients
        struct timeval timeout;
        timeout.tv_sec = 5;  // sec
        timeout.tv_usec = 0; // microsec

        setsockopt(new_socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

        pool.enqueue(new_socket);
    }
    std::cout << "\nShutting down server...\n";
    close(server_sfd); // close listening socket
    std::cout << "Server shut down cleanly.\n";
    return 0;
}