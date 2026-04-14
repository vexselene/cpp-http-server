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

void respond(int client_fd, int status, const std::string& body, const std::string& content_type) {
    std::string status_text;

    if (status == 200) status_text = "200 OK";
    else if (status == 400) status_text = "400 Bad Request";
    else if (status == 404) status_text = "404 Not Found";
    else if (status == 405) status_text = "405 Method Not Allowed";
    else status_text = "500 Internal Server Error";

    std::string response =
        "HTTP/1.1 " + status_text + "\r\n"
        "Content-Type: " + content_type + "\r\n"
        "Content-Length: " + std::to_string(body.size()) + "\r\n"
        "Connection: close\r\n"
        "\r\n" + body;

    ssize_t sent = send_all(client_fd, response.c_str(), response.size());
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

void send_error(int client_fd, int status, const std::string& web_root) {
    std::string path;

    if (status == 400) path = "/400.html";
    else if (status == 404) path = "/404.html";
    else if (status == 405) path = "/405.html";

    std::string body = load_file(web_root + path);

    if (body.empty()) {
        body = "<h1>" + std::to_string(status) + " Error</h1>";
    }

    respond(client_fd, status, body, "text/html");
}

// serve any file
void serve_file(int client_fd, const std::string& web_root, const std::string& path) {
    std::string resolved_path = resolve_path(path);
    std::string full_path = web_root + resolved_path;

    std::string body = load_file(full_path);

    if (body.empty()) {
        send_error(client_fd, 404, web_root);
        return;
    }

    respond(client_fd, 200, body, get_content_type(full_path));
}

// route request 
void route_request(int client_fd, const Request& rq, const std::string& web_root) {
    if (rq.method.empty() || rq.path.empty() || rq.version.empty()) {
        send_error(client_fd, 400, web_root);
        return;
    }

    if (rq.method != "GET") {
        send_error(client_fd, 405, web_root);
        return;
    }

    if (rq.path == "/favicon.ico") {
        send_error(client_fd, 404, web_root);
        return;
    }

    if (rq.path.find("..") != std::string::npos) {
        send_error(client_fd, 400, web_root);
        return;
    }

    serve_file(client_fd, web_root, rq.path);
}

void handle_client(int client_fd) {
    // create buffer to hold incoming data
    char buffer[BUFFER_SIZE];
    std::string request;
    ssize_t valread = -1;

    while(true) {  // Read until "\r\n\r\n" --> marks complete request header
        valread = read(client_fd, buffer, BUFFER_SIZE);
        if(valread <= 0) break;

        request.append(buffer, valread);

        if(request.find("\r\n\r\n") != std::string::npos) break;
    }
    
    if(valread < 0) {
        // check if valread == -1 is error or timeout
        if(errno == EAGAIN || errno == EWOULDBLOCK) { 
            std::cout << "Client timed out!\n";
        } else {perror("read() failed");}
        close(client_fd);
        return;
    } if (valread == 0) {
        std::cout << "Connection closed with client\n";
        close(client_fd);
        return;
    }
    
    // parse request
    std::istringstream stream(request);
    std::string line;
    Request rq = parse_request(stream, line);

    // parse headers into map
    std::unordered_map<std::string, std::string> headers;
    parse_headers(headers, stream, line);

    std::cout << "Method: " << rq.method << "\n";
    std::cout << "Path: " << rq.path << "\n";
    std::cout << "Version: " << rq.version << "\n";

    std::string web_root  = "./static";

    route_request(client_fd, rq, web_root);

    close(client_fd);
}