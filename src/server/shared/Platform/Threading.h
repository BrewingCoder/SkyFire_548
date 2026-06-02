/*
* This file is part of Project SkyFire https://www.projectskyfire.org.
* See LICENSE.md file for Copyright information
*/

#ifndef SKYFIRE_PLATFORM_THREADING_H
#define SKYFIRE_PLATFORM_THREADING_H

#include <mutex>

namespace Skyfire
{
    using Mutex = std::mutex;
    using RecursiveMutex = std::recursive_mutex;

    class NullMutex
    {
    public:
        void lock() { }
        bool try_lock() { return true; }
        void unlock() { }
    };
}

#endif
