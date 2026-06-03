/*
* This file is part of Project SkyFire https://www.projectskyfire.org.
* See LICENSE.md file for Copyright information
*/

#ifndef SKYFIRE_PLATFORM_SINGLETON_H
#define SKYFIRE_PLATFORM_SINGLETON_H

#include "Platform/Threading.h"

namespace Skyfire
{
    template <class T, class Lock = NullMutex>
    class Singleton
    {
    public:
        static T* instance()
        {
            static T instance;
            return &instance;
        }
    };
}

#endif
