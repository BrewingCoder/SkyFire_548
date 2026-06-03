/*
* This file is part of Project SkyFire https://www.projectskyfire.org.
* See LICENSE.md file for Copyright information
*/

#ifndef SF_ADDONHANDLER_H
#define SF_ADDONHANDLER_H

#include "Common.h"
#include "Config.h"
#include "Platform/Singleton.h"
#include "WorldPacket.h"

class AddonHandler
{
    /* Construction */
    friend class Skyfire::Singleton<AddonHandler, Skyfire::NullMutex>;
    AddonHandler();

public:
    ~AddonHandler();
    //build addon packet
    bool BuildAddonPacket(WorldPacket* Source, WorldPacket* Target);
};
#define sAddOnHandler Skyfire::Singleton<AddonHandler, Skyfire::NullMutex>::instance()
#endif
