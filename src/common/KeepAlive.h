#pragma once

#include <memory>
#include <boost/intrusive_ptr.hpp>
#include <boost/pool/singleton_pool.hpp>

#include <jemalloc/jemalloc.h>
#include <iostream>

struct JemallocAllocator {
    using size_type = std::size_t;          // 表示内存块大小的无符号整数类型
    using difference_type = std::ptrdiff_t; // 用于指针差的有符号整数类型

    static char* malloc(const std::size_t size) {

#ifdef _WIN32
        return static_cast<char*>(je_malloc(size)); 
#else
        return static_cast<char*>(std::malloc(size));
#endif
    }

    static void free(void* const ptr) {
#ifdef _WIN32
        je_free(ptr); 
#else
        std::free(ptr);
#endif
    }
};


template<typename T>
class KeepAliveObject {

public:

    template<typename... TArgs>
    inline static boost::intrusive_ptr<T> make_intrusive(TArgs&&... mArgs)
    {

        return boost::intrusive_ptr<T>(create(mArgs...), false);
    }

    template<typename... TArgs>
    inline static T* create(TArgs&&... mArgs)
    {
#ifdef _WIN32
        auto ptr = static_cast<T*>(je_malloc(sizeof(T)));   //static_cast<T*>(SinglePool::malloc());
#else
        auto ptr = static_cast<T*>(std::malloc(sizeof(T)));
#endif
        new(ptr) T(std::forward<TArgs>(mArgs)...);
        ptr->retainAlive();
        totalAliveObjectCount.fetch_add(1);
        return ptr;
    }

    inline uint32_t retainAlive() const{

        return m_refcount.fetch_add(1) + 1;
    }

    inline void release() const{
        if (m_refcount == 0)
            return;

        auto before_sub = m_refcount.fetch_sub(1);

        if (before_sub == 1) {
            destroy((T*)this);
        }
    }
    inline uint32_t getRefcount() const {
        return m_refcount.load();
    }
    friend void intrusive_ptr_add_ref(T* obj){
        obj->retainAlive();
    }

    friend void intrusive_ptr_release(T* obj){
        if (obj) {
            obj->release();
        }
    }
    friend void intrusive_ptr_add_ref(const T* obj) {
        obj->retainAlive();
    }

    friend void intrusive_ptr_release(const T* obj) {
        if (obj) {
            obj->release();
        }
    }
public:

    inline static std::atomic<uint64_t> totalAliveObjectCount = 0; 

private:
    mutable std::atomic<uint32_t>  m_refcount{ 0 };

    inline static void destroy(T* mObj)
    {
        if (!mObj)
            return;
        mObj->~T();
        totalAliveObjectCount.fetch_sub(1);

#ifdef _WIN32
        je_free(mObj);
#else 
        std::free(mObj);
#endif
    }

};
