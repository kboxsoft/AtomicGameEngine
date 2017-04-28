//
// Copyright (c) 2017 the Atomic project.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//

#pragma once

#include "../Container/Str.h"
#include "../Core/Thread.h"
#include "../Core/Timer.h"

#include <easy/profiler.h>

namespace Atomic
{

static const int DEFAULT_PROFILER_PORT = 28077;

/// Hierarchical performance profiler subsystem.
class ATOMIC_API Profiler : public Object
{
    ATOMIC_OBJECT(Profiler, Object);

public:
    /// Construct.
    Profiler(Context* context);
    /// Destruct.
    virtual ~Profiler();

    /// Enables or disables profiler.
    void SetEnabled(bool enabled);
    /// Returns true if profiler is enabled, false otherwise.
    bool GetEnabled() const;
    /// Enables or disables event profiling.
    void SetEventProfilingEnabled(bool enabled);
    /// Returns true if event profiling is enabled, false otherwise.
    bool GetEventProfilingEnabled() const;
    /// Starts listening for incoming profiler tool connections.
    void StartListen(unsigned short port=DEFAULT_PROFILER_PORT);
    /// Stops listening for incoming profiler tool connections.
    void StopListen();
    /// Returns true if profiler is currently listening for incoming connections.
    bool GetListening() const;
    /// Enables or disables event tracing. This is windows-specific, does nothing on other OS.
    void SetEventTracingEnabled(bool enable);
    /// Returns true if event tracing is enabled, false otherwise.
    bool GetEventTracingEnabled();
    /// Enables or disables low priority event tracing. This is windows-specific, does nothing on other OS.
    void SetLowPriorityEventTracing(bool isLowPriority);
    /// Returns true if low priority event tracing is enabled, false otherwise.
    bool GetLowPriorityEventTracing();
    /// Save profiler data to a file.
    void SaveProfilerData(const String& filePath);
    /// Begin non-scoped profiled block. Block has to be terminated with call to EndBlock(). This is slow and is for
    /// integration with scripting lnaguages. Use ATOMIC_PROFILE* macros when writing c++ code instead.
    void BeginBlock(const char* name, const char* file, int line,
                           unsigned int color = profiler::colors::Default,
                           unsigned char status=profiler::ON);
    /// Begin non-scoped profiled block. Block has to be terminated with call to EndBlock(). This is slow and is for
    /// integration with scripting lnaguages. Use ATOMIC_PROFILE* macros when writing c++ code instead.
    void BeginBlock(const String& name, const String& file, int line,
                           unsigned int color = profiler::colors::Default,
                           unsigned char status=profiler::ON);
    /// End block started with BeginBlock().
    void EndBlock();

    /// Get global instance
    static Profiler* GetInstance() { return instance_; }

private:
    bool enableEventProfiling_ = true;
    HashMap<unsigned, profiler::BaseBlockDescriptor*> blockDescriptorCache_;

    static WeakPtr<Profiler> instance_;
};

#ifdef ATOMIC_PROFILING
#   define ATOMIC_PROFILE(name, ...) EASY_BLOCK(#name, __VA_ARGS__)
#   define ATOMIC_PROFILE_SCOPED(name, ...) EASY_BLOCK(name, __VA_ARGS__)
#   define ATOMIC_PROFILE_NONSCOPED(name, ...) EASY_NONSCOPED_BLOCK(name, __VA_ARGS__)
#   define ATOMIC_PROFILE_END(...) EASY_END_BLOCK
#   define ATOMIC_PROFILE_THREAD(name) EASY_THREAD(name)
#else
#   define ATOMIC_PROFILE(name, ...)
#   define ATOMIC_PROFILE_NONSCOPED(name, ...)
#   define ATOMIC_PROFILE_SCOPED(name, ...)
#   define ATOMIC_PROFILE_END(...)
#   define ATOMIC_PROFILE_THREAD(name)
#endif

}
