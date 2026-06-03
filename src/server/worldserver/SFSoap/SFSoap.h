/*
* This file is part of Project SkyFire https://www.projectskyfire.org.
* See LICENSE.md file for Copyright information
*/

#ifndef _SFSoap_H
#define _SFSoap_H

#include "Define.h"

#include <condition_variable>
#include <mutex>

class SFSoapRunnable
{
public:
    SFSoapRunnable() : _port(0) { }

    void Run();

    void SetListenArguments(const std::string& host, uint16 port)
    {
        _host = host;
        _port = port;
    }

private:
    void process_message(struct soap* soap);

    std::string _host;
    uint16 _port;
};

class SOAPCommand
{
public:
    SOAPCommand() :
        m_completed(false), m_success(false)
    {
    }

    ~SOAPCommand()
    {
    }

    void appendToPrintBuffer(const char* msg)
    {
        m_printBuffer += msg;
    }

    void wait()
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_condition.wait(lock, [this] { return m_completed; });
    }

    void complete(bool success)
    {
        {
            std::lock_guard<std::mutex> guard(m_mutex);
            m_success = success;
            m_completed = true;
        }

        m_condition.notify_one();
    }

    void setCommandSuccess(bool val)
    {
        m_success = val;
    }

    bool hasCommandSucceeded() const
    {
        return m_success;
    }

    static void print(void* callbackArg, const char* msg)
    {
        ((SOAPCommand*)callbackArg)->appendToPrintBuffer(msg);
    }

    static void commandFinished(void* callbackArg, bool success);

    bool m_success;
    std::string m_printBuffer;

private:
    std::mutex m_mutex;
    std::condition_variable m_condition;
    bool m_completed;
};

#endif
