#pragma once

#include <stack>
#include <memory>
#include "concurrentqueue.h"
#include <boost/lockfree/queue.hpp>
#include <boost/pool/singleton_pool.hpp>

template <typename T>
class VectorPool {
public:
    using VectorPtr = std::shared_ptr<std::vector<T>>;

    VectorPool(std::size_t pool_size = 64) : pool_(pool_size) {}

    ~VectorPool() {
        std::vector<T>* ptr = nullptr;
        while (pool_.pop(ptr)) {
            delete ptr;
        }
    }

    VectorPtr acquire(std::size_t capacity_hint = 256) {

        std::vector<T>* ptr = nullptr;

        if (pool_.pop(ptr)) {
            return VectorPtr(ptr, [this](std::vector<T>* p) {
                this->release(p);
                });
        }
        else {
            auto new_ptr = new std::vector<T>();
            new_ptr->reserve(capacity_hint);
            return VectorPtr(new_ptr, [this](std::vector<T>* p) {
                this->release(p);
                });
        }
    }

    void release(std::vector<T>* ptr) {
        ptr->clear();
        while (!pool_.push(ptr)) {
            // Retry until successful
        }
    }

private:
    boost::lockfree::queue<std::vector<T>*> pool_;
};

/*thread safe*/
template <typename T, std::size_t N = 8192>
class CocurrentPool {
public:

    inline static std::atomic<uint64_t> totalAliveObjectCount = 0;  // 存活对象计数

    ~CocurrentPool() {
        T* ptr = nullptr;
        while (pool.try_dequeue(ptr)) {
            ::operator delete(ptr);
        }
    }
    static void preAllocatePool(std::size_t poolsize = N) {
        void* memory = ::operator new (poolsize * sizeof(T));
        for (size_t i = 0; i < poolsize; ++i) {
            T* ptr = reinterpret_cast<T*>(static_cast<char*>(memory) + i * sizeof(T));
            pool.enqueue(ptr);
        }
    }

    template <typename... Args>
    static T* construct(Args&&... args) {
        T* ptr = nullptr;

        if (pool.try_dequeue(ptr)) {
            totalAliveObjectCount.fetch_add(1);
            new (ptr) T(std::forward<Args>(args)...);
            return ptr;
        }
        preAllocatePool(N);
        return construct(std::forward<Args>(args)...);

    }

    static void destroy(T* ptr) {
        totalAliveObjectCount.fetch_sub(1);
        ptr->~T();
        pool.enqueue(ptr);
    }
private:
    static moodycamel::ConcurrentQueue<T*> pool;

};


template <typename T, std::size_t N>
/*static*/moodycamel::ConcurrentQueue<T*> CocurrentPool<T, N>::pool = moodycamel::ConcurrentQueue<T*>(N);

/*thread safe*/
template <typename T>
class BenchmarkPool {
public:

    template <typename... Args>
    inline static T* construct(Args&&... args) {
        totalAliveObjectCount.fetch_add(1);
        return new T(std::forward<Args>(args)...);
    }
    inline static void destroy(T* ptr) {
        totalAliveObjectCount.fetch_sub(1);
        delete ptr;
    }
public:
    inline static std::atomic<uint64_t>  totalAliveObjectCount = 0;
};


template <typename T>
class ObjectFactoryThreadSafe
{
private:
    struct SharedDeleter
    {
        SharedDeleter() {}
        inline void operator()(T* p) const
        {
            SinglePool::destroy(p);
        }
    };
    inline static SharedDeleter  m_Deleter{};

public:

    typedef boost::singleton_pool<T, sizeof(T)>    SinglePool;

    ObjectFactoryThreadSafe() = default;
    virtual ~ObjectFactoryThreadSafe() {}

    inline static std::atomic<uint64_t> totalAliveObjectCount = 0;  // 存活对象计数

    template<typename... TArgs>
    inline static T* create(TArgs&&... mArgs)
    {
        auto ptr = static_cast<T*>(SinglePool::malloc());
        new(ptr) T(std::forward<TArgs>(mArgs)...);
        ptr->retainAlive();
        totalAliveObjectCount.fetch_add(1);
        return ptr;
    }

    inline static void destroy(T* mObj)
    {
        if (!mObj)
            return;
        mObj->~T();
        totalAliveObjectCount.fetch_sub(1);
        SinglePool::free(mObj);
    }

    template<typename TType = T, typename... TArgs>
    inline std::shared_ptr<TType> make_shared(TArgs&&... mArgs)
    {
        return std::shared_ptr<TType>(this->construct(mArgs...), m_Deleter);
    }
};
