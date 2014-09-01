#ifndef __HOOK_HPP__
#define __HOOK_HPP__

#include <map>

using namespace std;

inline int log(const char *format,...);

typedef enum {
    EVT_FUNCTION_ENTER,
    EVT_FUNCTION_EXIT,
} hook_evt_t;

class Hook {
public:
    char *name;
    bool (*handler)(Hook &hook, ea_t ea, hook_evt_t event);
    ulong args[10];

    bool hook();
    void unhook();
    bool hook_ret(ea_t addr);
    void unhook_ret(ea_t addr);

    bool read_args(int n);

    bool handle(ea_t ea);

    ea_t bpt_addr;
	map<ea_t, int> ret_addrs;

};

bool Hook::hook() {

    bpt_addr = get_debug_name_ea(name);

    if (bpt_addr == BADADDR) {
        // log("WARNING: Couldn't resolve address for %s\n", name);
    } else {
        if (!add_bpt(bpt_addr))
            ;// log("WARNING: Couldn't set breakpoint for %s at 0x%08x\n", name, bpt_addr);
        else {
            bpt_t bpt;
            get_bpt(bpt_addr, &bpt);

            bpt.flags = 0;
            update_bpt(&bpt);

            log("Breakpoint set for %s at 0x%08x\n", name, bpt_addr);
            return true;
        }
    }
    return false;
}

bool Hook::hook_ret(ea_t addr) {
	if (ret_addrs.find(addr) == ret_addrs.end())
		ret_addrs[addr] = 1;
	else {
		// log("Setting %d bpt at %08x\n", ret_addrs[addr], addr);
		ret_addrs[addr]++;
		return true;
	}

    if (!add_bpt(addr))
        ;// log("WARNING: Couldn't set breakpoint for %s's return at 0x%08x\n", name, addr);
    else {
        bpt_t bpt;
        get_bpt(addr, &bpt);

        bpt.flags = 0;
        update_bpt(&bpt);

        // log("Breakpoint set for %s's return at 0x%08x\n", name, addr);
        return true;
    }
    return false;
}

void Hook::unhook() {
    del_bpt(bpt_addr);
    log("Breakpoint removed for %s at 0x%08x\n", name, bpt_addr);
} 

void Hook::unhook_ret(ea_t addr) {
	if (--ret_addrs[addr] == 0) {
		ret_addrs.erase(addr);
	    del_bpt(addr);
	}
}

bool Hook::handle(ea_t ea) {
    if (ea == bpt_addr) return handler(*this, ea, EVT_FUNCTION_ENTER);
    if (ret_addrs.find(ea) != ret_addrs.end()) return handler(*this, ea, EVT_FUNCTION_EXIT);
    return false;
}

bool Hook::read_args(int n) {
    ulonglong esp;

    if (n > 10) return false;

    if (!get_reg_val("ESP", &esp)) return false;

    invalidate_dbgmem_contents(esp,4*n+4);

    for (;n+1;n--)
        args[n] = get_long(esp+n*4);
    return true;
}

#endif
