#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <string>
#include <cstring>
#include <cerrno>

#include <signal.h> // catching termination signal
#include <atomic>

std::atomic<bool> keep_running(true);

constexpr int PORT = 8080;
constexpr int BUFFER_SIZE = 1024;

//handle signal
void handle_sigint(int) {keep_running = false;}

int main() {

    /*
        The sigaction() system call is used to change the action taken by a process on receipt of a specific signal.
        The sigaction structure is defined as something like:

            struct sigaction {
                void     (*sa_handler)(int);
                void     (*sa_sigaction)(int, siginfo_t *, void *);
                sigset_t   sa_mask;
                int        sa_flags;
                void     (*sa_restorer)(void);
            };
        
    */

    struct sigaction sa; // sigaction is a struct that describes how to handle a signal.
    sa.sa_handler = handle_sigint; // tells which function to call
    /*The sigemptyset() function initializes the signal set pointed to
       by set, such that all signals defined in POSIX.1‐2008 are
       excluded.
       
       What sa_mask controls is something different — it's about what happens during the 2-3 microseconds your handler function is actually executing. 
       Should other signals be blocked in that tiny window? 
       For our simple handler that just sets a bool, it doesn't matter — it's so fast nothing can interfere. 
       So we set it to empty. 
       Before sigemptyset():
        sigset_t = [???? ???? ???? ????]  (garbage/unknown) - the bits represents signal like SIGINT, SIGTERM, etc

        After sigemptyset():
        sigset_t = [0000 0000 0000 0000]  (all zeros = empty)*/
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0; /*Extra options for behavior. 
    Some useful flags exist like SA_RESTART which automatically restarts interrupted system calls. 
    Setting it to 0 means default behavior — no extras. 
    We explicitly want interrupted system calls to return EINTR so our loop can detect the signal, so 0 is correct here.*/

    /*sigaction(SIGINT, &sa, nullptr);
    This is the actual system call that registers everything with the kernel. You're saying:
        SIGINT — handle this specific signal
        &sa — using these instructions
        nullptr — we don't care about saving the old handler 
        (you can pass a pointer here if you want to restore the previous behavior later)*/
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
    
    /*
    int setsockopt(int sockfd, int level, int optname,
                    const void optval[.optlen],
                    socklen_t optlen);

    Parameters:
        sockfd - Socket file descriptor (what you got from socket())

        level - Protocol level to set option at:
            SOL_SOCKET - Socket-level options
            IPPROTO_TCP - TCP-specific options
            IPPROTO_IP - IP-specific options

        optname - Which option to set:
            SO_REUSEADDR - Reuse address
            SO_REUSEPORT - Reuse port
            SO_KEEPALIVE - Enable keep-alive probes
            SO_RCVTIMEO - Receive timeout

        optval - Pointer to value (usually int but can be other types)
    Return val: 0 = success , -1 = failure.

    level: Level tells the kernel which layer or which subsystem the option belongs to.
                    Application
                        ↑
                    Transport (TCP/UDP)  ← IPPROTO_TCP, IPPROTO_UDP
                        ↑
                    Network (IP)         ← IPPROTO_IP
                        ↑
                    Socket Layer        ← SOL_SOCKET
            SOL_SOCKET has following options :  SO_REUSEADDR - Reuse address/port
                                                SO_KEEPALIVE - Enable TCP keep-alive
                                                SO_RCVBUF - Receive buffer size
                                                SO_SNDBUF - Send buffer size
                                                SO_RCVTIMEO - Receive timeout
    */
    // without this the OS holds the port for ~60 seconds after the server exits
    int optval = 1; // enable 
    if(setsockopt(server_sfd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &optval, sizeof(optval)) == -1) {
        std::cerr << "Setsockopt error\n";
        return 1;
    }

    /* 
    struct sockaddr {
        unsigned short sa_family;  // address family
        char sa_data[14];          // raw data - hard to use!
    };

    // struct sockaddr is generic parent type thus it is impractical to use directly 
    // You'd have to manually pack/unpack IP addresses and ports into sa_data[14]. Ugly and error-prone.
    // thus use sockaddr_in fo incomming address

    struct sockaddr_in {
        short sin_family;          // AF_INET
        unsigned short sin_port;   // port (network byte order)
        struct in_addr sin_addr;   // IP address
        char sin_zero[8];          // padding
    };

    htons : htons() = Host TO Network Short (converts to network byte order).
            Network byte order is big-endian (most significant byte first).
            Without this, port would be wrong on little-endian machines (Intel, ARM).
            
    INADDR_ANY = bind to all available network interfaces (0.0.0.0)
    Accept connections on any network card (WiFi, Ethernet, localhost)
    Alternative: inet_addr("192.168.1.100") for specific IP
    */
    struct sockaddr_in socket_addr;
    socket_addr.sin_family = AF_INET;
    socket_addr.sin_port = htons(PORT); 
    socket_addr.sin_addr.s_addr = INADDR_ANY;
    socklen_t socketaddr_len = sizeof(socket_addr);
    /*
    int bind(int sockfd, const struct sockaddr *addr,
                socklen_t addrlen);
    
    bind takes generic sockaddr : type caste sockaddr_in to sockaddr
    (struct sockaddr*)&addr = cast to generic type (API requirement)
    sizeof(addr) = tells kernel how much data to read (16 bytes for IPv4)
    */
    if(bind(server_sfd, (struct sockaddr*)&socket_addr, socketaddr_len) == -1) {
        perror("bind() failed");
        return 1;
    }
    std::cout << "Socket binding successful" << std::endl;
    // listen : listen for connections on a socket
    /*
    int listen(int sockfd, int backlog); // ret -> success 0, error -1
    
    listen() marks the socket referred to by sockfd as a passive socket,
    that is, as a socket that will be used to accept incoming connection requests using accept(2).
    The sockfd argument is a file descriptor that refers to a socket of type SOCK_STREAM or SOCK_SEQPACKET.
    The  backlog  argument  defines  the maximum length to which the queue of pending connections for sockfd may grow.
    If a connection request arrives when the queue is full, the client may receive an error with an indication of
    ECONNREFUSED or, if the underlying protocol supports retransmission, the request may be ignored so that a later reattempt at connection succeeds.
    
    basically it, 
    Marks the socket as passive (server) and tells the kernel:

    "I'm ready to accept incoming connections"

    "Queue up to backlog pending connections while I'm busy"
    
    Pending connections = those that connected but haven't been accept()-ed yet
    Common values: 5, 10, 128, SOMAXCONN (system max)
    Modern Linux caps at /proc/sys/net/core/somaxconn (default 128)
    
    Only for connection-oriented sockets (TCP, not UDP)

    UDP doesn't need listen() - it's connectionless

    Must call after bind() and before accept()
    */
    // tell the os only once to start listening 
    if(listen(server_sfd, 3) == -1) {
        perror("listen() failed");
        return 1;
    }
    std::cout << "Server listening on port " << PORT << std::endl;
    // create a buffer to store incomming data
    while(keep_running) { // keep the server running
        // accept connection
        /*
        The  accept()  system  call  is used with connection-based socket types
        (SOCK_STREAM, SOCK_SEQPACKET).  It extracts the  first  connection  re‐
        quest  on  the  queue  of pending connections for the listening socket,
        sockfd, creates a new connected socket, and returns a new file descrip‐
        tor referring to that socket.  The newly created socket is not  in  the
        listening  state.   The  original  socket  sockfd is unaffected by this
        call.
        
        int accept(int sockfd, struct sockaddr *_Nullable restrict addr,
        socklen_t *_Nullable restrict addrlen);
        
        
        What it does:
        Takes the first pending connection from the queue
        Creates a brand new socket for that specific client
        Returns a new file descriptor for communication with that client
        Leaves the original socket untouched (still listening for more)
        
        Parameters:
        sockfd - The listening socket (the one you called listen() on)
        addr - Pointer to store client's address (can be NULL if you don't care)
        */
       
       int new_socket = accept(server_sfd, (struct sockaddr*)&socket_addr, &socketaddr_len);
       if (new_socket == -1) {
           if (errno == EINTR) { // EINTR  The system call was interrupted by a signal that was caught
            // before a valid connection arrived;
            // interrupted by signal, not a real error
            break;
        }
        perror("accept() failed");
        return 1;
    }
    // echo the message recieved from client
    /*
    #include <unistd.h> // for ssize_t
    
    ssize_t read(int fd, void buf[.count], size_t count);
    
    read() returns the number of bytes read. -1 on error and 0 if connection closed by client
    */
   
   char buffer[BUFFER_SIZE];
   memset(buffer, 0, BUFFER_SIZE); // set buffer to 0 to accept new data
   ssize_t valread = read(new_socket, buffer, BUFFER_SIZE);
   
   if(valread == -1) {
       perror("read() failed");
            close(new_socket);
            continue;
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
    
            send(new_socket, buffer, valread, 0);
            std::cout << "Sending back: " << buffer << std::endl;
        }

        // close client socket
        close(new_socket);
    }
    std::cout << "\nShutting down server...\n";
    close(server_sfd); // close listening socket
    std::cout << "Server shut down cleanly.\n";
    return 0;
}