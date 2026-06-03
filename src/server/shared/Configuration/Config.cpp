/*
* This file is part of Project SkyFire https://www.projectskyfire.org.
* See LICENSE.md file for Copyright information
*/

#include "Config.h"
#include "Errors.h"

#include <algorithm>
#include <cctype>
#include <fstream>

namespace
{
    std::string Trim(std::string value)
    {
        std::string::iterator first = std::find_if(value.begin(), value.end(), [](unsigned char c) { return !std::isspace(c); });
        std::string::reverse_iterator last = std::find_if(value.rbegin(), value.rend(), [](unsigned char c) { return !std::isspace(c); });

        if (first == value.end())
            return "";

        return std::string(first, last.base());
    }

    std::string StripComment(std::string const& line)
    {
        bool quoted = false;
        char quote = '\0';

        for (std::string::size_type i = 0; i < line.size(); ++i)
        {
            char c = line[i];

            if ((c == '"' || c == '\'') && (i == 0 || line[i - 1] != '\\'))
            {
                if (!quoted)
                {
                    quoted = true;
                    quote = c;
                }
                else if (quote == c)
                    quoted = false;
            }
            else if (!quoted && (c == '#' || c == ';'))
                return line.substr(0, i);
        }

        return line;
    }

    std::string StripQuotes(std::string value)
    {
        if (value.size() >= 2)
        {
            char first = value.front();
            char last = value.back();

            if ((first == '"' && last == '"') || (first == '\'' && last == '\''))
                return value.substr(1, value.size() - 2);
        }

        return value;
    }
}

// Defined here as it must not be exposed to end-users.
bool ConfigMgr::GetValueHelper(const char* name, std::string& result)
{
    GuardType guard(_configLock);

    if (!_configLoaded)
        return false;

    for (Config::const_iterator section = _config.begin(); section != _config.end(); ++section)
    {
        ConfigSection::const_iterator value = section->second.find(name);
        if (value != section->second.end())
        {
            result = value->second;
            return true;
        }
    }

    return false;
}

bool ConfigMgr::LoadInitial(char const* file)
{
    ASSERT(file);

    GuardType guard(_configLock);

    _filename = file;
    _config.clear();
    _configLoaded = false;

    if (LoadData(_filename.c_str()))
    {
        _configLoaded = true;
        return true;
    }

    _config.clear();
    return false;
}

bool ConfigMgr::LoadMore(char const* file)
{
    ASSERT(file);
    ASSERT(_configLoaded);

    GuardType guard(_configLock);

    return LoadData(file);
}

bool ConfigMgr::Reload()
{
    return LoadInitial(_filename.c_str());
}

bool ConfigMgr::LoadData(char const* file)
{
    std::ifstream input(file);
    if (!input.is_open())
        return false;

    std::string section;
    std::string line;

    while (std::getline(input, line))
    {
        line = Trim(StripComment(line));

        if (line.empty())
            continue;

        if (line.front() == '[' && line.back() == ']')
        {
            section = Trim(line.substr(1, line.size() - 2));
            _config[section];
            continue;
        }

        std::string::size_type separator = line.find('=');
        if (separator == std::string::npos)
            continue;

        std::string key = Trim(line.substr(0, separator));
        std::string value = StripQuotes(Trim(line.substr(separator + 1)));

        if (!key.empty())
            _config[section][key] = value;
    }

    return true;
}

std::string ConfigMgr::GetStringDefault(const char* name, const std::string& def)
{
    std::string val;
    return GetValueHelper(name, val) ? val : def;
}

bool ConfigMgr::GetBoolDefault(const char* name, bool def)
{
    std::string val;

    if (!GetValueHelper(name, val))
        return def;

    return (val == "true" || val == "TRUE" || val == "yes" || val == "YES" ||
        val == "1");
}

int ConfigMgr::GetIntDefault(const char* name, int def)
{
    std::string val;
    return GetValueHelper(name, val) ? atoi(val.c_str()) : def;
}

float ConfigMgr::GetFloatDefault(const char* name, float def)
{
    std::string val;
    return GetValueHelper(name, val) ? (float)atof(val.c_str()) : def;
}

std::string const& ConfigMgr::GetFilename()
{
    GuardType guard(_configLock);
    return _filename;
}

std::list<std::string> ConfigMgr::GetKeysByString(std::string const& name)
{
    GuardType guard(_configLock);

    std::list<std::string> keys;
    if (!_configLoaded)
        return keys;

    for (Config::const_iterator section = _config.begin(); section != _config.end(); ++section)
    {
        for (ConfigSection::const_iterator value = section->second.begin(); value != section->second.end(); ++value)
        {
            size_t pos = value->first.find(name);

            if (pos != std::string::npos)
                keys.push_back(value->first);
        }
    }

    return keys;
}
