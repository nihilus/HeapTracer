#ifndef __HEAPCHUNK_HPP__
#define __HEAPCHUNK_HPP__

extern unsigned int __t;

class HeapChunk {
public:
    unsigned int pid;
    unsigned int base, top;
    unsigned int allocTime, freeTime;
    unsigned int allocCaller, freeCaller;

    friend bool operator<(const HeapChunk &a, const HeapChunk &b);
    HeapChunk(unsigned int pid, unsigned int base, unsigned int top, unsigned int allocTime):
        pid(pid), base(base), top(top), allocTime(allocTime), freeTime(0), allocCaller(0), freeCaller(0) {};

    bool contains(unsigned int address) const { return base <= address && top > address; };
    unsigned int gettime() const { return __t++; };
	unsigned int getFreeTime() const { return freeTime?freeTime:__t; }
};

#endif
