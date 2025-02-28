#include "launcher.hpp"
#include "util.hpp"

#include <iostream>
#include <fstream>
#include <filesystem>
#include <list>
#include <unordered_map>

namespace fs = std::filesystem;

namespace {

inline void addFile(const fs::directory_entry &entry, std::list<fs::path> &files) {
    //".desktop";
    if(!entry.is_regular_file()) return;
    const auto filename = entry.path().filename().generic_string();
    const uint N = filename.size();
    if(N < 9) return;
    if(filename.substr(filename.size()-8,8) != ".desktop") return;
    files.push_back(entry);
}

std::list<fs::path> getApplicationEntries() {
    std::list<fs::path> files;
    for (const auto &entry : fs::directory_iterator("/usr/share/applications")) addFile(entry, files);
    for (const auto &entry : fs::directory_iterator(
        std::string(std::getenv("HOME"))+"/.local/share/applications")) addFile(entry, files);
    return files;
}

//returns false if the files stream ended
bool nextLine(std::ifstream &file) {
    char next;
    while(file.get(next)) {
        if(next == '\n') return true;
    }
    return false;
}

bool parseField(std::ifstream &file, std::unordered_map<std::string, std::string> &fields) {
    if(file.peek() == '#') return nextLine(file);

    char next;
    std::string key = "";
    std::string value = "";
    while(true) {
        if(!file.get(next)) return false;
        if(next == '=') break;
        key.push_back(next);
    }
    while(true) {
        if(!file.get(next)) return false;
        if(next == '\n') break;
        value.push_back(next);
    }

    std::cout << "key=" << key << "; value=" << value << ";" << std::endl;
    return true;
}

bool parseDesktopEntry(std::ifstream &file) {
    //get entry type
    while(file.peek() != '[') if(!nextLine(file)) return false;
    if(!file.get()) return false;

    char next;
    std::string name = "";
    while(true) {
        if(!file.get(next)) return false;
        if(next == ']') break;
        name.push_back(next);
    }
    
    if(!nextLine(file)) return false;

    std::cout << name << std::endl;
    if(name != "Desktop Entry") return true;

    std::unordered_map<std::string, std::string> fields; //mit dem passiert no nüt
    while(file.peek() != '[') if(!parseField(file, fields)) return false;
    return true;
}

void parseFile(fs::path &file_path) {
    std::ifstream file(file_path.string());
    if (!file.is_open()) {
        error("failed to open file: {}", file_path.filename().string());
        return;
    }

    while(parseDesktopEntry(file));

    if(!file.eof()) warn("end of file not reached: {}", file_path.filename().string());
}

}

namespace Launcher {
    void setup() {
        auto files = getApplicationEntries();
        for(auto &file : files) parseFile(file);
    }
}