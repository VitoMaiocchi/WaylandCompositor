#pragma once
#include "wlr/util/box.h"
#include <exception>
#include <cassert>
#include <unistd.h> 

//TODO: Extends class mache wo denn Surface, Display, etc inherited
struct Extends : wlr_box {
    Extends() = default;
    Extends(const wlr_box& box) : wlr_box(box) {}
    Extends(int x, int y, int width, int height) : wlr_box({x,y,width,height}) {}

    Extends& setHeight(int height) {
        this->height = height;
        return *this;
    }

    bool contains(int x, int y) {
        if(this->x > x) return false;
        if(this->x + this->width < x) return false;
        if(this->y > y) return false;
        if(this->y + this->height < y) return false;
        return true;
    }

    //limit Extends to constraint area
    Extends& constrain(Extends c) {
        if(this->height > c.height) this->height = c.height;
		if(this->width > c.width) this->width = c.width;

		if(this->x + this->width > c.x + c.width) this->x = c.x + c.width - this->width;
		else if(this->x < c.x) this->x = c.x;

		if(this->y + this->height > c.y + c.height) this->y = c.y + c.height - this->height;
		else if(this->y < c.y) this->y = c.y;
		return *this;
    }
};

inline void spawn(const char* cmd) {
    if (fork() == 0) {
        execl("/bin/sh", "/bin/sh", "-c", cmd, (void *)NULL);
        return;
    }
}

/*
    UTILITY ZÜG WOS NO BRUCHT
    [ ] Extend type mit guete util member functions
    [ ] Ahständigs log system (mit std::format)
*/

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

