#include <vector>
#include <math.h>
#include <GL/gl.h>
// #include <GL/glx.h>
#include <GL/glut.h>

#include "Heap.hpp"

float deltaT;

unsigned drawingLeft, drawingBottom;
unsigned int screenWidth, screenHeight;
unsigned int colorTimeScale = 0;
unsigned int drawSubPixel = 1;
float selectionX,selectionY=0.0;
float selectionX1,selectionY1;

unsigned globalMinAddr, globalMaxAddr;
unsigned globalMinTime, globalMaxTime;

GLint heapList;

#define LIMITS  (limits.back())
#define LEFT	(limits.back().left)
#define RIGHT	(limits.back().right)
#define TOP		(limits.back().top)
#define BOTTOM	(limits.back().bottom)

HeapRegion *region;

class CScreenLimits {
public:
	float left, right, top, bottom;

	CScreenLimits(float l, float r, float t, float b) :
		left(l),
		right(r),
		top(t),
		bottom(b) {};
	
	void glOrtho() {
        unsigned int t,b;
        b=bottom;
        t=top;

	  	::glOrtho(left, right, top, bottom, 1.0, -1.0); };
};

std::vector <CScreenLimits> limits;

void setColor(unsigned int time) {
	float t0=(time-region->minTime-LEFT)/deltaT;
	float r,g,b;

	// if (t0 < 0.0) return;
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

	glColor3f(r,g,b);
//	printf("%f/%f=%f: %f %f %f\n", (time-minTime-left), deltaT, t0, r, g, b); 
}

void compile() {
	// heapList = glGenLists(1);

	deltaT = RIGHT-LEFT;

	// glNewList(heapList, GL_COMPILE);

    glBegin(GL_QUADS);
	for (set<HeapChunk>::iterator i=region->chunks.begin();i != region->chunks.end(); i++) {
        if (colorTimeScale)
            setColor(i->freeTime);
        else
            setColor(i->allocTime);

        glVertex2i(i->allocTime-region->minTime, i->top);
        glVertex2i(i->freeTime-region->minTime, i->top);
        glVertex2i(i->freeTime-region->minTime, i->base);
        glVertex2i(i->allocTime-region->minTime, i->base);
        // glVertex2i(i->allocTime-region->minTime, i->top-region->minAddr);
        // glVertex2i(i->freeTime-region->minTime, i->top-region->minAddr);
        // glVertex2i(i->freeTime-region->minTime, i->base-region->minAddr);
        // glVertex2i(i->allocTime-region->minTime, i->base-region->minAddr);
	}
    glEnd();

    if (drawSubPixel) {
        glBegin(GL_LINES);
        for (set<HeapChunk>::iterator i=region->chunks.begin();i != region->chunks.end(); i++) {
            if (colorTimeScale)
                setColor(i->freeTime);
            else
                setColor(i->allocTime);
            // glVertex2i(i->allocTime-region->minTime, i->top-region->minAddr);
            // glVertex2i(i->freeTime-region->minTime, i->top-region->minAddr);
            // glVertex2i(i->freeTime-region->minTime, i->base-region->minAddr);
            // glVertex2i(i->allocTime-region->minTime, i->base-region->minAddr);
            glVertex2i(i->allocTime-region->minTime, i->top);
            //glVertex2i(i->freeTime-region->minTime, i->top);
            glVertex2i(i->freeTime-region->minTime, i->base);
            //glVertex2i(i->allocTime-region->minTime, i->base);
        }
        glEnd();
        // glEndList();
    }
}

void draw() {
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	
	limits.back().glOrtho();

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glClear(GL_COLOR_BUFFER_BIT);
	compile();
    // glCallList(heapList);
    if (selectionX && selectionY) {
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glEnable(GL_BLEND);
        glColor4f(0.8, 0.8, 1.0, 0.5);
        glRectf(selectionX, selectionY, selectionX1, selectionY1);
        glDisable(GL_BLEND);
    }
	glFlush();
    glutSwapBuffers();
}

void reshape(GLsizei w, GLsizei h) {
	screenWidth = w;
	screenHeight = h;

	glViewport(0,0,w,h);
//	printf("%f-%f %f-%f\n",left, right, top, bottom);
}

void dump() {
    int r=0;
    printf("Region: [%d] Loose\n",r++);

    for (set<HeapChunk>::iterator j=heap.loose.chunks.begin();j != heap.loose.chunks.end(); j++)
        printf("\tChunk: 0x%08x-0x%08x Time: %lu-%lu\n", j->base, j->top, j->allocTime, j->freeTime);

	for (set<HeapRegion>::iterator i=heap.regions.begin();i != heap.regions.end(); i++) {
		printf("Region: [%d] 0x%08x-0x%08x Time: %lu-%lu\n", r++, i->base, i->top, i->allocTime, i->freeTime);
        for (set<HeapChunk>::iterator j=i->chunks.begin();j != i->chunks.end(); j++)
            printf("\tChunk: 0x%08x-0x%08x Time: %lu-%lu\n", j->base, j->top, j->allocTime, j->freeTime);
    }
}

void printLimits() {
    // printf("Limits: %f-%f %08x-%08x\n", (LEFT+region->minTime)/100000.0, (RIGHT+region->minTime)/100000.0, region->minAddr+(int)BOTTOM, region->minAddr+(int)TOP);
    printf("Limits: %f-%f 0x%08x-0x%08x (%u)\n", (LEFT+region->minTime)/100000.0, (RIGHT+region->minTime)/100000.0, (int)BOTTOM, (int)TOP, (unsigned)(TOP-BOTTOM));
}

