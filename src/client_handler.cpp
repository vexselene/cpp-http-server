#include "../include/client_handler.h"
#include <iostream>
#include <sys/socket.h>
#include <unistd.h>
#include <cstring>

constexpr int BUFFER_SIZE = 1024;

void handle_client(int client_fd) {
    // echo the message recieved from client
    /*
    #include <unistd.h> // for ssize_t
    
    ssize_t read(int fd, void buf[.count], size_t count);
    
    read() returns the number of bytes read. -1 on error and 0 if connection closed by client
    */
   
    char buffer[BUFFER_SIZE];
    memset(buffer, 0, BUFFER_SIZE); // set buffer to 0 to accept new data
    ssize_t valread = read(client_fd, buffer, BUFFER_SIZE);

    if(valread == -1) {
       perror("read() failed");
    } else if (valread == 0) {
        std::cout << "Connection closed with client!\n";
    } else {
        std::cout << "Recieved: " << buffer << std::endl;

        // sending back messag recieved
        /*
        #include <sys/socket.h>

        ssize_t send(int sockfd, const void buf[.len], size_t len, int flags);
        ssize_t sendto(int sockfd, const void buf[.len], size_t len, int flags,
                        const struct sockaddr *dest_addr, socklen_t addrlen);
        ssize_t sendmsg(int sockfd, const struct msghdr *msg, int flags);

        send() - Use when: Socket is already connect()-ed 
        Sends data to the pre-connected peer (no address needed)

        sendto() - Use when: Socket is not connected, or you want to send to different addresses
        Where it sends: To the specific dest_addr you provide

        sendmsg() - Most flexible
        ssize_t sendmsg(int sockfd, const struct msghdr *msg, int flags);
        Use when: Need advanced features (multiple buffers, file descriptors, complex addressing)

        flags parameter (common values)
            0 - Normal operation
            MSG_DONTWAIT - Non-blocking send
            MSG_NOSIGNAL - Don't send SIGPIPE on broken connection

        */

        send(client_fd, buffer, valread, 0);
        std::cout << "Sending back: " << buffer << std::endl;
    }

    // close client socket
    close(client_fd);    
}