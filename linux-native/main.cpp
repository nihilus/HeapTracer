#include <stdlib.h>
#include "Heap.hpp"

// To do
// [ ] Make a stack to mantain arguments (specially for truss input, where calls may be nested)
using namespace std;

int verbose = 0, pidToFollow = -1, quiet=0, numberOfRegion = 0;

Heap heap;

void ltrace_input() {
	// File format is
	// 1900 16:43:29.397714 [08053bc1] malloc(9) = 0x0805e050
	// 1900 16:43:29.397714 [08053bc1] realloc(0x0805e050, 100) = 0x0805e100
	// 1900 16:43:29.398460 [08053bc1] free(0x08097c60) = <void>

	char line[1000];
	char call[1000],answer[1000];
    unsigned int base, newBase, size, caller, pid, time;
	int r,h,m,s;

	printf("Reading input from ltrace's output\n");
	while (!feof(stdin)) {
		fgets(line, sizeof(line)-1, stdin);
		line[1999]=0;
		r=sscanf(line, "%d %d:%d:%d.%d [%p] %s = %s", &pid, &h, &m, &s, &time, &caller, &call, &answer);
		//pid=0;
        if (r < 7) {
            qprintf(("Unrecognized line: %s", line));
            continue;
        }
		if (r < 8) 
			r=sscanf(line, "%d %d:%d:%d.%d [%p] %s %d) = %s", &pid, &h, &m, &s, &time, &caller, &call, &size, &answer);
		time += ((h*60+m)*60+s)*1000000;
		verprintf(4,("PID: %d TIME: %lu ADDR: %08x CALL: %s ANSWER: %s\n", pid, time, caller, call, answer));

		if (pidToFollow == 0) pidToFollow = pid;
		if (pidToFollow == -1 || pidToFollow == (int)pid) {
			if (!strncmp(call,"malloc",6)) {
				sscanf(answer, "0x%p", &base);
				sscanf(call, "malloc(%d)", &size);

                heap.alloc(base, size, time, pid, caller);
            } else if (!strncmp(call, "calloc", 6)) {
				sscanf(answer, "0x%p", &base);
				sscanf(call, "calloc(%d, %d", &size, &s);
                size *= s;

                heap.alloc(base, size, time, pid, caller);
			} else if (!strncmp(call, "free", 4)) {
				sscanf(call, "free(0x%p)", &base);

                heap.free(base, time, pid, caller);
			} else if (!strncmp(call, "realloc", 7)) {
				sscanf(call, "realloc(0x%p", &base);
				sscanf(answer, "0x%p", &newBase);

			printf("realloc(0x%x, %d) = 0x%\n", base, size, newBase);

                heap.realloc(base, newBase, size, time, pid, caller);
			}
		}
	}
}