void Zoom1() {
	if (!globalMinAddr) {
		// limits.push_back(CScreenLimits(0.0,region->getDeltaTime(),region->getDeltaAddr(),0.0));
		limits.push_back(CScreenLimits(0, region->getDeltaTime(), region->maxAddr, region->minAddr));
	} else 
		limits.push_back(CScreenLimits(0, globalMaxTime-globalMinTime, globalMaxAddr, globalMinAddr));
    printLimits();
}

void keystroke(unsigned char key, int x, int y) {
	float rl = 0.5*(RIGHT-LEFT), tb = 0.5*(TOP-BOTTOM);
	switch (key) {
        case 0x1b:  // Esc
		case 'q':
		case 'Q':
			std::exit(0);
		case 8:	// backspace
			if (limits.size() == 1)
			  	return;
			limits.pop_back();
			break;
		case '1':
			limits.clear();
			Zoom1();
			break;
        case 'f':   // flip color time scale
            colorTimeScale ^= 1;
            break;
		case 'h':	// move left
			limits.push_back(CScreenLimits(LEFT-rl, RIGHT-rl, TOP, BOTTOM));
			break;
		case 'l':	// move right
			limits.push_back(CScreenLimits(LEFT+rl, RIGHT+rl, TOP, BOTTOM)); 
			break;
		case 'k':	// move down
			limits.push_back(CScreenLimits(LEFT, RIGHT, TOP-tb, BOTTOM-tb));
			break;
        case 'x':   // draw subpixel
            drawSubPixel ^= 1;
            break;
		case 'j':	// move up
			limits.push_back(CScreenLimits(LEFT, RIGHT, TOP+tb, BOTTOM+tb));
			break;
		default:
			printf("[%c] %8x %d %d\n", key, key, x, y);
		        printLimits();
			return;
	}
    printLimits();
	glutPostRedisplay();
}

void screenToReal(int x, int y, float *time, float *addr) {
    // y+=20;
	*time = (float)x*(RIGHT-LEFT)/(float)screenWidth+LEFT;
	*addr = (float)y*(TOP-BOTTOM)/(float)screenHeight+BOTTOM;
}

float b2_x=0, b2_y=0;
void mouseMove(int x, int y) {
    screenToReal(x, y, &selectionX1, &selectionY1);
    glutPostRedisplay();
    if (b2_x && b2_y) {
        float xx,yy;
        screenToReal(x,y,&xx,&yy);
        b2_x = xx;
        b2_y = yy;
    }
}

void timer(int _) {
    if (b2_x && b2_y) {
        limits.push_back(CScreenLimits(
                    LEFT*0.75+b2_x*0.25,
                    RIGHT*0.75+b2_x*0.25,
                    TOP*0.75+b2_y*0.25,
                    BOTTOM*0.75+b2_y*0.25));
        printLimits();
        glutPostRedisplay();
    }
    glutTimerFunc(100,timer,0);
}

void mouseButton(int button, int state, int x, int y) {
	float xx,yy;
	float left, right, top, bottom;

    screenToReal(x, y, &xx, &yy);
	
	if (button == GLUT_LEFT_BUTTON) {
		if (state == GLUT_DOWN) {
			selectionX1 = selectionX = xx;
			selectionY1 = selectionY = yy;
		} else if (state == GLUT_UP) {
			if (selectionX<xx) {
				left = selectionX;
				right = xx;
			} else {
				right = selectionX;
				left = xx;
			}
			if (selectionY<yy) {
				top = yy;
				bottom = selectionY;
			} else {
				bottom = yy;
				top = selectionY;
			}
            if (left == right || top == bottom) return;
			limits.push_back(CScreenLimits(left, right, top, bottom));

            selectionX = selectionY = 0;
			glutPostRedisplay();
            printLimits();
		}
	} else 
	if (button == GLUT_RIGHT_BUTTON)
		if (state == GLUT_DOWN) {
			b2_x = xx;
			b2_y = yy;
		} else if (state == GLUT_UP) {
            b2_x = b2_y = 0;
        }
    else printf("%08x:%08x %d %d\n", button, state, x, y);
}

void startDrawing() {
    int r=1;
    extern int Argc;
    extern char **Argv;

   	if (verbose >= 1) {
		printf("Dumping...\n");
		dump();
	}

    if (numberOfRegion) {
        for (set<HeapRegion>::iterator i=heap.regions.begin();i != heap.regions.end(); i++,r++)
            if (r == numberOfRegion)
                region = (HeapRegion*)&*i;
    } else region = &heap.loose;

	printf("Calculating limits, chunks:%d\n",region->size());
    region->setLimits();

	printf("Drawing...\n");
	glutInit(&Argc, Argv);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
	glutCreateWindow("HeapDraw");
    glutSetCursor(GLUT_CURSOR_FULL_CROSSHAIR);
	glutDisplayFunc(draw);
	glutReshapeFunc(reshape);
	glutKeyboardFunc(keystroke);
	glutMouseFunc(mouseButton);
    glutTimerFunc(100,timer,0);
    glutMotionFunc(mouseMove);

	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

	Zoom1();

	// compile();
	glutMainLoop();
}
