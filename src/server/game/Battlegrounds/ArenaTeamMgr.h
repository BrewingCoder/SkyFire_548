/*
* This file is part of Project SkyFire https://www.projectskyfire.org.
* See LICENSE.md file for Copyright information
*/

#ifndef SF_ARENATEAMMGR_H
#define SF_ARENATEAMMGR_H

#include "ArenaTeam.h"
#include "Platform/Singleton.h"

class ArenaTeamMgr
{
    friend class Skyfire::Singleton<ArenaTeamMgr, Skyfire::NullMutex>;
    ArenaTeamMgr() : NextArenaTeamId(1) { }
    ~ArenaTeamMgr();

public:
    typedef UNORDERED_MAP<uint32, ArenaTeam*> ArenaTeamContainer;

    ArenaTeam* GetArenaTeamById(uint32 arenaTeamId) const;

    void AddArenaTeam(ArenaTeam* arenaTeam);
    void RemoveArenaTeam(uint32 Id);

    ArenaTeamContainer::iterator GetArenaTeamMapBegin() { return ArenaTeamStore.begin(); }
    ArenaTeamContainer::iterator GetArenaTeamMapEnd() { return ArenaTeamStore.end(); }

protected:
    uint32 NextArenaTeamId;
    ArenaTeamContainer ArenaTeamStore;
};

#define sArenaTeamMgr Skyfire::Singleton<ArenaTeamMgr, Skyfire::NullMutex>::instance()

#endif