void truss_input() {
	// File format is
	// 4388/1@1:	 0.3619	-> libSDtFwa:malloc(0x20, 0x87d4c, 0xc, 0xff31a000)
	// 4388/1@1:	 0.3630	  -> libc:mmap(0xfe982000, 0x2000, 0x3, 0x12)
	// 4388/1@1:	 0.3638	  <- libc:mmap() = 0xfe982000
	// 4388/1@1:	 0.3647	<- libSDtFwa:malloc() = 0xfe982000
	// 4388/1@1:	 0.3695	-> libSDtFwa:malloc(0x1ff0, 0x0, 0x0, 0xff31a000)
	// 4388/1@1:	 0.3706	-> libc:malloc(0x1ff0, 0x0, 0x0, 0xff31a000)
	// 4388/1@1:	 0.3717	<- libSDtFwa:malloc() = 0x48650
	// 4388/1@1:	 0.3724	-> libSDtFwa:malloc(0x800, 0x48650, 0xc, 0x2000)
	// 4388/1@1:	 0.3734	-> libc:malloc(0x800, 0x48650, 0xc, 0x2000)
	// 4388/1@1:	 0.3743	<- libSDtFwa:malloc() = 0x4a648
	// 4388/1@1:	 0.3751	-> libSDtFwa:malloc(0x8, 0x0, 0x0, 0x0)
	// 4388/1@1:	 0.3764	  -> libc:mmap(0xfec02000, 0x2000, 0x3, 0x12)
	// 4388/1@1:	 0.3772	  <- libc:mmap() = 0xfec02000
	// 4388/1@1:	 0.3781	<- libSDtFwa:malloc() = 0xfec02000
	// 4388/1@1:	 0.3788	-> libSDtFwa:malloc(0x10, 0x878c8, 0x71, 0x159a0)
	// 4388/1@1:	 0.3798	<- libSDtFwa:malloc() = 0xfeb12010
	// 4388/1@1:	 0.3808	-> libSDtFwa:malloc(0x8, 0x8759c, 0x11a, 0xff2913d0)

	char line[1000];
	char library[1000],*call,*arg, direction[1000];
    unsigned int newSize, size, caller, pid, time; 
	unsigned int r,h,m,s,arg1,arg2,answer;

	printf("Reading input from truss output\n");
	while (!feof(stdin)) {
		fgets(line, sizeof(line)-1, stdin);
		line[999]=0;
		r=sscanf(line, "%d%*s\t %d.%d\t%s %s 0x%p", &pid, &s, &time, &direction, &library, &arg2);

        if (r != 6) 
            sscanf(line, "%d%*s\t %d.%d\t%s %s = 0x%p", &pid, &s, &time, &direction, &library, &answer);

		time += s*10000;

		call = strchr(library, ':');
		if (!call) continue;
		*call = 0;
		call++;

        arg = strchr(call, '(');
        if (arg) {
            *arg = 0;
            if (direction[0] == '-') {
                arg+=3;
                sscanf(arg, "%p", &arg1);
            }
        }

        fflush(stdout);

		if (pidToFollow == 0) pidToFollow = pid;
		if (pidToFollow == -1 || pidToFollow == (int)pid) {
            verprintf(4,("PID: %d TIME: %lu AUX: %s LIB: %s CALL: %s ARG1: %x ARG2: %x ANSWER: %x\n", pid, time, direction, library, call, arg1, arg2, answer));
            if (direction[0] == '-') {
                // -> entering call
                if (!strncmp(call, "free", 4)) {
                    if (strcmp("libc",library) && strcmp("libmtmalloc",library)) continue;
                    heap.free(arg1, time, pid);
                } else if (!strcmp(call, "malloc"))
                    size = arg1;
                else if (!strcmp(call, "realloc"))
                    newSize = arg2;
            } else {
                // <- returning from call
                if (!strncmp(call,"malloc", 6)) {
                    heap.alloc(answer, size, time, pid);
                    
                } else if (!strncmp(call,"mmap", 4)) {
                    heap.mmap(answer, arg2, time, pid);
                    
                } else if (!strncmp(call, "realloc", 7)) {
                    heap.realloc(arg1, answer, newSize, time, pid);
                }
            }
		}
	}
}

