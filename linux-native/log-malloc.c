#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <fcntl.h>

#define __USE_GNU
#include <dlfcn.h>

static void *(*real_malloc) (size_t size) = 0;
static void (*real_free) (void *ptr) = 0;
static void *(*real_realloc) (void *ptr, size_t size) = 0;
static void *(*real_calloc) (size_t nmemb, size_t size) = 0;
static unsigned int *start_sp = 0;
int memlog = 0;

static void init_me()
{
    if(memlog) return;
    real_malloc = dlsym(RTLD_NEXT, "malloc");
    real_calloc = dlsym(RTLD_NEXT, "calloc");
    real_free   = dlsym(RTLD_NEXT, "free");
    real_realloc = dlsym(RTLD_NEXT, "realloc");

    memlog = 2;
    if (getenv("LOG_MALLOC_OUTPUT"))
	memlog = open(getenv("LOG_MALLOC_OUTPUT"), O_RDWR | O_CREAT, 0666);

    if(memlog == -1)
      memlog = 2;
}

#define GETSP() ({ register unsigned int *stack_ptr asm ("esp"); stack_ptr; })
#define GETBP() ({ register unsigned int *stack_ptr asm ("ebp"); stack_ptr; })
#define GETRET() (*(GETBP()+1))

void
__attribute__ ((constructor))
_init (void)
{
  start_sp = GETSP();
  init_me();
}

static void print_stack() {
    return;

    unsigned int *bp = GETBP();
#if 0
    while(bp < start_sp) {
      Dl_info i;
      if(dladdr(*(bp+1), &i)) {
	if(i.dli_sname) {
	  fprintf(memlog, "  %s:%s:0x%08x", basename(i.dli_fname), i.dli_sname, *(bp+1));
	} else {
	  fprintf(memlog, "  %s::0x%08x", i.dli_fname, *(bp+1));
	}
      } else {
	fprintf(memlog, "  <>:0x%08x", *(bp+1));
      }
      bp = *bp;
    }
#endif
}

void flog(char *msg, ...) {
    va_list ap;
    struct timeval tv;
    struct tm _tm, *timeM=&_tm;
    static adentro = 0;
    static no_time = 0;

    char buf[1000];

    if (adentro || !memlog) return;
    ++adentro;

    va_start(ap, msg);

    // gettimeofday(&tv,NULL);
    // timeM = localtime(&(tv.tv_sec));

    timeM->tm_hour = 0;
    timeM->tm_min = 0;
    timeM->tm_sec = 0;
    tv.tv_usec = no_time++;

    sprintf(buf, "%i %02d:%02d:%02d.%06d ",
          getpid(),
          timeM->tm_hour,
          timeM->tm_min,
          timeM->tm_sec,
          tv.tv_usec);

    vsprintf(buf+strlen(buf), msg, ap);
    strcat(buf, "\n");

    write(memlog, buf, strlen(buf));
    adentro--;
}

void *malloc(size_t size)
{
    void *p;

    if(!real_malloc) {
    	real_malloc = dlsym(RTLD_NEXT, "malloc");
	if(!real_malloc) return NULL;
    }

    p = real_malloc(size);

    flog("[0x%08x] malloc(%u)\t= 0x%08x ",GETRET() , size, p);

    return p;
}

void *calloc(size_t nmemb, size_t size)
{
    void *p;

    if(!real_calloc)
	return NULL;

    p = real_calloc(nmemb, size);

    flog("[0x%08x] calloc(%u,%u)\t= 0x%08x", GETRET(), nmemb, size, p);

    return p;
}

void free(void *ptr)
{
    if(!real_free) {
    	real_free = dlsym(RTLD_NEXT, "free");
	if(!real_free) return;
    }

    flog("[0x%08x] free(0x%08x)\t= <void>", GETRET(), ptr);

    real_free(ptr);
}

void *realloc(void *ptr, size_t size)
{
    void *p;
    if(!real_realloc) {
    	real_realloc = dlsym(RTLD_NEXT, "realloc");
	if(!real_realloc) return NULL;
    }

    p = real_realloc(ptr, size);

    flog("[0x%08x] realloc(0x%08x, %d)\t= 0x%08x", GETRET(), ptr, size, p);
    return p;
}

