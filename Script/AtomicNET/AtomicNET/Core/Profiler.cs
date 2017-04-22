using System;
using System.Runtime.CompilerServices;

namespace AtomicEngine
{
    /// Copied from easy_profiler, might need manual update if enum changes upstream.
    [Flags]
    public enum EasyBlockStatus
    {
        OFF = 0,
        ON = 1,
        FORCE_ON = ON | 2,
        OFF_RECURSIVE = 4,
        ON_WITHOUT_CHILDREN = ON | OFF_RECURSIVE,
        FORCE_ON_WITHOUT_CHILDREN = FORCE_ON | OFF_RECURSIVE
    }

    public partial class Profiler : AObject
    {
        public static void Block(string name, Action block, uint color = 0xffffecb3,
                                 EasyBlockStatus status = EasyBlockStatus.ON,
                                 [CallerFilePath] string file = "",
                                 [CallerLineNumber] int line = 0)
        {
#if ATOMIC_PROFILING
            var profiler = AtomicNET.GetSubsystem<Profiler>();
            profiler?.BeginBlock(name, file, line, color, (byte)status);
#endif
            block();
#if ATOMIC_PROFILING
            profiler?.EndBlock();
#endif
        }
    }
}
