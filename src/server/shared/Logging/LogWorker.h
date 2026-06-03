/*
* This file is part of Project SkyFire https://www.projectskyfire.org.
* See LICENSE.md file for Copyright information
*/

#ifndef LOGWORKER_H
#define LOGWORKER_H

#include "LogOperation.h"

#include <condition_variable>
#include <mutex>
#include <queue>
#include <thread>

class LogWorker
{
public:
    LogWorker();
    ~LogWorker();

    enum
    {
        HIGH_WATERMARK = 8 * 1024 * 1024,
        LOW_WATERMARK = 8 * 1024 * 1024
    };

    int enqueue(LogOperation* op);

private:
    int svc();

    std::queue<LogOperation*> m_queue;
    std::mutex m_queueLock;
    std::condition_variable m_condition;
    std::thread m_thread;
    bool m_active;
};

#endif
