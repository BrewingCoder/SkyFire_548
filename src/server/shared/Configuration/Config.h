/*
* This file is part of Project SkyFire https://www.projectskyfire.org.
* See LICENSE.md file for Copyright information
*/

#ifndef CONFIG_H
#define CONFIG_H

#include <AutoPtr.h>
#include "Platform/Singleton.h"
#include <ace/Configuration_Import_Export.h>
#include <ace/Null_Mutex.h>
#include <list>
#include <mutex>
#include <string>

typedef Skyfire::AutoPtr<ACE_Configuration_Heap, ACE_Null_Mutex> Config;

class ConfigMgr
{
    friend class Skyfire::Singleton<ConfigMgr, Skyfire::NullMutex>;
    friend class ConfigLoader;

    ConfigMgr() { }
    ~ConfigMgr() { }

public:
    /// Method used only for loading main configuration files (authserver.conf and worldserver.conf)
    bool LoadInitial(char const* file);

    /**
     * This method loads additional configuration files
     * It is recommended to use this method in WorldScript::OnConfigLoad hooks
     *
     * @return true if loading was successful
     */
    bool LoadMore(char const* file);

    bool Reload();

    std::string GetStringDefault(const char* name, const std::string& def);
    bool GetBoolDefault(const char* name, bool def);
    int GetIntDefault(const char* name, int def);
    float GetFloatDefault(const char* name, float def);

    std::string const& GetFilename();
    std::list<std::string> GetKeysByString(std::string const& name);

private:
    bool GetValueHelper(const char* name, ACE_TString& result);
    bool LoadData(char const* file);

    typedef std::mutex LockType;
    typedef std::lock_guard<std::mutex> GuardType;

    std::string _filename;
    Config _config;
    LockType _configLock;

    ConfigMgr(ConfigMgr const&);
    ConfigMgr& operator=(ConfigMgr const&);
};

#define sConfigMgr Skyfire::Singleton<ConfigMgr, Skyfire::NullMutex>::instance()

#endif