void ntsd_input() {
	// File format is
    // Opened log file 'c:\l'
    /*
     
    0:000> g
    <ntdll!RtlAllocateHeap>
    $tid=000005b4
    Debugger (not debuggee) time: Tue Jun 22 18:07:47.186 2004 (GMT-3)
    0006f904  77f8d88d 00070000 00180000 00000020
    *** ERROR: Module load completed but symbols could not be loaded for calc.exe
    *** ERROR: Symbol file could not be found.  Defaulted to export symbols for C:\WINNT\system32\SHLWAPI.DLL - 
    *** ERROR: Symbol file could not be found.  Defaulted to export symbols for C:\WINNT\system32\COMCTL32.DLL - 
    *** ERROR: Symbol file could not be found.  Defaulted to export symbols for C:\WINNT\system32\RPCRT4.DLL - 
    *** ERROR: Symbol file could not be found.  Defaulted to export symbols for C:\WINNT\system32\USER32.DLL - 
    *** ERROR: Symbol file could not be found.  Defaulted to export symbols for C:\WINNT\system32\GDI32.DLL - 
    *** ERROR: Symbol file could not be found.  Defaulted to export symbols for C:\WINNT\system32\msvcrt.dll - 
    *** ERROR: Symbol file could not be found.  Defaulted to export symbols for C:\WINNT\system32\SHELL32.dll - 
    *** ERROR: Symbol file could not be found.  Defaulted to export symbols for C:\WINNT\system32\ADVAPI32.DLL - 
    *** ERROR: Symbol file could not be found.  Defaulted to export symbols for C:\WINNT\system32\KERNEL32.DLL - 
    eax=00072858
    </ntdll!RtlAllocateHeap>
    <ntdll!RtlAllocateHeap>
    $tid=000005b4
    Debugger (not debuggee) time: Tue Jun 22 18:07:47.717 2004 (GMT-3)
    0006f538  77f91e63 00070000 00000000 00000022
    eax=00072890
    </ntdll!RtlAllocateHeap>
    <ntdll!RtlAllocateHeap>
    $tid=000005b4
    Debugger (not debuggee) time: Tue Jun 22 18:07:47.827 2004 (GMT-3)
    0006f4f8  77f896b6 00070000 00000000 0000000c
    eax=000728d0
    </ntdll!RtlAllocateHeap>
    <ntdll!RtlFreeHeap>
    $tid=000005b4
    Debugger (not debuggee) time: Tue Jun 22 18:07:47.957 2004 (GMT-3)
    0006f52c  77f8970f 00070000 00000000 000728d0
    eax=00000001
    </ntdll!RtlFreeHeap>
    <ntdll!RtlAllocateHeap>
    $tid=000005b4
    Debugger (not debuggee) time: Tue Jun 22 18:07:48.077 2004 (GMT-3)
    0006f5ac  77f8b60c 00170000 00000000 0000001c
    eax=00170688
    </ntdll!RtlAllocateHeap>
    <ntdll!RtlFreeHeap>
    $tid=000005b4
    Debugger (not debuggee) time: Tue Jun 22 18:07:48.218 2004 (GMT-3)
    0006f5bc  77f8b644 00170000 00000000 00170688
    eax=00000001
    </ntdll!RtlFreeHeap>
    <ntdll!RtlAllocateHeap>
    $tid=000005b4
    Debugger (not debuggee) time: Tue Jun 22 18:07:48.338 2004 (GMT-3)
    0006f680  77f8301a 00070000 00000000 00000009
    eax=000728d0
    </ntdll!RtlAllocateHeap>
    */

	char line[1000],*p,g[1000];
    unsigned int caller, tid, time; 
	unsigned int r,h,m,s,arg1,arg2,arg3,arg4,answer;

	printf("Reading input from ntsd output\n");
	while (!feof(stdin)) {
		fgets(line, sizeof(line)-1, stdin);
		line[999]=0;

        if (line[0] == '<' && line[1] == '/') {
            // it's a closing tag
            if (NULL != (p = strchr(line,'>'))) *p=0;
            verprintf(4,("[%d] %d: %s(%x, %x, %x) = %x\n", tid, time, line, arg1, arg2, arg3, answer));
            if (pidToFollow == 0) pidToFollow = tid;
            if (pidToFollow == -1 || pidToFollow == (int)tid) {
                if (!strcmp("</ntdll!RtlAllocateHeap",line))
                    heap.alloc(answer, arg3, time, tid, caller);
                if (!strcmp("</ntdll!RtlReAllocateHeap",line))
                    heap.realloc(arg3, answer, arg4, time, tid, caller);
                if (!strcmp("</ntdll!RtlFreeHeap",line)) 
                    heap.free(arg3, time, tid, caller);
            }
        }
        
        // Thread ID
        r = sscanf(line, "$tid=%p", &tid);
        if (r==1) continue;

        // Time
        r = sscanf(line, "Debugger (not debuggee) time: %*s %*s %*s %d:%d:%d.%d", &h, &m, &s, &time);
        if (r==4) {
            time += ((((h*60)+m)*60)+s)*1000;
            continue;
        }
        
        // Stack (Arguments)
        r = sscanf(line, "%*p %p %p %p %p", &arg4, &arg1, &arg2, &arg3);
        if (r == 4) {
            caller = arg4;
            continue;
        }
        // next line for multiline stack dump (berp!)
        if (r == 1) continue;

        // answer
        r = sscanf(line, "eax=%p", &answer);
        if (r == 1) continue;
	}
}

typedef void (*input_format_function)();

typedef struct {
	char *name;
	char *description;
	input_format_function function;
} lGeraStruct;

