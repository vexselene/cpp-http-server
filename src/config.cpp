#include "../include/config.h"
#include <fstream>
#include <iostream>
#include <string>
#include <cctype>

static inline std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, last - first + 1);
}

Config load_config(const std::string& filepath) {
    Config cfg;
    
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "Warning: Could not open " << filepath 
                  << ", using default values\n";
        return cfg;
    }
    
    std::string line;
    int line_num = 0;
    
    while (std::getline(file, line)) {
        line_num++;
        
        // Remove leading/trailing whitespace
        line = trim(line);
        
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#') continue;
        
        // Find equals sign
        size_t eq_pos = line.find('=');
        if (eq_pos == std::string::npos) {
            std::cerr << "Warning: Line " << line_num << " invalid (no '='), skipping\n";
            continue;
        }
        
        // Extract key and value
        std::string key = trim(line.substr(0, eq_pos));
        std::string value = trim(line.substr(eq_pos + 1));
        
        // Assign based on key
        if (key == "port") {
            try {
                cfg.port = std::stoi(value);
            } catch (...) {
                std::cerr << "Error: Invalid port value '" << value << "'\n";
            }
        }
        else if (key == "threads") {
            try {
                cfg.threads = std::stoi(value);
            } catch (...) {
                std::cerr << "Error: Invalid threads value '" << value << "'\n";
            }
        }
        else if (key == "web_root") {
            cfg.web_root = value;
        }
        else {
            std::cerr << "Warning: Unknown key '" << key << "' at line " << line_num << '\n';
        }
    }
    
    return cfg;
}