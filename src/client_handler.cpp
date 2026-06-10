#include "../include/client_handler.h"
#include <iostream>
#include <sys/socket.h>
#include <unistd.h>
#include <cstring>
#include <string>
#include <sstream>
#include <unordered_map>
#include <fstream>
#include <algorithm>
#include <sys/un.h>  // Unix domain sockets

constexpr int BUFFER_SIZE = 1024;

void parse_headers(std::unordered_map<std::string, std::string>& headers, 
                    std::istringstream& stream, std::string& line) {
    while (std::getline(stream, line) && line != "\r" && !line.empty()) {
        std::string::size_type colon = line.find(':');
        if (colon == std::string::npos) continue;

        std::string key   = line.substr(0, colon);
        std::transform(key.begin(), key.end(), key.begin(), ::tolower);
        std::string value = line.substr(colon + 1);

        // trim leading space and trailing \r
        if (!value.empty() && value[0] == ' ')    value.erase(0, 1);
        if (!value.empty() && value.back() == '\r') value.pop_back();

        headers[key] = value;
    }
}

Request parse_request(std::istringstream& stream, std::string& line) {
    std::string method;
    std::string path;
    std::string version;

    std::getline(stream, line); // first line
    std::istringstream first_line(line);
    first_line >> method >> path >> version;

    Request rq{method, path, version};
    return rq;
}

ssize_t send_all(int sock, const char* data, size_t length) {
    size_t total = 0;

    while (total < length) {
        ssize_t sent = send(sock, data + total, length - total, 0);
        if (sent <= 0) return sent;
        total += sent;
    } return total;
}

// determine content type from file extension
std::string get_content_type(const std::string& path) {
    if (path.ends_with(".html")) return "text/html";
    if (path.ends_with(".css"))  return "text/css";
    if (path.ends_with(".js"))   return "application/javascript";
    if (path.ends_with(".png"))  return "image/png";
    if (path.ends_with(".jpg"))  return "image/jpeg";
    if (path.ends_with(".ico"))  return "image/x-icon";
    if (path.ends_with(".svg")) return "image/svg+xml";
    return "application/octet-stream"; // instead of "text/plain"
}
void respond(int client_fd, int status, const std::string& body, bool keep_alive,
             const std::string& content_type) {

    std::string status_text;

    if (status == 200) status_text = "200 OK";
    else if (status == 400) status_text = "400 Bad Request";
    else if (status == 404) status_text = "404 Not Found";
    else if (status == 405) status_text = "405 Method Not Allowed";
    else status_text = "500 Internal Server Error";

    std::string response =
        "HTTP/1.1 " + status_text + "\r\n"
        "Content-Length: " + std::to_string(body.size()) + "\r\n"
        "Content-Type: " + content_type + "\r\n"
        "Connection: " + std::string(keep_alive ? "keep-alive" : "close") + "\r\n";
    if (keep_alive) {
        response += "Keep-Alive: timeout=5, max=100\r\n";
    }
    response += "\r\n" + body;

    ssize_t sent = send_all(client_fd, response.c_str(), response.size());

    // log
    std::cout << "[RESP] " << status 
          << " | bytes=" << body.size()
          << " | " << (keep_alive ? "KA" : "CLOSE") << "\n";

    if (sent == -1) perror("send() failed");
}

// load file
std::string load_file(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        std::cerr << "Failed to open: " << path << "\n";
        return "";
    }

    std::ostringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

// resolve path
std::string resolve_path(const std::string& p) {
    std::string path = p;
    
    if(path == "/") return "/index.html";

    if(!path.empty() && path.back() == '/') 
        path += "index.html";
    
    if(path.find('.') == std::string::npos)
        path += ".html";
    return path;
}

void send_error(int client_fd, int status, const std::string& web_root, bool keep_alive) {
    std::string path;

    if (status == 400) path = "/400.html";
    else if (status == 404) path = "/404.html";
    else if (status == 405) path = "/405.html";

    std::string body = load_file(web_root + path);

    if (body.empty()) {
        body = "<h1>" + std::to_string(status) + " Error</h1>";
    }

    respond(client_fd, status, body, keep_alive, "text/html");
}

// serve any file 
void serve_file(int client_fd, const std::string& web_root, const std::string& path, bool keep_alive) {
    std::string resolved_path = resolve_path(path);
    std::string full_path = web_root + resolved_path;

    std::string body = load_file(full_path);

    if (body.empty()) {
        send_error(client_fd, 404, web_root, keep_alive);
        return;
    }

    respond(client_fd, 200, body, keep_alive, get_content_type(full_path));
}

// route request 
void route_request(int client_fd, const Request& rq, const std::string& web_root, std::string& body, bool keep_alive) {
    if (rq.method.empty() || rq.path.empty() || rq.version.empty()) {
        send_error(client_fd, 400, web_root, keep_alive);
        return;
    }

    if (rq.method == "POST") {
        handle_post(client_fd, rq.path, body, keep_alive);
        return;
    }

    if (rq.method != "GET") {
        send_error(client_fd, 405, web_root, keep_alive);
        return;
    }

    if (rq.path == "/favicon.ico") {
        send_error(client_fd, 404, web_root, keep_alive);
        return;
    }

    if (rq.path.find("..") != std::string::npos) {
        send_error(client_fd, 400, web_root, keep_alive);
        return;
    }

    serve_file(client_fd, web_root, rq.path, keep_alive);
}

