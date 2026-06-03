/*
* This file is part of Project SkyFire https://www.projectskyfire.org.
* See LICENSE.md file for Copyright information
*/

#ifndef _SKYFIRE_AUTO_PTR_H
#define _SKYFIRE_AUTO_PTR_H

#include <memory>

namespace Skyfire
{

    template <class Pointer, class Lock>
    class AutoPtr : public std::shared_ptr<Pointer>
    {
        typedef std::shared_ptr<Pointer> Base;

    public:
        AutoPtr() : Base() { }

        AutoPtr(Pointer* x) : Base(x) { }

        operator bool() const
        {
            return Base::get() != NULL;
        }

        bool operator !() const
        {
            return Base::get() == NULL;
        }

        bool null() const
        {
            return Base::get() == NULL;
        }
    };

} // namespace Skyfire

#endif
