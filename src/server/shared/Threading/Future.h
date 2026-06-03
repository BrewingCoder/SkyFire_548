/*
* This file is part of Project SkyFire https://www.projectskyfire.org.
* See LICENSE.md file for Copyright information
*/

#ifndef SKYFIRE_FUTURE_H
#define SKYFIRE_FUTURE_H

#include <chrono>
#include <future>
#include <memory>
#include <mutex>

namespace Skyfire
{
    template <class T>
    class Future
    {
    public:
        Future() : _state(std::make_shared<State>()) { }

        void set(T value)
        {
            std::lock_guard<std::mutex> lock(_state->mutex);
            if (_state->completed)
                return;

            _state->promise.set_value(value);
            _state->completed = true;
        }

        bool ready() const
        {
            return _state->future.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
        }

        void get(T& value) const
        {
            value = _state->future.get();
        }

        void cancel()
        {
            _state = std::make_shared<State>();
        }

    private:
        struct State
        {
            State() : future(promise.get_future().share()), completed(false) { }

            std::promise<T> promise;
            std::shared_future<T> future;
            bool completed;
            std::mutex mutex;
        };

        std::shared_ptr<State> _state;
    };
}

#endif
