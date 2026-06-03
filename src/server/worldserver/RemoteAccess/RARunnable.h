/*
* This file is part of Project SkyFire https://www.projectskyfire.org.
* See LICENSE.md file for Copyright information
*/

/// \addtogroup Skyfired
/// @{
/// \file

#ifndef _SKYFIRE_RARUNNABLE_H_
#define _SKYFIRE_RARUNNABLE_H_

#include "Common.h"

class RARunnable
{
public:
    RARunnable() { }
    virtual ~RARunnable() { }
    void Run();
};

#endif /* _SKYFIRE_RARUNNABLE_H_ */

/// @}
