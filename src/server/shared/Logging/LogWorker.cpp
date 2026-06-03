/*
* This file is part of Project SkyFire https://www.projectskyfire.org.
* See LICENSE.md file for Copyright information
*/

#include "LogWorker.h"

LogWorker::LogWorker()
    : m_active(true), m_thread(&LogWorker::svc, this)
{
}

LogWorker::~LogWorker()
{
    {
        std::lock_guard<std::mutex> guard(m_queueLock);
        m_active = false;
    }

    m_condition.notify_all();

    if (m_thread.joinable())
        m_thread.join();
}

int LogWorker::enqueue(LogOperation* op)
{
    if (!op)
        return -1;

    {
        std::lock_guard<std::mutex> guard(m_queueLock);

        if (!m_active)
            return -1;

        m_queue.push(op);
    }

    m_condition.notify_one();
    return 0;
}

int LogWorker::svc()
{
    while (1)
    {
        LogOperation* request = NULL;

        {
            std::unique_lock<std::mutex> lock(m_queueLock);
            m_condition.wait(lock, [this] { return !m_queue.empty() || !m_active; });

            if (m_queue.empty())
                break;

            request = m_queue.front();
            m_queue.pop();
        }

        request->call();
        delete request;
    }

    return 0;
}
