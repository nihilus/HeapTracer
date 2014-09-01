// make -D__NT__ && copy /y ..\..\bin\plugins\HeapDraw.plw \progra~1\ida\plugins && \progra~1\ollydbg\ollydbg.idb

#include <windows.h>

#include <ida.hpp>
#include <idp.hpp>
#include <dbg.hpp>
#include <name.hpp>
#include <bytes.hpp>
#include <loader.hpp>
#include <kernwin.hpp>

#include <stdlib.h>

#include "hook.hpp"
#include "Heap.hpp"
#include "HeapChunk.hpp"

// Todo:
// [ ] Make "double free" a little bit more inteligent (specially when attaching to a process)
// [ ] Create "Segment" concept (a heap is a collection of segments)
// [x] Better handling of multithreaded applications (a return address per stack at least)
//     [ ] One return address per thread is not enough, we need to save the arguments too
// [ ] Rescan Heap
//     [x] Basic rescan of used chunks in main segment
//     [ ] Scan extra segments
//     [x] Pay attention to lookaside lists (where chunks are still marked busy, but are actually free)
//     [ ] Put some checks (prev->size == prevSize, get_long() != -1, etc)
// [ ] Get list of heaps following the Heaps list starting at PEB
//     [ ] Get peb address looking for a segment named TIB and from there...
// [ ] Solve race condition when more than a single bp is set for the same function but diff threads.
//
// UI:
// [ ] Show blocks in FreeList/Lookaside somehow different (red border? shadded?)
// [ ] Select chunk, "break on access", "trace access" (ignore libraries)
// [ ] Select chunk, "breal on allocation" (for next execution, not straightforward)
// [ ] Global option: break on inter-chunk space usage (ignore libraries)
// [ ] Selection/tree list where you can select which Heaps (and Segments) to draw
//     [ ] Single selection list (easier)
//     [ ] Multiple selection lists which may
//         [ ] Open several windows
//         [ ] Open a single window horizontally splet.
// [ ] Let the user "delete" memory regions from the display (selects a region and hits DELETE:
//     the selected memory range disapears from the display, usefull to delete empty ranges)

// Notes
// bytes.hpp: chunkstart(), chunksize(), etc.

#define elements_of(x)      (sizeof(x)/sizeof(x[0]))

static bool cfg_GlobalTrace		= false;
static bool cfg_BreakOnMyBpts	= false;
static bool cfg_Verbose			= false;
static bool cfg_Graphics		= true;
bool		cfg_ShowEvents		= true;
ea_t        cfg_HeapToDraw		= 0xA0000;

// static bool only_once           = true;
// static bool attach_lost_bpt_hack = false;
// void (idaapi *old_stopped_at_debug_event)(const debug_event_t *event, bool dlls_added) = NULL;
// debugger_t *known_dbg = NULL;

static bool doneResolving	= false;
static HANDLE graphicThread	= NULL;

DWORD WINAPI startDrawing(LPVOID _);
void invalidateDisplay();
void restartDisplay();

int log(const char *format,...);
bool handle_RtlAllocateHeap(Hook &hook, ea_t ea, hook_evt_t event);
bool handle_RtlFreeHeap(Hook &hook, ea_t ea, hook_evt_t event);
bool handle_RtlReAllocateHeap(Hook &hook, ea_t ea, hook_evt_t event);
bool handle_AllocMem(Hook &hook, ea_t ea, hook_evt_t event);
bool handle_FreeMem(Hook &hook, ea_t ea, hook_evt_t event);

Hook hooks[] = {
    {"NTDLL_RtlAllocateHeap", handle_RtlAllocateHeap},
    {"ntdll_RtlAllocateHeap", handle_RtlAllocateHeap},
    {"NTDLL_RtlFreeHeap", handle_RtlFreeHeap},
    {"ntdll_RtlFreeHeap", handle_RtlFreeHeap},
//	{"loc_76A9256C", handle_AllocMem},
//	{"loc_76A9232A", handle_FreeMem},

//    {"NTDLL_RtlReAllocateHeap", handle_RtlReAllocateHeap},
//    {"NTDLL_RtlCreateHeap", handle_RtlCreateHeap},
//    {"NTDLL_RtlDestroyHeap", handle_RtlDestroyHeap},
};

bool handle_AllocMem(Hook &hook, ea_t ea, hook_evt_t evt) {
    switch (evt) {
        case EVT_FUNCTION_ENTER:
            if (!hook.read_args(3)) {
                log("WARNING: Couldn't read arguments for %s.\n", hook.name);
                return false;
            }

            hook.hook_ret(0x76A92572);
            break;

        case EVT_FUNCTION_EXIT:
            ulonglong eax;

            hook.unhook_ret(ea);

            if (!get_reg_val("EAX", &eax)) {
                log("WARNING: Couldn't read return value for %s.\n", hook.name);
                break;
            }

            getHeap(hook.args[0])->malloc(0, eax, hook.args[2]);

            if (cfg_Verbose) log("AllocMem(0x%08x, %08x, %d) = 0x%08x\n",
                    hook.args[0], hook.args[1], hook.args[2], eax);
            break;
    }

    return true;
}

