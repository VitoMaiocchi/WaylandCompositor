#define LOGGER_CATEGORY Logger::LAUNCHER
#include "launcher.hpp"
#include "util.hpp"
#include "output.hpp"
#include "layout.hpp"

#include <iostream>
#include <fstream>
#include <filesystem>
#include <list>
#include <unordered_map>
#include <unordered_set>
#include <ranges>

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
    const char p = file.peek();
    if(p == '#' || p == '\n' || p == ' ') return nextLine(file);

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

std::list<Fields> getUnprocessedApplicationEntries() {
    std::list<Fields> application_entries;
    for (const auto &entry : fs::directory_iterator("/usr/share/applications")) addFile(entry, application_entries);
    for (const auto &entry : fs::directory_iterator(
        std::string(std::getenv("HOME"))+"/.local/share/applications")) addFile(entry, application_entries);
    return application_entries;
}

/*
NoDisplay=true
Name=KNewStuff Dialog
Name=Foot
GenericName=Terminal
Exec=knewstuff-dialog6
TryExec=nvim
Exec=nvim %F
Type=Application
Terminal=false
Keywords=shell;prompt;command;commandline;
*/

struct ApplicationEntry {
    const std::string name;
    const std::string generic_name;
    const std::string exec;
    const std::unordered_set<std::string> search_terms;
    const bool terminal;
};

void processEntry(const Fields& fields, std::list<ApplicationEntry> &entries) {
    auto it = fields.find("NoDisplay");
    if(it != fields.end() && it->second == "true") return; 

    it = fields.find("Type");
    if(it == fields.end()) return;
    if(it->second != "Application") return;

    it = fields.find("Name");
    if(it == fields.end()) return;
    std::string name = it->second;

    it = fields.find("Exec");
    if(it == fields.end()) return;
    std::string exec = it->second;
    uint N = exec.size();
    if(N == 0) return;
    for(size_t i = 0; i < N; i++) {
        if(exec[i] == '%') {
            exec[i] = ' ';
            i++;
            if(i < N) exec[i] = ' ';
            i++;
        }
    }

    it = fields.find("Terminal");
    bool terminal = false;
    if(it != fields.end() && it->second == "true") terminal = true;

    it = fields.find("GenericName");
    std::string generic_name = "";
    if(it != fields.end()) generic_name = it->second;

    std::unordered_set<std::string> search_terms;
    search_terms.insert(name);
    if(generic_name.size() > 0) search_terms.insert(generic_name);
    it = fields.find("Keywords");
    if(it != fields.end() && it->second.size() > 0) {
        std::string keywords = it->second;
        for (auto&& part : keywords | std::views::split(';')) {
            if(part.begin() == part.end()) continue;
            search_terms.emplace(part.begin(), part.end());
        }
    }

    entries.push_back({
        name,
        generic_name,
        exec,
        search_terms,
        terminal
    });
}

std::list<ApplicationEntry> getApplicationEntries() {
    std::list<ApplicationEntry> entries;
    auto application_entries = getUnprocessedApplicationEntries();
    for(Fields &fields : application_entries) processEntry(fields, entries);
    return entries;
}

std::string search_text;

void draw(cairo_t* cr) {
	cairo_set_source_rgb(cr, 0.2, 0.8, 0.6);
	cairo_paint(cr);

	cairo_select_font_face (cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
	cairo_set_font_size (cr, 20.0);

    std::string text = "LAUNCHER";

	cairo_set_source_rgb(cr, 0, 0, 0);
	cairo_move_to (cr, 20.0, 25.0);
	cairo_show_text (cr, text.c_str());
    cairo_move_to(cr, 20, 60);
    cairo_show_text(cr, search_text.c_str());
}

}

namespace Launcher {
    void setup() {
        // auto application_entries = getUnprocessedApplicationEntries();
        // for(Fields fields : application_entries) {
        //     std::cout << "\n\n[DESKTOP ENTRY]" << std::endl;
        //     for(auto f : fields) std::cout << "key=" << f.first << "; value=" << f.second << ";" << std::endl;
        // }

        auto entries = getApplicationEntries();
        for(ApplicationEntry &entry : entries) {
            std::cout << std::format("\n[Desktop Entry]\nname={}\ngeneric_name={}\nexec={}\nterminal={}",
                entry.name,
                entry.generic_name,
                entry.exec,
                entry.terminal
            ) << std::endl;
            std::cout << "searchterms=[";
            for(auto e : entry.search_terms) std::cout << e << "], [";
            std::cout << "]" << std::endl;
        }
    }

    bool running = false;
    Output::Buffer* buffer = nullptr;
    Extends ext;

    void run() {
        assert(!running);
        running = true;
        search_text = "kys";
        buffer = new Output::Buffer();
        ext = Layout::getActiveDisplayDimensions();
        ext = {
            ext.x + ext.width/4,
            ext.y + ext.height/4,
            ext.width / 2,
            ext.height / 2
        };

        buffer->draw(draw, ext);
    }

    void keyPressEvent(xkb_keysym_t sym) {
        debug("Keypress Event sym={}", (char)sym);
        switch(sym) {
            case XKB_KEY_Escape:
                delete buffer;
                buffer = nullptr;
                running = false;
            break;
            case XKB_KEY_BackSpace:
                debug("Keypress Event sym=backspace");
                if(search_text.size() == 0) break;
                search_text.erase(search_text.end()-1);
                buffer->draw(draw, ext);
            break;
            default:
                if (sym < 0x20 || sym > 0xfff) break;
                debug("Keypress Event text");
                search_text += sym;
                buffer->draw(draw, ext);
            break;
        }
    }

    bool isRunning() {
        return running;
    }
}