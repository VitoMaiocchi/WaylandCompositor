#include "launcher.hpp"
#include "util.hpp"

#include <iostream>
#include <fstream>
#include <filesystem>
#include <list>
#include <unordered_map>

namespace fs = std::filesystem;

namespace {

typedef std::unordered_map<std::string, std::string> Fields;

//returns false if the files stream ended
bool nextLine(std::ifstream &file) {
    char next;
    while(file.get(next)) {
        if(next == '\n') return true;
    }
    return false;
}

bool parseField(std::ifstream &file, Fields &fields) {
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

    fields[key] = value;
    return true;
}

bool parseDesktopEntry(std::ifstream &file, std::list<Fields> &application_entries) {
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
    if(name != "Desktop Entry") return true;
    application_entries.push_front({});
    while(file.peek() != '[') if(!parseField(file, (*application_entries.begin()) )) return false;
    return true;
}

void parseFile(const fs::path &file_path, std::list<Fields> &application_entries) {
    std::ifstream file(file_path.string());
    if (!file.is_open()) {
        error("failed to open file: {}", file_path.filename().string());
        return;
    }

    while(parseDesktopEntry(file, application_entries));
    if(!file.eof()) warn("end of file not reached: {}", file_path.filename().string());
}

inline void addFile(const fs::directory_entry &entry, std::list<Fields> &application_entries) {
    //".desktop";
    if(!entry.is_regular_file()) return;
    const auto filename = entry.path().filename().generic_string();
    const uint N = filename.size();
    if(N < 9) return;
    if(filename.substr(filename.size()-8,8) != ".desktop") return;
    parseFile(entry.path(), application_entries);
}

std::list<Fields> getApplicationEntries() {
    std::list<Fields> application_entries;
    for (const auto &entry : fs::directory_iterator("/usr/share/applications")) addFile(entry, application_entries);
    for (const auto &entry : fs::directory_iterator(
        std::string(std::getenv("HOME"))+"/.local/share/applications")) addFile(entry, application_entries);
    return application_entries;
}

}

namespace Launcher {
    void setup() {
        auto application_entries = getApplicationEntries();
        for(Fields fields : application_entries) {
            std::cout << "\n\n[DESKTOP ENTRY]" << std::endl;
            for(auto f : fields) std::cout << "key=" << f.first << "; value=" << f.second << ";" << std::endl;
        }
    }
}