bool handle_RtlAllocateHeap(Hook &hook, ea_t ea, hook_evt_t evt) {
    switch (evt) {
        case EVT_FUNCTION_ENTER:
            if (!hook.read_args(4)) {
                log("WARNING: Couldn't read arguments for %s.\n", hook.name);
                return false;
            }

            hook.hook_ret(hook.args[0]);
            break;

        case EVT_FUNCTION_EXIT:
            ulonglong eax;

            hook.unhook_ret(ea);

            if (!get_reg_val("EAX", &eax)) {
                log("WARNING: Couldn't read return value for %s.\n", hook.name);
                break;
            }

            getHeap(hook.args[1])->malloc(0, eax, hook.args[3]);

            if (cfg_Verbose) log("RtlAllocateHeap(0x%08x, %08x, %d) = 0x%08x\n",
                    hook.args[1], hook.args[2], hook.args[3], eax);
            break;
    }

    return true;
}

bool handle_FreeMem(Hook &hook, ea_t ea, hook_evt_t evt) {
    switch (evt) {
        case EVT_FUNCTION_ENTER:
            if (!hook.read_args(2)) {
                log("WARNING: Couldn't read arguments for %s.\n", hook.name);
                return false;
            }

            getHeap(hook.args[0])->free(0, hook.args[2]);

            if (cfg_Verbose) log("FreeMem(0x%08x, %08x)\n",
                    hook.args[0], hook.args[1]);

            break;

        case EVT_FUNCTION_EXIT:
            log("WARNING: got an EVT_FUNCTION_EXIT on %s.", hook.name);
            break;
    }

    return true;
}

bool handle_RtlFreeHeap(Hook &hook, ea_t ea, hook_evt_t evt) {
    switch (evt) {
        case EVT_FUNCTION_ENTER:
            if (!hook.read_args(4)) {
                log("WARNING: Couldn't read arguments for %s.\n", hook.name);
                return false;
            }

            getHeap(hook.args[1])->free(0, hook.args[3]);

            if (cfg_Verbose) log("RtlFreeHeap(0x%08x, %08x, %08x)\n",
                    hook.args[1], hook.args[2], hook.args[2]);

            break;

        case EVT_FUNCTION_EXIT:
            log("WARNING: got an EVT_FUNCTION_EXIT on %s.", hook.name);
            break;
    }

    return true;
}

// Not called: RtlReAllocateHeap() [internally] calls RtlAllocateHeap() and RtlFreeHeap()
bool handle_RtlReAllocateHeap(Hook &hook, ea_t ea, hook_evt_t evt) {
    switch (evt) {
        case EVT_FUNCTION_ENTER:
            if (!hook.read_args(5)) {
                log("WARNING: Couldn't read arguments for %s.\n", hook.name);
                return false;
            }

            hook.hook_ret(hook.args[0]);
            break;

        case EVT_FUNCTION_EXIT:
            ulonglong eax;

            hook.unhook_ret(ea);

            if (!get_reg_val("EAX", &eax)) {
                log("WARNING: Couldn't read return value for %s.\n", hook.name);
                return false;
            }

            getHeap(hook.args[1])->realloc(0, hook.args[1], eax, hook.args[3]);

            if (cfg_Verbose) log("RtlReAllocateHeap(0x%08x, %08x, %08x, %d) = 0x%08x\n",
                    hook.args[1], hook.args[2], hook.args[3], hook.args[4], eax);
            break;
    }

    return true;
}

void hookAll() {
	if (doneResolving) return;
    doneResolving = true;
    for (int i=0; i<elements_of(hooks); i++)
        doneResolving &= hooks[i].hook();
}

void unhookAll() {
    for (int i=0; i<elements_of(hooks); i++)
        hooks[i].unhook();
}

bool handle_MyBreakpoint(ea_t ea) {
    bool handled = false;

    for (int i=0; i<elements_of(hooks); i++)
        handled |= hooks[i].handle(ea);

	if (handled) invalidateDisplay();

    return handled;
}

