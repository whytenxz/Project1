#pragma once

// Lazy singleton for manual-map injection: avoids CRT .CRT$XCU static init.
template<typename T>
class lazy_ptr {
    mutable T* instance_ = nullptr;

public:
    lazy_ptr() = default;
    lazy_ptr(const lazy_ptr&) = delete;
    lazy_ptr& operator=(const lazy_ptr&) = delete;

    ~lazy_ptr() {
        delete instance_;
        instance_ = nullptr;
    }

    T* get() const {
        if (!instance_)
            instance_ = new T();
        return instance_;
    }

    T* operator->() const { return get(); }
    T& operator*() const { return *get(); }

    explicit operator bool() const { return instance_ != nullptr; }
};
