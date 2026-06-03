/*
* This file is part of Project SkyFire https://www.projectskyfire.org.
* See LICENSE.md file for Copyright information
*/

#ifndef SKYFIRE_PLATFORM_THREADING_H
#define SKYFIRE_PLATFORM_THREADING_H

#include <mutex>
#include <shared_mutex>

namespace Skyfire
{
    using RecursiveMutex = std::recursive_mutex;
    using SharedMutex = std::shared_mutex;

    class Mutex
    {
    public:
        void lock() { _mutex.lock(); }
        bool try_lock() { return _mutex.try_lock(); }
        void unlock() { _mutex.unlock(); }
        int acquire() { lock(); return 0; }
        int tryacquire() { return try_lock() ? 0 : -1; }
        int release() { unlock(); return 0; }

    private:
        std::mutex _mutex;
    };

    class NullMutex
    {
    public:
        void lock() { }
        bool try_lock() { return true; }
        void unlock() { }
        int acquire() { return 0; }
        int tryacquire() { return 0; }
        int release() { return 0; }
    };
}

#endif
