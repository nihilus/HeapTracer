#ifndef __HEAPCHUNK_HPP__
#define __HEAPCHUNK_HPP__

#include <set>
using namespace std;

class HeapChunk {
public:
	unsigned int base;
	unsigned int top;
	unsigned int pid;
	unsigned int allocCaller,freeCaller;
	unsigned int allocTime,freeTime;

	friend bool operator<(const HeapChunk &t, const HeapChunk &other);
	HeapChunk(unsigned int _base=0, unsigned int _top=0, unsigned int _allocTime=0, unsigned int _allocCaller=0, unsigned int _pid = 0, unsigned int _freeTime=0, unsigned int _freeCaller = 0):
		base(_base),
		top(_top),
		allocTime(_allocTime),
		allocCaller(_allocCaller),
		pid(_pid),
	   freeTime(_freeTime),
		freeCaller(_freeCaller) {};
//	HeapChunk(HeapChunk &copy) : base(copy.base), top(copy.top), allocT(copy.allocT), freeT(copy.freeT), pid(copy.pid) {};

    bool contains(unsigned int p) { return (base <= p && top > p); };
    void setBase(unsigned int _base) { base = _base; };
    void setTop(unsigned int _top) { top = _top; };
	void setFreeTime(unsigned int _freeTime) { freeTime = _freeTime; };
	void setFreeCaller(unsigned int _caller) { freeCaller = _caller; };
};

class HeapRegion:public HeapChunk {
public:
    set<HeapChunk> chunks;

	unsigned int minAddr, maxAddr, minTime, maxTime;

	friend bool operator<(const HeapRegion &t, const HeapRegion &other);

	HeapRegion(unsigned int _base=0, unsigned int _top=0, unsigned int _allocTime=0, unsigned int _allocCaller=0, unsigned int _pid = 0, unsigned int _freeTime=0, unsigned int _freeCaller = 0):
        HeapChunk(_base, _top, _allocTime, _allocCaller, _pid, _freeTime, _freeCaller) {};

    void alloc(unsigned int base, unsigned int size, unsigned int time=0, unsigned int pid=0, unsigned int caller=0);
    void free(unsigned int base, unsigned int time=0, unsigned int pid=0, unsigned int caller=0);
    void realloc(unsigned int base, unsigned int newBase, unsigned int size, unsigned int time=0, unsigned int pid=0, unsigned int caller=0);
    void setLimits();

    unsigned int size() {return chunks.size();};
    unsigned int getDeltaTime() { return maxTime-minTime;};
    unsigned int getDeltaAddr() { return maxAddr-minAddr;};
};

class Heap {
    int autoRegions;
public:
    set <HeapRegion> regions;
    HeapRegion loose;

    Heap(int _autoRegions = 0): autoRegions(_autoRegions) {};

    void setAutoRegions(int _autoRegions) { autoRegions = _autoRegions;};
    void _mmap(unsigned int base, unsigned int size, unsigned int time=0, unsigned int pid=0, unsigned int caller=0);
    void mmap(unsigned int base, unsigned int size, unsigned int time=0, unsigned int pid=0, unsigned int caller=0);
    void munmap(unsigned int base, unsigned int time=0, unsigned int pid=0, unsigned int caller=0);
    // void mprotect();
    // void brk();
    void alloc(unsigned int base, unsigned int size, unsigned int time=0, unsigned int pid=0, unsigned int caller=0, unsigned int dontRecurse = 0);
    void free(unsigned int base, unsigned int time=0, unsigned int pid=0, unsigned int caller=0);
    void realloc(unsigned int base, unsigned int newBase, unsigned int size, unsigned int time=0, unsigned int pid=0, unsigned int caller=0);
    unsigned int size() {return regions.size();};
};

void startDrawing();
extern int verbose, quiet, numberOfRegion;

extern Heap heap;

#define verprintf(level, x) if (verbose >= (level)) printf x;
#define qprintf(x) if (!quiet) printf x;

#endif