static int idaapi handle_dbg_evt(void * /*user_data*/, int event_id, va_list va) {
	const debug_event_t *event;

	if (!cfg_GlobalTrace) return 0;
    
    // log("Event %d\n", event_id);

    switch (event_id) {
		case dbg_process_start:
		case dbg_process_attach:
            event = va_arg(va, debug_event_t *);
			clearHeaps();
			restartDisplay();
			doneResolving = false;
                        if (event_id == dbg_process_attach)
                        {
                          dbg->stopped_at_debug_event(true);
                          hookAll();
                        }
            break;

        case dbg_process_exit:
		case dbg_process_detach:
            unhookAll();
            break;
 
		case dbg_library_load:
            event = va_arg(va, debug_event_t *);
              dbg->stopped_at_debug_event(true);
              hookAll();
			
            break;

        case dbg_bpt:
            // thread_id_t tid;
            ea_t ea;
            // int *warn;

            /* tid = */ va_arg(va, thread_id_t);
            ea = va_arg(va, ea_t);
            // *warn = va_arg(va, int *);

			if (!handle_MyBreakpoint(ea))
				getHeap(cfg_HeapToDraw)->addEvent("Breakpoint");
            break;
        case dbg_run_to:
            log("!!!!!!!!!!!! RUN TO !!!!!!!!!!!!!!!1\n");
			break;
    };

    return 0; 
}

#define CHUNK_BUSY				1
#define CHUNK_VIRTUAL_ALLOC		8
#define CHUNK_LAST_ENTRY		0x10

void RescanHeap(ea_t handle) {
	struct _CHUNK {
		unsigned short size;			// size >> 3
		unsigned short prev_size;		// size >> 3
		unsigned char  segment_index;
		unsigned char  flags;
		unsigned char  unused_bytes;
        unsigned char tag_index;
	} chunk, *chunkAddr;

	struct _LOOKASIDE {
		struct _CHUNK *first;
		unsigned short realDepth;
		unsigned short sequence;

		unsigned short Depth;
		unsigned short MaximumDepth;

		unsigned long unused[9];

	} lookaside, *lookasideAddr;

	Heap *heap;

	heap = getHeap(handle);

	chunkAddr = (struct _CHUNK*)handle;

	// scan heap chunks
	do {
		invalidate_dbgmem_contents((ea_t)chunkAddr, sizeof *chunkAddr);

		chunk.size  = get_long((ea_t)&chunkAddr->size);
		chunk.flags = get_long((ea_t)&chunkAddr->flags);

		if (chunk.flags & CHUNK_BUSY) 
			heap->malloc(0,(ea_t)(chunkAddr+1),chunk.size << 3);
		if (chunk.flags & CHUNK_LAST_ENTRY)
			break;
		chunkAddr = (struct _CHUNK*)(((char*)chunkAddr) + (chunk.size << 3));
	} while (chunk.size);

	// free chunks in lookaside lists (this chunks are marked busy, but are actually free)
	invalidate_dbgmem_contents(handle+0x580, 4);	// handle+offsetof(_HEAP, Lookaside), 4); //
	lookasideAddr = (struct _LOOKASIDE*)get_long(handle+0x580);			// offsetof(_HEAP, Lookaside));

	if (lookasideAddr) {
		invalidate_dbgmem_contents((ea_t)lookasideAddr, sizeof(_LOOKASIDE)*128);
		for (int i=0; i<128; i++) {
			chunkAddr = lookaside.first = (struct _CHUNK*)get_long((ea_t)&lookasideAddr[i].first);
			if (lookaside.first) {
				lookaside.realDepth = get_long((ea_t)&lookasideAddr[i].realDepth);
				for (;lookaside.realDepth;lookaside.realDepth--) {
					heap->free(0, (ea_t)chunkAddr);
					invalidate_dbgmem_contents((ea_t)chunkAddr, 4);
					chunkAddr = (struct _CHUNK*)get_long((ea_t)chunkAddr);
				}
			}
		}
	}

}

// ------------------------------
// IDA Plugin structure

char wanted_name[] = "HeapTracer";

int idaapi init(void) {
  const char *options = get_plugin_options(wanted_name);
  if ( options != NULL )
    warning("command line options: %s", options);

  hook_to_notification_point(HT_DBG, handle_dbg_evt, NULL);

  return PLUGIN_KEEP;
}

void idaapi term(void) {
  TerminateThread(graphicThread, 0);
  unhook_from_notification_point(HT_DBG, handle_dbg_evt);
}

void open_close_window(int open) {
    unsigned long status;
    bool threadRunning;

	if (!GetExitCodeThread(graphicThread, &status))
		threadRunning = false;
	else
  		threadRunning = status == STILL_ACTIVE;

	if (!threadRunning && open) {
        graphicThread = CreateThread(NULL, 100000, &startDrawing, NULL, 0, NULL);
        invalidateDisplay();
    }
    if (!open && threadRunning)
        TerminateThread(graphicThread, 0);
}

