#ifndef __HEAP_HPP__
#define __HEAP_HPP__

#include <ida.hpp>
#include "HeapChunk.hpp"

#include <windows.h>
//#include <vector>
#include <map>

using namespace std;

// inline int log(const char *format,...);

class HeapEvent {
public:
    char *comment;
    unsigned int time;
    HeapEvent(char *comment, unsigned int time): time(time), comment(comment) {};
};

// Thread safe vector
// one thread can add elements
// the other iterate

template<class _element> 
class ts_vector {
private:
	static const unsigned int INITIAL_SIZE = 512;
	_element *elements;
	unsigned int maxSpace;
	unsigned int count;
	CRITICAL_SECTION _lock;

	void resize(unsigned int newSize) {
		_element *newElements = new _element[newSize];
		_element *oldElements = elements;

		memcpy(newElements, elements, ((maxSpace<newSize)?maxSpace:newSize) * sizeof _element);

		lock();
			elements = newElements;
			maxSpace = newSize;
			delete[] oldElements;
		unlock();
	}

public:
	ts_vector() {
		InitializeCriticalSection(&_lock);
		elements = NULL;
		clear();
	}

	void lock() {
		EnterCriticalSection(&_lock);
	}

	void unlock() {
		LeaveCriticalSection(&_lock);
	}

	unsigned int size() { return count; }

	void push_back(_element toAdd) {
		if (count == maxSpace)
			resize(maxSpace?(maxSpace + maxSpace):INITIAL_SIZE);
		elements[count] = toAdd;
		++count;
	}

	void clear() {
		maxSpace = 0;
		count = 0;

		lock();
			delete[] elements;
			elements = NULL;
		unlock();
	}

	class iterator {
		private:
			ts_vector<_element> *_vector;
			unsigned int current;
			bool locking;
		public:

			iterator(unsigned int start = 0, ts_vector<_element> *vect = NULL, bool doLocking = true) {
				current = start;
				_vector = vect;
				locking = doLocking;
				if (locking) _vector->lock();
			}

			~iterator() {
				if (locking) _vector->unlock();
			}

			iterator& operator=(iterator &other) {
				current = other.current;
				_vector = other._vector;
				return *this;
			}

			bool operator==(iterator &other)	{ return (_vector == other._vector) && (current == other.current);	}
			bool operator!=(iterator &other)	{ return ! (*this == other);	}
			iterator& operator++()				{ current++; return *this;	}
			// iterator& operator++()				{ ++current; return *this;	}
			_element& operator*()				{ return _vector->elements[current]; };
	};

	iterator begin() {
		return iterator(0, this);
	}

	iterator end() {
		return iterator(count, this, false);
	}
};


class Heap:public HeapChunk {
public:
    map<unsigned int, HeapChunk *> openChunks;
    ts_vector<HeapChunk *> chunks;
    ts_vector<HeapEvent *> events;

    Heap():HeapChunk(0,0xffffffff,0,gettime()) {};

    HeapChunk *malloc(uint pid, unsigned int base, unsigned int size, unsigned int time = 0);
    HeapChunk *free(unsigned int pid, unsigned int base, unsigned int time = 0);
    HeapChunk *realloc(unsigned int pid, unsigned int old_base, unsigned int new_base, unsigned int new_size, unsigned int time = 0);

    void addEvent(char *comment, unsigned int time = 0);
	void clear();
};

extern map<unsigned int, Heap *>heaps;
Heap *getHeap(unsigned int handle);
void clearHeap(unsigned int handle);
void clearHeaps();

#endif
