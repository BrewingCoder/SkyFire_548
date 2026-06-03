/*
* This file is part of Project SkyFire https://www.projectskyfire.org.
* See LICENSE.md file for Copyright information
*/

#ifndef _DATABASEQUEUE_H
#define _DATABASEQUEUE_H

#include <condition_variable>
#include <mutex>
#include <queue>

class SQLOperation;

namespace Skyfire
{
    class DatabaseQueue
    {
    public:
        void enqueue(SQLOperation* operation)
        {
            {
                std::lock_guard<std::mutex> lock(_mutex);
                if (_closed)
                    return;

                _queue.push(operation);
            }

            _condition.notify_one();
        }

        SQLOperation* dequeue()
        {
            std::unique_lock<std::mutex> lock(_mutex);
            _condition.wait(lock, [this] { return _closed || !_queue.empty(); });

            if (_queue.empty())
                return nullptr;

            SQLOperation* operation = _queue.front();
            _queue.pop();
            return operation;
        }

        void close()
        {
            {
                std::lock_guard<std::mutex> lock(_mutex);
                _closed = true;
            }

            _condition.notify_all();
        }

    private:
        std::mutex _mutex;
        std::condition_variable _condition;
        std::queue<SQLOperation*> _queue;
        bool _closed = false;
    };
}

#endif
