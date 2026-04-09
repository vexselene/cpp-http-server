#include "../include/client_handler.h"
#include <iostream>
#include <sys/socket.h>
#include <unistd.h>
#include <cstring>
#include <string>
#include <sstream>
#include <unordered_map>
#include <fstream>

constexpr int BUFFER_SIZE = 1024;

// determine content type from file extension
std::string get_content_type(const std::string& path) {
    if (path.ends_with(".html")) return "text/html";
    if (path.ends_with(".css"))  return "text/css";
    if (path.ends_with(".js"))   return "application/javascript";
    if (path.ends_with(".png"))  return "image/png";
    if (path.ends_with(".jpg"))  return "image/jpeg";
    if (path.ends_with(".ico"))  return "image/x-icon";
    return "text/plain";
}
void respond(int client_fd, int status, const std::string& body, const std::string& content_type = "text/html") {
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

    ssize_t sent = send(client_fd, response.c_str(), response.size(), 0);
    if (sent == -1) perror("send() failed");
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
        perror("read() failed");
        close(client_fd);
        return;
    }
    if (valread == 0) {
        std::cout << "Connection closed with client\n";
        close(client_fd);
        return;
    }
    // parse request
    std::string method;
    std::string path;
    std::string version;
    std::istringstream stream(request);
    std::string line;

    std::getline(stream, line); // first line
    std::istringstream first_line(line);
    first_line >> method >> path >> version;

    // validation 
    if (method.empty() || path.empty() || version.empty()) {
        respond(client_fd, 400, "");
        close(client_fd);
        return;
    }

    // handle annoying browser request
    if (path == "/favicon.ico") {
        respond(client_fd, 404, "");
        close(client_fd);
        return;
    }
    
    // parse headers into map
    std::unordered_map<std::string, std::string> headers;
    while (std::getline(stream, line) && line != "\r" && !line.empty()) {
        std::string::size_type colon = line.find(':');
        if (colon == std::string::npos) continue;

        std::string key   = line.substr(0, colon);
        std::string value = line.substr(colon + 1);

        // trim leading space and trailing \r
        if (!value.empty() && value[0] == ' ')    value.erase(0, 1);
        if (!value.empty() && value.back() == '\r') value.pop_back();

        headers[key] = value;
    }
    
    std::cout << "Method: " << method << "\n";
    std::cout << "Path: " << path << "\n";
    std::cout << "Version: " << version << "\n";

    // basic validation
    if (method != "GET") {
        respond(client_fd, 405, "");
        close(client_fd);
        return;
    }

    // map request path to file on disk
    std::string web_root  = "./static";
    std::string file_path = web_root + path;
    if (path == "/") file_path = web_root + "/index.html";

    // read file
    std::ifstream file(file_path);
    if (!file.is_open()) {
        respond(client_fd, 404, "<html><body><h1>404 Not Found</h1></body></html>");
        close(client_fd);
        return;
    }

    std::string body(
        (std::istreambuf_iterator<char>(file)),
        (std::istreambuf_iterator<char>())
    );

    respond(client_fd, 200, body, get_content_type(file_path));

    close(client_fd);
}