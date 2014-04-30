#ifndef _TIMECOUNTER_HPP_
#define _TIMECOUNTER_HPP_
#include <sys/time.h>

class TimeCounter
{
public:
    TimeCounter() {reset();}
    ~TimeCounter() {}
    void reset() { gettimeofday(&_start, NULL); }
    long GetTimeMilliS() {return get_interval()/1000;}
    long GetTimeMicroS() {return get_interval();}
    long GetTimeS() {return get_interval()/1000000;}
private:
    long get_interval()
    {
        struct timeval now;
        gettimeofday(&now, NULL);
        return (long)(now.tv_sec - _start.tv_sec)*1000000
            + now.tv_usec - _start.tv_usec;
    }
    struct timeval _start;
};



#endif

