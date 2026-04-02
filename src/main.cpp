#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <string>

constexpr int PORT = 8080;
constexpr int BUFFER_SIZE = 1024;

int main(int argc, char* argv[]) {
    // create a socket
    // int socket(int domain, int type, int protocol);
    // AF_INET = IPv4, SOCK_STREAM = TCP (as opposed to UDP which is SOCK_DGRAM)
    int server_sfd = socket(AF_INET, SOCK_STREAM, 0);
    if(server_sfd == -1) {
        std::cerr << "Socket Failed" << std::endl;
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
        std::cerr << "Bind Faliure\n";
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

    if(listen(server_sfd, 3) == -1) {
        std::cerr << "Listen error\n";
        return 1;
    }
    std::cout << "Server listening on port " << PORT << std::endl;

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
    if(new_socket == -1) {
        std::cerr << "Accepting connection failed.\n";
        return 1;
    }
    // echo the message recieved from client
    /*
    #include <unistd.h> // for ssize_t

    ssize_t read(int fd, void buf[.count], size_t count);

    read() returns the number of bytes read. -1 on error and 0 if connection closed by client
    */

    char buffer[BUFFER_SIZE] = {0};
    ssize_t valread = read(new_socket, buffer, BUFFER_SIZE);
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

    ssize_t valsent = send(new_socket, buffer, valread, 0);
    std::cout << "Message echoed back successfully.\n";

    // close sockets
    close(new_socket);
    close(server_sfd);
    return 0;
}