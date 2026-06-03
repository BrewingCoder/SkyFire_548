/*
* This file is part of Project SkyFire https://www.projectskyfire.org.
* See LICENSE.md file for Copyright information
*/

#ifndef SKYFIRE_WAYPOINTMANAGER_H
#define SKYFIRE_WAYPOINTMANAGER_H

#include "Platform/Singleton.h"
#include <vector>

struct WaypointData
{
    uint32 id;
    float x, y, z, orientation;
    uint32 delay;
    uint32 event_id;
    bool run;
    uint8 event_chance;
};

typedef std::vector<WaypointData*> WaypointPath;
typedef UNORDERED_MAP<uint32, WaypointPath> WaypointPathContainer;

class WaypointMgr
{
    friend class Skyfire::Singleton<WaypointMgr, Skyfire::NullMutex>;

public:
    // Attempts to reload a single path from database
    void ReloadPath(uint32 id);

    // Loads all paths from database, should only run on startup
    void Load();

    // Returns the path from a given id
    WaypointPath const* GetPath(uint32 id) const
    {
        WaypointPathContainer::const_iterator itr = _waypointStore.find(id);
        if (itr != _waypointStore.end())
            return &itr->second;

        return NULL;
    }

private:
    // Only allow instantiation from the singleton wrapper.
    WaypointMgr();
    ~WaypointMgr();

    WaypointPathContainer _waypointStore;
};

#define sWaypointMgr Skyfire::Singleton<WaypointMgr, Skyfire::NullMutex>::instance()

#endif