void handle_post(int client_fd, const std::string& path, std::string& body, bool keep_alive) {
    if (path == "/api/search") {
        // Connect to Indexer daemon
        int sock = socket(AF_UNIX, SOCK_STREAM, 0);
        if (sock < 0) {
            respond(client_fd, 500, "{\"error\": \"daemon unreachable\"}", keep_alive, "application/json");
            return;
        }
        
        struct sockaddr_un addr;
        addr.sun_family = AF_UNIX;
        strcpy(addr.sun_path, "/tmp/indexer.sock");
        
        if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            close(sock);
            respond(client_fd, 500, "{\"error\": \"daemon unreachable\"}", keep_alive, "application/json");
            return;
        }
        
        // Parse the JSON body to extract query
        // Simple: body is {"query":"math notes"}
        // For now, just send the raw body
        send(sock, body.c_str(), body.size(), 0);
        
        // Read response
        char buffer[4096] = {0};
        int bytes = recv(sock, buffer, sizeof(buffer) - 1, 0);
        close(sock);
        
        if (bytes <= 0) {
            respond(client_fd, 500, "{\"error\": \"no response\"}", keep_alive, "application/json");
            return;
        }
        
        std::string json(buffer, bytes);
        respond(client_fd, 200, json, keep_alive, "application/json");
        return;
    }
    
    respond(client_fd, 404, "{\"error\": \"not found\"}", keep_alive, "application/json");
}

void handle_client(int client_fd, const std::string& web_root) {
    std::cout << "[NEW CONNECTION]\n";
    bool connection_alive = true;
    int handled = 0;
    const int MAX_REQ = 100;
    std::string buffer;

    while (handled < MAX_REQ && connection_alive) {
        while(buffer.find("\r\n\r\n") == std::string::npos) {
            char temp_buffer[BUFFER_SIZE];
            ssize_t valread = -1;
            valread = recv(client_fd, temp_buffer, BUFFER_SIZE, 0);

            if (valread == 0) {
                // client closed connection cleanly
                connection_alive = false;
                break;
            }

            if (valread < 0) {
                // give error for everything but EAGAIN EWOULDBLOCK ECONNRESET adn break regardless
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    // SO_RCVTIMEO fired
                } else {
                    // ECONNRESET, EBADF (from shutdown), or real error
                    if (errno != ECONNRESET)
                        perror("recv failed");
                }
                connection_alive = false;
                break;
            }
            buffer.append(temp_buffer, valread);
        }

        if (buffer.empty()) break;

        size_t pos = buffer.find("\r\n\r\n");
        if (pos == std::string::npos) break;

        std::string request = buffer.substr(0, pos + 4);

        // remove processed part, KEEP rest
        buffer.erase(0, pos + 4);
        
        std::istringstream stream(request);
        std::string line;
        Request rq = parse_request(stream, line);

        // log 
        std::cout << "[REQ] " << rq.method << " " << rq.path << " (" << rq.version << ")\n";
        
        std::unordered_map<std::string, std::string> headers;
        parse_headers(headers, stream, line);
        
        // recieve body
        std::string body;
        auto cl = headers.find("content-length");
        if(cl != headers.end()) {
            size_t content_length = 0;

            try {
                content_length = std::stoi(cl->second);
            } catch (...) {
                connection_alive = false;
                break;
            }

            if (content_length > 1024 * 1024) { // 1MB limit
                std::cout << "Content-length > 1MB\n";
                connection_alive = false;
                break;
            }
            body.resize(content_length);
            
            // recv exactly content_length bytes
            size_t total = 0;
            
            // consume from buffer first (if buffer has left over data)
            if (!buffer.empty()) {
                size_t copy = std::min(buffer.size(), content_length);
                std::memcpy(body.data(), buffer.data(), copy);
                buffer.erase(0, copy);
                total += copy;
            }

            while(total < content_length) {
                ssize_t received = recv(client_fd, body.data() + total, content_length - total, 0); // body.data()  ≈  char* -> gives the pointer to the underlying char buffer
                if(received <= 0) {connection_alive = false;  break;}
                total += received;
            } 
        }

        bool keep_alive = true;

        auto it = headers.find("connection");
        if (it != headers.end()) {
            std::string val = it->second;
            std::transform(val.begin(), val.end(), val.begin(), ::tolower);
            if (val == "close") keep_alive = false;
        } else {
            keep_alive = (rq.version == "HTTP/1.1");
        }
        
        std::cout << "[CONN] keep_alive=" << (keep_alive ? "true" : "false") << "\n";
        
        route_request(client_fd, rq, web_root, body, keep_alive);

        handled++;

        if (!keep_alive) break;
    }
    std::cout << "[CONNECTION CLOSED]\n";
    close(client_fd);
}