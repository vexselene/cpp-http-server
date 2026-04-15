#pragma once
#include <cstring>
#include <string>
#include <sstream>
#include <unordered_map>

struct Request {
    std::string method;
    std::string path;
    std::string version;
};

void handle_client(int client_fd, const std::string& web_root);
void parse_headers(std::unordered_map<std::string, std::string>& headers, 
                    std::istringstream& stream, std::string& line);
Request parse_request(std::istringstream& stream, std::string& line);
ssize_t send_all(int sock, const char* data, size_t length);
std::string get_content_type(const std::string& path);
void respond(int client_fd, int status, const std::string& body, bool keep_alive, const std::string& content_type = "text/html");
std::string load_file(const std::string& path);
std::string resolve_path(const std::string& p);
void serve_file(int client_fd, const std::string& web_root, const std::string& path, bool keep_alive);
void route_request(int client_fd, const Request& rq, const std::string& web_root, bool keep_alive);
void send_error(int client_fd, int status, const std::string& web_root, bool keep_alive);