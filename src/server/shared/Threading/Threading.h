/*
* This file is part of Project SkyFire https://www.projectskyfire.org.
* See LICENSE.md file for Copyright information
*/

#ifndef THREADING_H
#define THREADING_H

#include <atomic>
#include <assert.h>
#include <cstdint>
#include <thread>

namespace Skyfire
{

    class Runnable
    {
    public:
        Runnable() : m_refs(0) { }
        virtual ~Runnable() { }
        virtual void run() = 0;

        void incReference() { ++m_refs; }
        void decReference()
        {
            if (!--m_refs)
                delete this;
        }
    private:
        std::atomic<long> m_refs;
    };

    enum Priority
    {
        Idle,
        Lowest,
        Low,
        Normal,
        High,
        Highest,
        Realtime
    };

#define MAXPRIORITYNUM (Realtime + 1)

    class ThreadPriority
    {
    public:
        ThreadPriority();
        int getPriority(Priority p) const;

    private:
        int m_priority[MAXPRIORITYNUM];
    };

    class Thread
    {
    public:
        Thread();
        explicit Thread(Runnable* instance);
        ~Thread();

        bool start();
        bool wait();
        void destroy();

        void suspend();
        void resume();

        void setPriority(Priority type);

        static void Sleep(unsigned long msecs);
        static uint64_t currentId();
        static std::thread::native_handle_type currentHandle();
        static Thread* current();

    private:
        Thread(const Thread&);
        Thread& operator=(const Thread&);

        static void ThreadTask(Runnable* param);

        uint64_t m_iThreadId;
        std::thread m_thread;
        Runnable* m_task;
        bool m_started;
    };

}

#endif
