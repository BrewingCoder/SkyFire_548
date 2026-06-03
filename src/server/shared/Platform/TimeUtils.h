/*
* This file is part of Project SkyFire https://www.projectskyfire.org.
* See LICENSE.md file for Copyright information
*/

#ifndef SKYFIRE_PLATFORM_TIME_H
#define SKYFIRE_PLATFORM_TIME_H

#include "Define.h"

#include <chrono>
#include <ctime>
#include <thread>

namespace Skyfire
{
    inline uint32 GetMSTime()
    {
        using Clock = std::chrono::steady_clock;
        static const Clock::time_point ApplicationStartTime = Clock::now();
        return uint32(std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - ApplicationStartTime).count());
    }

    inline uint32 GetMSTimeDiff(uint32 oldMSTime, uint32 newMSTime)
    {
        if (oldMSTime > newMSTime)
            return (0xFFFFFFFF - oldMSTime) + newMSTime;

        return newMSTime - oldMSTime;
    }

    inline uint32 GetMSTimeDiffToNow(uint32 oldMSTime)
    {
        return GetMSTimeDiff(oldMSTime, GetMSTime());
    }

    inline void SleepFor(uint32 milliseconds)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
    }

    inline bool LocalTime(time_t const& time, tm& result)
    {
#if PLATFORM == PLATFORM_WINDOWS
        return localtime_s(&result, &time) == 0;
#else
        return localtime_r(&time, &result) != nullptr;
#endif
    }
}

#endif
