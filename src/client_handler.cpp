#include "../include/client_handler.h"
#include <iostream>
#include <sys/socket.h>
#include <unistd.h>
#include <cstring>
#include <string>
#include <sstream>
#include <unordered_map>

constexpr int BUFFER_SIZE = 1024;

void respond(int client_fd, int status, const std::string& body) {
    std::string status_text;

    if (status == 200) status_text = "200 OK";
    else if (status == 400) status_text = "400 Bad Request";
    else if (status == 404) status_text = "404 Not Found";
    else if (status == 405) status_text = "405 Method Not Allowed";
    else status_text = "500 Internal Server Error";

    std::string response =
        "HTTP/1.1 " + status_text + "\r\n"
        "Content-Length: " + std::to_string(body.size()) + "\r\n"
        "Connection: close\r\n";

    if (status == 200) {
        response += "Content-Type: text/html\r\n";
    }

    response += "\r\n" + body;

    // send back response
    ssize_t sent = send(client_fd, response.c_str(), response.size(), 0);
    if (sent == -1) perror("send() failed");
}

void handle_client(int client_fd) {
    // create buffer to hold incoming data
    char buffer[BUFFER_SIZE];
    std::string request;

    ssize_t valread = -1;
    while(true) {  // Read until "\r\n\r\n" marks complete request header
        valread = read(client_fd, buffer, BUFFER_SIZE);
        if(valread <= 0) break;

        request.append(buffer, valread);

        if(request.find("\r\n\r\n") != std::string::npos) break;
    }
    
    if(valread < 0) {
        perror("read() failed");
        close(client_fd);
        return;
    } else if (valread == 0) {
        std::cout << "Connection closed with client\n";
        close(client_fd);
        return;
    } else {
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

        // extract headers
        std::unordered_map<std::string, std::string> header;
        while(std::getline(stream, line) && line != "\r" && !line.empty()) {
            std::string::size_type colon = line.find(':');
            if (colon == std::string::npos) continue;
            /*
            header example -> key : value
            Connection: keep-alive
            */
            std::string key = line.substr(0, colon);
            std::string value = line.substr(colon + 1);

            // trim space and \r
            if (!value.empty() && value[0] == ' ') value.erase(0, 1);
            if (!value.empty() && value.back() == '\r') value.pop_back();
            header[key] = value;
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

        // default response
        std::string body = "<html><body><center><h1>Hello from my HTTP server!</h1></center></body></html>";
        respond(client_fd, 200, body);
    }
    // close client socket
    close(client_fd);
}