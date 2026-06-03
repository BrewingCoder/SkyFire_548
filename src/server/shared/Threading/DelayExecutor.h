/*
* This file is part of Project SkyFire https://www.projectskyfire.org.
* See LICENSE.md file for Copyright information
*/

#ifndef _M_DELAY_EXECUTOR_H
#define _M_DELAY_EXECUTOR_H

#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

class DelayTask
{
public:
    virtual ~DelayTask() { }
    virtual int call() = 0;
};

class DelayExecutor
{
public:
    DelayExecutor();
    virtual ~DelayExecutor();
    static DelayExecutor* instance();
    int execute(std::unique_ptr<DelayTask> new_req);
    int start(int num_threads = 1, std::unique_ptr<DelayTask> pre_svc_hook = std::unique_ptr<DelayTask>(), std::unique_ptr<DelayTask> post_svc_hook = std::unique_ptr<DelayTask>());
    int deactivate();
    bool activated();
    int svc();

private:
    std::queue<std::unique_ptr<DelayTask> > queue_;
    std::unique_ptr<DelayTask> pre_svc_hook_;
    std::unique_ptr<DelayTask> post_svc_hook_;
    std::vector<std::thread> threads_;
    std::mutex queue_lock_;
    std::condition_variable condition_;
    bool activated_;

    void activated(bool s);
};

#endif // _M_DELAY_EXECUTOR_H
