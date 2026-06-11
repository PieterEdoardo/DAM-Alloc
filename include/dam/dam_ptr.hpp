#pragma once
#include "dam.h"
#include <utility>
#include <new>

namespace dam {
    template <typename T>
    class unique_ptr {
        T* ptr;

    public:
        // malloc
        explicit unique_ptr(T* p = nullptr) noexcept : ptr(p) {}

        // free
        ~unique_ptr() noexcept {
            if (ptr) dam_free(ptr);
        }

        // no copy
        unique_ptr(const unique_ptr&) = delete;
        unique_ptr& operator=(const unique_ptr&) = delete;

        // move
        unique_ptr(unique_ptr&& other) noexcept : ptr(other.ptr) {
            other.ptr = nullptr;
        }
        unique_ptr& operator=(unique_ptr&& other) noexcept {
            if (this != &other) {
                if (ptr) dam_free(ptr);
                ptr = other.ptr;
                other.ptr = nullptr;
            }
            return *this;
        }

        T* release() noexcept {
            T* temp = ptr;
            ptr = nullptr;
            return temp;
        }

        void reset(T* p = nullptr) noexcept {
            if (ptr) dam_free(ptr);
            ptr = p;
        }

        T* get() const { return ptr; }
        T& operator*() const { return *ptr; }
        T* operator->() const { return ptr; }

        explicit operator bool() const noexcept { return ptr != nullptr; }
    };

    template<typename T, typename... Args>
    unique_ptr<T> make_unique(Args&&... args) {
        T* ptr = static_cast<T*>(dam_malloc(sizeof(T)));
        if (!ptr) return unique_ptr<T>(nullptr);
        new (ptr) T(std::forward<Args>(args)...);
        return unique_ptr<T>(ptr);
    }




}
