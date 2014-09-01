#include <ida.hpp>
#include <math.h>
//#include <vector>

#if defined(WINDOWS) || defined(WIN32) || defined(__NT__)
#include <windows.h>
#endif

#include <GL/gl.h>

#ifdef LINUX
#include <GL/glx.h>
#endif

#include "GL/glut.h"

#include "Heap.hpp"
#include "Draw.hpp"

int log(const char *format,...);
int msg(const char *format,...);
extern ea_t cfg_HeapToDraw;
extern bool cfg_ShowEvents;

unsigned int screenWidth, screenHeight;
unsigned int colorTimeScale = 0;
unsigned int drawSubPixel = 1;
unsigned int autoGrow = 1;
float selectionX,selectionY=0.0;
float selectionX1,selectionY1;

GLint heapList;

HeapDraw *region = NULL;

void compile() {
    HeapChunkDraw *hcd;
	// heapList = glGenLists(1);

	// glNewList(heapList, GL_COMPILE);

	// draw chunks
    glBegin(GL_QUADS);
	for (ts_vector<HeapChunk*>::iterator i=region->chunks.begin();i != region->chunks.end(); ++i) {
        hcd = (HeapChunkDraw*)*i;

        hcd->glSetColor(colorTimeScale);
        hcd->glDrawQUAD();
	}
    glEnd();

	// draw lines to see subpixel chunks
    glBegin(GL_LINES);
    if (drawSubPixel) {
        for (ts_vector<HeapChunk*>::iterator i=region->chunks.begin();i != region->chunks.end(); ++i) {
            hcd = (HeapChunkDraw*)*i;

            hcd->glSetColor(colorTimeScale);
            hcd->glDrawLINE();
        }
    }

	// draw 
	if (cfg_ShowEvents) {
		glColor3f(1, 0, 0);
		for (ts_vector<HeapEvent*>::iterator i=region->events.begin();i != region->events.end(); ++i) {
			glVertex2i((*i)->time, LIMITS->lowAddr);
			glVertex2i((*i)->time, LIMITS->highAddr);
		}
	}
    glEnd();
    // glEndList();
}

void printLimits() {
	return;
    log("Limits: %ud-%ud 0x%08x-0x%08x (%u)\n", 
		LIMITS->startTime,
		LIMITS->endTime,
		LIMITS->lowAddr,
		LIMITS->highAddr,
		LIMITS->deltaA());
}

void deleteLastLimit() {
	if (limits.size()) {
		delete limits.back();
		limits.pop_back();
	}
}

void ZoomAll() {
	if (limits.size() == 1)
		deleteLastLimit();
	limits.push_back(new CScreenLimits(region->allocTime, region->getFreeTime(), region->base, region->top));
    printLimits();
}

void ZoomMore() {
	if (region->getFreeTime() >= LIMITS->endTime)
		LIMITS->endTime = region->getFreeTime();
	
	if (region->top >= LIMITS->highAddr)
		LIMITS->highAddr = region->top;
}

void _cdecl draw() {
	glClear(GL_COLOR_BUFFER_BIT);

	if (region) {
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		
		LIMITS->glOrtho();

		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
		compile();
		// glCallList(heapList);
		if (selectionX && selectionY) {
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			glEnable(GL_BLEND);
			glColor4f(0.8, 0.8, 1.0, 0.5);
			glRectf(selectionX, selectionY, selectionX1, selectionY1);
			glDisable(GL_BLEND);
		}
	}
	glFlush();
    glutSwapBuffers();
}

void _cdecl reshape(GLsizei w, GLsizei h) {
	screenWidth = w;
	screenHeight = h;

	glViewport(0,0,w,h);
	// log("%d %d\n",w,h);
}

void screenToReal(int x, int y, float *time, float *addr) {
    // y+=20;
	*time = (float)x*LIMITS->deltaT()/(float)screenWidth+LIMITS->startTime;
	*addr = (float)y*LIMITS->deltaA()/(float)screenHeight+LIMITS->lowAddr;
}

