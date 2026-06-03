/*
* This file is part of Project SkyFire https://www.projectskyfire.org.
* See LICENSE.md file for Copyright information
*/

#include "Errors.h"
#include "Threading.h"
#include <chrono>
#include <functional>

using namespace Skyfire;

ThreadPriority::ThreadPriority()
{
    for (int i = Idle; i < MAXPRIORITYNUM; ++i)
        m_priority[i] = i;
}

int ThreadPriority::getPriority(Priority p) const
{
    if (p < Idle)
        p = Idle;

    if (p > Realtime)
        p = Realtime;

    return m_priority[p];
}

Thread::Thread() : m_iThreadId(0), m_task(0), m_started(false) { }

Thread::Thread(Runnable* instance) : m_iThreadId(0), m_task(instance), m_started(false)
{
    // register reference to m_task to prevent it deletion until destructor
    if (m_task)
        m_task->incReference();

    bool _start = start();
    ASSERT(_start);
}

Thread::~Thread()
{
    if (m_thread.joinable())
        m_thread.detach();

    // deleted runnable object (if no other references)
    if (m_task)
        m_task->decReference();
}

bool Thread::start()
{
    if (m_task == 0 || m_started)
        return false;

    // incRef before spawning the thread, otherwise Thread::ThreadTask() might call decRef and delete m_task
    m_task->incReference();

    try
    {
        m_thread = std::thread(&Thread::ThreadTask, m_task);
        m_iThreadId = std::hash<std::thread::id>()(m_thread.get_id());
        m_started = true;
    }
    catch (...)
    {
        m_task->decReference();
        return false;
    }

    return true;
}

bool Thread::wait()
{
    if (!m_thread.joinable())
        return false;

    m_thread.join();

    m_iThreadId = 0;
    m_started = false;

    return true;
}

void Thread::destroy()
{
    if (m_thread.joinable())
        m_thread.detach();

    m_iThreadId = 0;
    m_started = false;
}

void Thread::suspend()
{
}

void Thread::resume()
{
}

void Thread::ThreadTask(Runnable* param)
{
    Runnable* _task = param;
    _task->run();

    // task execution complete, free reference added at start()
    _task->decReference();
}

uint64_t Thread::currentId()
{
    return std::hash<std::thread::id>()(std::this_thread::get_id());
}

std::thread::native_handle_type Thread::currentHandle()
{
    return std::thread::native_handle_type();
}

Thread* Thread::current()
{
    static thread_local Thread thread;
    if (!thread.m_started)
    {
        thread.m_iThreadId = Thread::currentId();
        thread.m_started = true;
    }

    return &thread;
}

void Thread::setPriority(Priority type)
{
    (void)type;
}

void Thread::Sleep(unsigned long msecs)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(msecs));
}
