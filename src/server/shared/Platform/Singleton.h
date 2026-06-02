/*
* This file is part of Project SkyFire https://www.projectskyfire.org.
* See LICENSE.md file for Copyright information
*/

#ifndef SKYFIRE_PLATFORM_SINGLETON_H
#define SKYFIRE_PLATFORM_SINGLETON_H

namespace Skyfire
{
    template <class T>
    T* Singleton()
    {
        static T instance;
        return &instance;
    }
}

#endif