void _cdecl keystroke(unsigned char key, int x, int y) {
	float xx,yy;
	switch (key) {
        case 0x1b:  // Esc
		case 'q':
		case 'Q':
			// glutDestroyWindow(glutGetWindow());
            ExitThread(0);
            break;
	}
	if (!region) return;

	float rl = 0.5*LIMITS->deltaT(), tb = 0.5*LIMITS->deltaA();
	switch (key) {
		case 8:	// backspace
			if (limits.size() == 1)
			  	return;
			deleteLastLimit();
			break;
		case '1':
			while (limits.size())
				deleteLastLimit();
			ZoomAll();
			break;
		case 'e':
			cfg_ShowEvents = !cfg_ShowEvents;
			break;
        case 'f':   // flip color time scale
            colorTimeScale ^= 1;
            break;
		case 'g':	// enable/disable auto region growing
			autoGrow ^= 1;
			break;
		case 'h':	// move left
			limits.push_back(new CScreenLimits(LIMITS->startTime-rl, LIMITS->endTime-rl, LIMITS->lowAddr, LIMITS->highAddr));
			break;
		case 'l':	// move right
			limits.push_back(new CScreenLimits(LIMITS->startTime+rl, LIMITS->endTime+rl, LIMITS->lowAddr, LIMITS->highAddr));
			break;
		case 'k':	// move down
			limits.push_back(new CScreenLimits(LIMITS->startTime, LIMITS->endTime, LIMITS->lowAddr-tb, LIMITS->highAddr-tb));
			break;
		case 'j':	// move up
			limits.push_back(new CScreenLimits(LIMITS->startTime, LIMITS->endTime, LIMITS->lowAddr+tb, LIMITS->highAddr+tb));
			break;
        case 'x':   // draw subpixel
            drawSubPixel ^= 1;
            break;
		case '.':
			screenToReal(x,y,&xx,&yy);
			msg("%08x\n", (int)yy);
			return;
		default:
			screenToReal(x,y,&xx,&yy);
			log("[%c] %8x %d %d %f %08x\n", key, key, x, y, xx, (int)yy);
			return;
	}
    printLimits();
	glutPostRedisplay();
}

float b2_x=0, b2_y=0;

void _cdecl mouseMove(int x, int y) {
	if (!region) return;
    screenToReal(x, y, &selectionX1, &selectionY1);
    glutPostRedisplay();
    if (b2_x && b2_y) {
        float xx,yy;
        screenToReal(x,y,&xx,&yy);
        b2_x = xx;
        b2_y = yy;
    }
}

void _cdecl timer(int _) {
    glutTimerFunc(100,timer,0);
	if (!region) return;

    if (b2_x && b2_y) {
        limits.push_back(new CScreenLimits(
                    LIMITS->startTime * 0.75 + b2_x*0.25,
                    LIMITS->endTime   * 0.75 + b2_x*0.25,
                    LIMITS->lowAddr   * 0.75 + b2_y*0.25,
                    LIMITS->highAddr  * 0.75 + b2_y*0.25));
        printLimits();
        glutPostRedisplay();
    }
}


void _cdecl mouseButton(int button, int state, int x, int y) {
	float xx,yy;
	float startTime, endTime, lowAddr, highAddr;

	if (!region) return;

    screenToReal(x, y, &xx, &yy);
	
	if (button == GLUT_LEFT_BUTTON) {
		if (state == GLUT_DOWN) {
			selectionX1 = selectionX = xx;
			selectionY1 = selectionY = yy;
		} else if (state == GLUT_UP) {
			if (selectionX<xx) {
				startTime = selectionX;
				endTime   = xx;
			} else {

				endTime   = selectionX;
				startTime = xx;
			}
			if (selectionY>yy) {
				lowAddr  = yy;
				highAddr = selectionY;
			} else {
				highAddr = yy;
				lowAddr  = selectionY;
			}
            if (startTime == endTime || lowAddr == highAddr) return;
			limits.push_back(new CScreenLimits(startTime, endTime, lowAddr, highAddr));

            selectionX = selectionY = 0;
			glutPostRedisplay();
            printLimits();
		}
	} else 
	if (button == GLUT_RIGHT_BUTTON)
		if (state == GLUT_DOWN) {
			b2_x = xx;
			b2_y = yy;
		    glutTimerFunc(100,timer,0);
		} else if (state == GLUT_UP) {
            b2_x = b2_y = 0;
		    glutTimerFunc(100,NULL,0);
        }
    else log("%08x:%08x %d %d\n", button, state, x, y);
}

DWORD WINAPI startDrawing(LPVOID _) {
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
//	glutCreateWindow("HeapDraw");
	__glutCreateWindowWithExit("HeapDraw",(void(__cdecl*)(int))&ExitThread);
    glutSetCursor(GLUT_CURSOR_FULL_CROSSHAIR);
	glutDisplayFunc(&draw);
	glutReshapeFunc(&reshape);
	glutKeyboardFunc(&keystroke);
	glutMouseFunc(&mouseButton);
    glutMotionFunc(&mouseMove);

	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

	// compile();
	glutMainLoop();
	return 0;
}

void restartDisplay() {
	region = NULL;
	while (limits.size()) deleteLastLimit();
}

void invalidateDisplay() {
 	if (!region) {
		if (heaps.find(cfg_HeapToDraw) == heaps.end())
			return;

		region = (HeapDraw*)heaps[cfg_HeapToDraw];
		if (region->chunks.size() == 0) {
			region = NULL;
			return;
		}
	}

	if (autoGrow)
		if (limits.size() < 1)
			ZoomAll();
		else
			ZoomMore();

	__try {
		glutPostRedisplay();
	} __except (1) {/* hah! got you GL! */};
}
