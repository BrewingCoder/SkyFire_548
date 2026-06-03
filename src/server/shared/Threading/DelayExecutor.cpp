/*
* This file is part of Project SkyFire https://www.projectskyfire.org.
* See LICENSE.md file for Copyright information
*/

#include "DelayExecutor.h"
#include "Platform/Singleton.h"

DelayExecutor* DelayExecutor::instance()
{
    return Skyfire::Singleton<DelayExecutor, Skyfire::Mutex>::instance();
}

DelayExecutor::DelayExecutor()
    : activated_(false) { }

DelayExecutor::~DelayExecutor()
{
    deactivate();
}

int DelayExecutor::deactivate()
{
    {
        std::lock_guard<std::mutex> guard(queue_lock_);

        if (!activated_)
            return -1;

        activated_ = false;
    }

    condition_.notify_all();

    for (std::thread& thread : threads_)
        if (thread.joinable())
            thread.join();

    threads_.clear();
    pre_svc_hook_.reset();
    post_svc_hook_.reset();

    return 0;
}

int DelayExecutor::svc()
{
    if (pre_svc_hook_)
        pre_svc_hook_->call();

    for (;;)
    {
        std::unique_ptr<DelayTask> rq;

        {
            std::unique_lock<std::mutex> lock(queue_lock_);
            condition_.wait(lock, [this] { return !queue_.empty() || !activated_; });

            if (queue_.empty())
                break;

            rq = std::move(queue_.front());
            queue_.pop();
        }

        rq->call();
    }

    if (post_svc_hook_)
        post_svc_hook_->call();

    return 0;
}

int DelayExecutor::start(int num_threads, std::unique_ptr<DelayTask> pre_svc_hook, std::unique_ptr<DelayTask> post_svc_hook)
{
    if (activated())
        return -1;

    if (num_threads < 1)
        return -1;

    pre_svc_hook_ = std::move(pre_svc_hook);
    post_svc_hook_ = std::move(post_svc_hook);

    activated(true);

    try
    {
        for (int i = 0; i < num_threads; ++i)
            threads_.push_back(std::thread(&DelayExecutor::svc, this));
    }
    catch (...)
    {
        deactivate();
        return -1;
    }

    return 0;
}

int DelayExecutor::execute(std::unique_ptr<DelayTask> new_req)
{
    if (!new_req)
        return -1;

    {
        std::lock_guard<std::mutex> guard(queue_lock_);

        if (!activated_)
            return -1;

        queue_.push(std::move(new_req));
    }

    condition_.notify_one();
    return 0;
}

bool DelayExecutor::activated()
{
    std::lock_guard<std::mutex> guard(queue_lock_);
    return activated_;
}

void DelayExecutor::activated(bool s)
{
    std::lock_guard<std::mutex> guard(queue_lock_);
    activated_ = s;
}
