/*
* This file is part of Project SkyFire https://www.projectskyfire.org.
* See LICENSE.md file for Copyright information
*/

#include "DatabaseEnv.h"
#include "DatabaseWorker.h"
#include "MySQLConnection.h"
#include "MySQLThreading.h"
#include "SQLOperation.h"

DatabaseWorker::DatabaseWorker(Skyfire::DatabaseQueue* new_queue, MySQLConnection* con) :
    m_queue(new_queue),
    m_conn(con)
{
    m_thread = std::thread(&DatabaseWorker::svc, this);
}

DatabaseWorker::~DatabaseWorker()
{
    wait();
}

int DatabaseWorker::svc()
{
    if (!m_queue)
        return -1;

    SQLOperation* request = NULL;
    while (1)
    {
        request = m_queue->dequeue();
        if (!request)
            break;

        request->SetConnection(m_conn);
        request->call();

        delete request;
    }

    return 0;
}

int DatabaseWorker::wait()
{
    if (!m_thread.joinable())
        return 0;

    m_thread.join();
    return 0;
}
