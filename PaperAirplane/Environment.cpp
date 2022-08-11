#include "Environment.h"

int64_t GetSysTickCount64()
{
    LARGE_INTEGER freq = { 0 };
    LARGE_INTEGER tick;
    if (!freq.QuadPart)
    {
        QueryPerformanceFrequency(&freq);
    }
    QueryPerformanceCounter(&tick);
    int64_t seconds = tick.QuadPart / freq.QuadPart;
    int64_t leftpart = tick.QuadPart - (freq.QuadPart * seconds);
    int64_t millseconds = leftpart * 1000 / freq.QuadPart;
    int64_t retval = seconds * 1000 + millseconds;
    return retval;
}

int GetProcessorCoreCount()
{
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    return si.dwNumberOfProcessors;
}
