// If you need to get Glut for windows get it from http://www.xmission.com/~nate/glut.html

#ifndef __DRAW_HPP__
#define __DRAW_HPP__

#include <vector>

#include "HeapChunk.hpp"
#include "Heap.hpp"

#define LIMITS  (limits.back())

class CScreenLimits {
public:
	unsigned int startTime, endTime, lowAddr, highAddr;

	CScreenLimits(unsigned int startTime, unsigned int endTime, unsigned int lowAddr, unsigned int highAddr) :
		startTime(startTime),
		endTime(endTime),
		lowAddr(lowAddr),
		highAddr(highAddr) {};
	
	void glOrtho() const {
        ::glOrtho(startTime, endTime, highAddr, lowAddr, 1.0, -1.0);
    };

	unsigned int deltaA() { return highAddr-lowAddr; };
	unsigned int deltaT() { return endTime-startTime; };
};

std::vector <CScreenLimits *> limits;

class HeapChunkDraw: public HeapChunk {
public:
    void glSetColor(int creation_deletion) {
		float time = creation_deletion ? getFreeTime() : allocTime;
		float t0 = (time-LIMITS->startTime) / LIMITS->deltaT();
		float r,g,b;

		t0 += t0;
		if (t0 < 1.0) {
			r = 0.25 + 0.75*t0;
			g = 0.75 + 0.25*t0;
			b = 0.25 - 0.25*t0;
		} else {
			t0 -= 1.0;
			r = 1.0;
			g = 1.0 - 0.75*t0;
			b = 0.0;
		}

		::glColor3f(r,g,b);
    };

    void glDrawQUAD() const {
        glVertex2i(allocTime, top);
        glVertex2i(getFreeTime(), top);
        glVertex2i(getFreeTime(), base);
        glVertex2i(allocTime, base);
    };

    void glDrawLINE() const {
        glVertex2i(allocTime, top);
        glVertex2i(getFreeTime(), base);
    }

};

class HeapDraw: public Heap {
public:
    float getDeltaTime() const {
        return (freeTime?freeTime:gettime())-allocTime;
    }
};

void invalidateDisplay();
void restartDisplay();

#endif
