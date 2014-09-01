#include "HeapChunk.hpp"

unsigned int __t = 0;

bool operator<(const HeapChunk &a, const HeapChunk &b) {
    return ((a.base < b.base) ||
           ((a.base == b.base) && (a.allocTime < b.allocTime)));
}
