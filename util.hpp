#pragma once
#include "wlr/util/box.h"
#include <exception>
#include <cassert>

typedef wlr_box Extends;

inline void spawn(const char* cmd) {
    if (fork() == 0) {
        execl("/bin/sh", "/bin/sh", "-c", cmd, (void *)NULL);
        return;
    }
}

//ICH GLAUB DE OPTIONAL POINTER ISCH EN SCHEISS

template <typename T>
class OptionalPointer {
    uint* ref_count = nullptr; //should not be null
    bool* deleted = nullptr; //should not be null
    T* ptr = nullptr; //can be null

    public:
    //OptionalPointer() : ptr(nullptr) {} //nüt

    OptionalPointer(const OptionalPointer &other) : ref_count(other.ref_count), deleted(other.deleted), ptr(other.ptr) {
        assert(ref_count);
        assert(deleted);
        *ref_count += 1;
    }
    OptionalPointer(T* ptr) : ref_count(new uint(1)), deleted(new bool(false)), ptr(ptr) {}
    //template<typename... Args>
    //OptionalPointer(Args&&... args) : ptr(new T(std::forward<Args>(args)...)), ref_count(new uint(1)), deleted(new bool(false)) {}

    ~OptionalPointer() {
        *ref_count -= 1;
        if(ref_count == 0) {
            delete ref_count;
            if(!*deleted) delete ptr;
            delete deleted;
        }
    }

    void destroy() {
        if(*deleted) throw std::runtime_error("Optional Pointer allready deleted");
        *deleted = true;
        assert(ptr);
        delete ptr;
    }

    bool exists() {
        return !*deleted;
    }

    T* operator->() const {
        if(*deleted) throw std::runtime_error("Optional Pointer has been deleted");
        assert(ptr);
        return ptr;
    }

    T* get() const {
        if(*deleted) return nullptr;
        return ptr;
    }

    bool operator==(const OptionalPointer& other) const {
        if (*deleted != *other.deleted) return false;
        if (!*deleted && ptr == other.ptr) return true;
        return false;
    }
};