static bool get_user_prefs() {

#define CHK_TRACE		1
#define CHK_BREAK		2
#define CHK_VERBOSE		4
#define CHK_GRAPHIC		8
#define CHK_SHOWEVT		0x10
#define CHK_RESCANHEAP	0x20

	bool cfg_rescanHeap = false;

    short the_chkbx =
        cfg_GlobalTrace   * CHK_TRACE +
        cfg_BreakOnMyBpts * CHK_BREAK +
        cfg_Verbose       * CHK_VERBOSE +
        cfg_Graphics      * CHK_GRAPHIC +
		cfg_ShowEvents    * CHK_SHOWEVT +
		cfg_rescanHeap	  * CHK_RESCANHEAP +
        0;

    const char dialog[] = 
        "STARTITEM 0\n"
        "HELP\n"
        "The text seen in the 'help window' when the\n"             
        "user hits the 'Help' button.\n"
        "ENDHELP\n"

        "HeapTracer settings\n"                                 
        "This is the main text in the dialog.\n"

            "\n<#Global switch to turn on and off the plugin#"              
                "Enable heap tracing:C>"
            "\n<#Whether to stop or not on heap tracing breakpoints#"
                "Break on heap tracing breakpoints:C>"
            "\n<#Whether to log to the system log the function calls#"
                "Log to System Log:C>"
            "\n<#Whether to open or not the graphical window#"
                "Open graphical window:C>"
            "\n<#Whether to show or not collected events#"
                "Show events:C>"
			"\n<#Rescan the Heap to populate previously allocated chunks. This will clear all known about the heap, and then call WalkHeap() to find out what blocks are allocated.#"
            "Rescan heap:C>"
        ">"
        "\n<Heap to draw:$:16:16::>"
        "\n\n";

	unsigned int newHeapToDraw = cfg_HeapToDraw;
    int ok = AskUsingForm_c( dialog, &the_chkbx, &newHeapToDraw);
	// newHeapToDraw = choose_segm("segmento", (ea_t)newHeapToDraw)->startEA;


    if (!ok) return false;

    log("HeapToDraw: %08x\n", newHeapToDraw);

    cfg_GlobalTrace   = the_chkbx & CHK_TRACE;
    cfg_BreakOnMyBpts = the_chkbx & CHK_BREAK;
    cfg_Verbose       = the_chkbx & CHK_VERBOSE;
    cfg_Graphics      = the_chkbx & CHK_GRAPHIC;
	cfg_ShowEvents    = the_chkbx & CHK_SHOWEVT;
	cfg_rescanHeap	  = the_chkbx & CHK_RESCANHEAP;

	if (cfg_HeapToDraw != newHeapToDraw) {
		cfg_HeapToDraw = newHeapToDraw;
		restartDisplay();
	}

	if (cfg_rescanHeap) {
		clearHeap(cfg_HeapToDraw);
		RescanHeap(cfg_HeapToDraw);
	}

	invalidateDisplay();

	open_close_window(cfg_Graphics);
	return true;
}

void dump_heaps() {
    map<unsigned int, Heap *>::iterator heap;
    // ts_vector<HeapChunk *>::iterator chunk;

    for (heap = heaps.begin(); heap != heaps.end(); ++heap) {
		log("Heap (%08x):  chunks: %d [%08x:%08x -> %d:%d]\n", 
                heap->first,
				heap->second->chunks.size(),
                heap->second->base,
                heap->second->top,
                heap->second->allocTime,
                heap->second->freeTime);
/*        for (chunk = heap->second->chunks.begin(); chunk != heap->second->chunks.end(); ++chunk) {
            log("  Chunk:  %08x:%d -> %08x:%d\n", 
                    (*chunk)->base,
                    (*chunk)->allocTime,
                    (*chunk)->top,
                    (*chunk)->freeTime);
        }
*/    }
}

void idaapi run(int /*arg*/) {
    dump_heaps();
	get_user_prefs();
}

char comment[] = "This plugin will do the logging needed to use HeapDraw";

char help[] =
    "This plugin will do the logging needed to use HeapDraw";

char wanted_hotkey[] = "Ctrl-Shift-1";

plugin_t PLUGIN = {
  IDP_INTERFACE_VERSION,
  0,                    // plugin flags
  init,                 // initialize
  term,                 // terminate. this pointer may be NULL.
  run,                  // invoke plugin
  comment,              // long comment about the plugin
                        // it could appear in the status line
                        // or as a hint

  help,                 // multiline help about the plugin

  wanted_name,          // the preferred short name of the plugin
  wanted_hotkey         // the preferred hotkey to run the plugin
};

int log(const char *format,...)
{
  va_list va;

  msg("%s: ", wanted_name);

  va_start(va, format);
  int nbytes = vmsg(format, va);
  va_end(va);
  return nbytes;
}

