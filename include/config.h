#pragma once
#include <string>

struct Config {
    int port          = 8080;
    int threads       = 4;
    std::string web_root = "./static";
};

Config load_config(const std::string& filepath);