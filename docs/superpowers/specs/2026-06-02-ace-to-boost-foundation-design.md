# ACE to Boost Foundation Migration Design

## Context

SkyFire currently depends on ACE throughout the server and tools. ACE is used for build discovery, networking and reactor loops, socket acceptors, thread wrappers, task queues, method requests, message blocks, message queues, mutexes, semaphores, singleton access, configuration parsing, timers, sleeps, local time helpers, and some utility functions.

This migration will replace ACE with Boost and standard C++ incrementally. The branch must preserve Windows and Linux CI compatibility throughout the process. Boost 1.91.0 is the required Boost version.

## Goals

- Require Boost 1.91.0 in CMake.
- Introduce SkyFire-owned abstraction names instead of ACE-shaped replacement names.
- Keep ACE available while unmigrated code still depends on it.
- Start with low-risk shared utilities before touching networking, reactors, async queues, or configuration parsing.
- Keep each milestone buildable so regressions are localized.

## Non-Goals

- Do not remove ACE in the first milestone.
- Do not rewrite authserver, worldserver, RA, or SOAP socket/reactor code in the first milestone.
- Do not migrate `ACE_Configuration_Heap` in the first milestone.
- Do not create fake ACE compatibility types such as `ACE_Thread_Mutex` backed by Boost.

## Architecture

Add a SkyFire-owned portability layer under `src/server/shared`. The exact directory may be adjusted during implementation to match local style, but the layer should expose project-owned names such as:

- `Skyfire::SleepFor`
- `Skyfire::GetMSTime`
- `Skyfire::LocalTime`
- `Skyfire::Mutex`
- `Skyfire::RecursiveMutex`
- `Skyfire::NullMutex`
- `Skyfire::Singleton`

The wrappers may use standard C++ where that is cleaner, especially for time and thread sleeps. Boost remains the project dependency for the broader ACE replacement path, including future Boost.Asio networking and Boost threading or synchronization support where standard C++ is not enough.

ACE remains in CMake and linked targets until all ACE call sites are migrated.

## Components

### Build System

Top-level CMake will add Boost 1.91.0 discovery. It should support normal CMake discovery and the `BOOST_ROOT` environment variable. Existing ACE discovery remains in place during migration.

Documentation should list Boost 1.91.0 as a required dependency and clarify that ACE is still required during the transition.

### Time Utilities

Low-risk uses of `ACE_OS::gettimeofday`, `ACE_Time_Value`, and `ACE_OS::sleep` should migrate first through SkyFire time helpers. The implementation should prefer `std::chrono`, `std::this_thread::sleep_for`, and platform-safe local time conversion.

### Threading Primitives

Introduce SkyFire-owned mutex and null mutex abstractions. They should not mimic ACE names. Initial call-site migrations should be conservative and limited to code that does not depend on ACE reactor, ACE task, or ACE message queue behavior.

### Singleton Helper

Introduce a SkyFire-owned singleton helper so future manager migrations can replace `ACE_Singleton<T, Lock>::instance()` with a project API. The first milestone may migrate a small number of low-risk singleton definitions if compile fallout is manageable.

### Deferred Subsystems

Configuration parsing, async task queues, message queues, database workers, logging workers, map updater workers, authserver sockets, worldserver sockets, RA sockets, SOAP threading, and map tools remain separate migration milestones.

## Data Flow

The first milestone does not change gameplay, database, or network packet data flow. It changes dependency flow:

1. CMake discovers Boost 1.91.0.
2. New and migrated code includes SkyFire portability headers.
3. SkyFire portability headers use standard C++ or Boost internals.
4. Unmigrated code continues to include and link ACE.

Future milestones can replace internals without forcing all call sites to know whether ACE, Boost, or the standard library provides the implementation.

## Error Handling

CMake should fail early with a clear message if Boost 1.91.0 is not found. The error should mention `BOOST_ROOT` because Windows users are expected to install Boost manually.

Runtime helper functions should preserve existing behavior where possible. For local time conversion, helpers should return a success/failure signal or use a safe fallback instead of assuming platform calls always succeed.

Migration commits should avoid changing runtime behavior unless the change is explicitly part of the milestone.

## Testing And Verification

Each milestone should include:

- A CMake configure test on Windows with `BOOST_ROOT=C:\local\boost_1_91_0`.
- A CMake configure test on Linux CI or an equivalent local Linux environment.
- A focused compile target where available, starting with shared utilities before full server builds.
- Search checks showing reduced ACE usage in the touched area.

The first implementation plan should include a baseline configure attempt before edits. Known unrelated blockers, such as missing OpenSSL variables, should be recorded separately from Boost migration failures.

## Migration Order

1. Add Boost 1.91.0 CMake discovery and documentation.
2. Add SkyFire portability headers for time, sleep, local time, mutexes, null mutexes, and singleton access.
3. Migrate low-risk shared utility call sites.
4. Verify Windows and Linux-compatible configuration behavior.
5. Plan the next subsystem after the foundation compiles cleanly.

## Open Decisions

The exact header names and directory layout should follow existing `src/server/shared` conventions during implementation. The first plan should choose names that are boring, discoverable, and easy to include from shared, game, authserver, worldserver, and tools code.
