/*
* This file is part of Project SkyFire https://www.projectskyfire.org.
* See LICENSE.md file for Copyright information
*/

#include "Errors.h"

#include <chrono>
#include <cstdlib>
#include <thread>

namespace Skyfire
{
    void Assert(char const* file, int line, char const* function, char const* message)
    {
        fprintf(stderr, "\n%s:%i in %s ASSERTION FAILED:\n  %s\n",
            file, line, function, message);
        *((volatile int*)NULL) = 0;
        exit(1);
    }

    void Fatal(char const* file, int line, char const* function, char const* message)
    {
        fprintf(stderr, "\n%s:%i in %s FATAL ERROR:\n  %s\n",
            file, line, function, message);
        std::this_thread::sleep_for(std::chrono::seconds(10));
        *((volatile int*)NULL) = 0;
        exit(1);
    }

    void Error(char const* file, int line, char const* function, char const* message)
    {
        fprintf(stderr, "\n%s:%i in %s ERROR:\n  %s\n",
            file, line, function, message);
        *((volatile int*)NULL) = 0;
        exit(1);
    }

    void Warning(char const* file, int line, char const* function, char const* message)
    {
        fprintf(stderr, "\n%s:%i in %s WARNING:\n  %s\n",
            file, line, function, message);
    }

} // namespace Skyfire
