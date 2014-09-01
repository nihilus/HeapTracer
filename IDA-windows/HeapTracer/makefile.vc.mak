PROC=HeapTracer
O1=Draw
O2=Heap
O3=HeapChunk

!include ..\plugin.vc.mak

# LDFLAGS=$(LDFLAGS) -Lglut

# MAKEDEP dependency list ------------------
$(F)HeapTracer$(O) : $(I)area.hpp $(I)bytes.hpp $(I)expr.hpp $(I)fpro.h        \
	          $(I)funcs.hpp $(I)help.h $(I)ida.hpp $(I)idp.hpp          \
	          $(I)kernwin.hpp $(I)lines.hpp $(I)llong.hpp               \
	          $(I)loader.hpp $(I)nalt.hpp $(I)netnode.hpp $(I)pro.h     \
	          $(I)segment.hpp $(I)ua.hpp $(I)xref.hpp \
			  HeapTracer.cpp Heap.hpp HeapChunk.hpp Hook.hpp

$(F)Draw$(O) : Draw.cpp Draw.hpp Heap.hpp HeapChunk.hpp
$(F)Heap$(O) : Heap.cpp Heap.hpp HeapChunk.hpp
$(F)HeapChunk$(O) : HeapChunk.cpp HeapChunk.hpp