lGeraStruct input_formats[] = {
	{"ltrace", "ltrace -o ls.ltrace -f -tt -i -efork,vfork,clone,malloc,realloc,free,calloc,mmap,munmap,mremap,mprotect,mmap2 /bin/ls", ltrace_input},
	{"truss", "truss -o ls.truss -f -d -l -t \\!all -u a.out,*::malloc,realloc,free,calloc,fork,vfork,mmap,munmap,mprotect,brk /bin/ls", truss_input},
	{"ntsd", "ntsd -cf hd.ntsd calc.exe", ntsd_input},
};

#define INPUT_FORMATS (sizeof(input_formats)/sizeof(*input_formats))

input_format_function lookup_input_format(char *name) {
	int i;
	for (i=0; i<INPUT_FORMATS;i++)
		if (!strncmp(name, input_formats[i].name, strlen(name))) return input_formats[i].function;
	return NULL;
}

int usage() {
	int i;
	printf("Use: g [-a] [-q] [-v] [-p pid] [-r region] -R [lo-hi] -t type\n");
	printf("\ta\tautoregions: automatically find regions\n");
	printf("\tq\tquiet: don't print found errors\n");
	printf("\tv\tverbose, more is more\n");
	printf("\tp\tPID to follow, default is all, 0 is first, other is specific PID\n");
	printf("\tr\tregion to draw (0 is loose region, and it's the default)\n");
	printf("\tR\tManually define a region. lo and hi are its limits (can be hex)\n");
	printf("\tt\tinput file type, where type is one of:\n");
	for (i=0;i<INPUT_FORMATS;i++)
		printf("\t\t\t%s\n\t\t\t\t%s\n", input_formats[i].name, input_formats[i].description);

	return 1;
}

int  opterr = 1;
int  optind = 1;
int  optopt;
char *optarg;

int getopt(int argc, char **argv, char *opts) {
    char *p,c;
    static int sub = 1;

	if(sub == 1)
		if(optind >= argc || argv[optind][0] != '-' || argv[optind][1] == '\0')
            return -1;
		else if(strcmp(argv[optind], "--") == NULL) {
			optind++;
			return -1;
		}

	optopt = c = argv[optind][sub];
	if(c == ':' || (p=strchr(opts, c)) == NULL) {
		if(argv[optind][++sub] == '\0') {
			optind++;
			sub = 1;
		}
		return '?';
	}
	if(*++p == ':') {
		if(argv[optind][sub+1] != '\0')
			optarg = &argv[optind++][sub+1];
		else if(++optind >= argc) {
            fprintf(stderr,"'%c' options requires an argument", c);
			sub = 1;
			return '?';
		} else
			optarg = argv[optind++];
		sub = 1;
	} else {
		if(argv[optind][++sub] == '\0') {
			sub = 1;
			optind++;
		}
		optarg = NULL;
	}
	return c;
}

int Argc;
char **Argv;

int main(int argc, char **argv) {
	int c=0;
    unsigned int lo, hi;
	input_format_function read_input=NULL;

	Argc = argc;
	Argv = argv;
	while (c!=-1) {
		switch (c=getopt(argc, (char * const *)argv, "qvt:p:r:aR:")) {
			case 'q': // quiet
				quiet = !quiet;
				break;
			case 't': // input type
				read_input = lookup_input_format(optarg);
				if (!read_input) return usage();
				printf("Found input type.\n");
				break;
			case 'v': // verbose
				verbose++;
				break;
			case 'p': // pid to follow
				pidToFollow = atoi(optarg);
				break;
            case 'R': // define a region, can have many -R
                if (2 != (c = sscanf(optarg, "0x%x-0x%x", &lo, &hi))) 
                    return usage();
                heap.mmap(lo,hi-lo);
            case 'r': // number of region to draw
                numberOfRegion = atoi(optarg);
                break;
            case 'a': // auto regions
                heap.setAutoRegions(1);
                break;
			case -1:
				break;
			default:
				return usage();
		}
	}

	if (!read_input) return usage();
	
	read_input();
	qprintf(("Total number of chunks: %d\n",heap.loose.size()));
	startDrawing();
}

// Limits: 31.7406-31.7577 56bd8    (no hay free para este chunk) ?
// Limits: 31.7486-31.7486 58be0
// Limits: 47.1826-47.1827 56dd8
//
// Limits: 309900.656250-309901.375000 4daf8-4daf8
//


