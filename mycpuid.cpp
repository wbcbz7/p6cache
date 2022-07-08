/*
    P6Cache - Pentium II/III/Celeron L2 Cache management utility
    --wbcbz7 o9.o7.2o22
    
    p6cache.cpp, mycpuid.cpp/mycpuid.h and lowlevel.h are licensed under following terms:

    Copyright (c) 2022 Artem Vasilev - wbcbz7

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
    SOFTWARE.

    l2_cache.c and all files from "coreboot" folder are derived from coreboot project
    and licensed under GNU GPL 2.0 terms
*/

#include <stdint.h>
#include <string.h>

#include "mycpuid.h"

#if defined(__DJGPP__) 
// include GCC header
#include <cpuid.h>
#endif

uint32_t cpuid(uint32_t leaf,  uint32_t *_eax, uint32_t *_ebx, uint32_t *_ecx, uint32_t *_edx)
#if defined(__WATCOMC__)                     
{
    uint32_t a, b, c, d;
    
    _asm {
        mov     eax, [leaf]
        cpuid
        
        mov     [a], eax
        mov     [b], ebx
        mov     [c], ecx
        mov     [d], edx
    }
    
    *_eax = a; *_ebx = b; *_ecx = c; *_edx = d;
    
    return a;
}
#else if defined(__DJGPP__) 
{
    // use GCC built-in
    __get_cpuid(leaf, _eax, _ebx, _ecx, _edx);
    return _eax;
}
#endif


#if defined(__WATCOMC__)  
uint32_t eflagsTest(uint32_t mask);
#pragma aux eflagsTest = \
        "pushfd" \
        "pushfd" \
        "xor     dword ptr [esp],ebx" \
        "popfd" \
        "pushfd" \
        "pop     eax" \
        "xor     eax, dword ptr [esp]" \
        "popfd" \
        "and     eax, ebx" \
        parm [ebx] value [eax] modify [eax ebx]
        
#else if defined(__DJGPP__)
inline uint32_t eflagsTest(uint32_t mask) {
    uint32_t _eax;
    asm volatile(
        "pushfl" "\n\t"
        "pushfl" "\n\t"
        "xorl    %1, (%%esp)"  "\n\t"
        "popfl" "\n\t"
        "pushfl" "\n\t"
        "popl    %%0" "\n\t"
        "xorl    (%%esp), %%0" "\n\t"
        "popfl" "\n\t"
        "andl     %1, %%0" "\n\t"
        : "=a" (_eax)
        : "b" (mask)
        );
    return _eax;
}
#endif
inline uint32_t check486()    { return eflagsTest(0x00040000); };
inline uint32_t checkCpuid()  { return eflagsTest(0x00200000); };

// returns processor family
uint32_t cpuidget(cpuid_t *p) {
    uint32_t _eax, _ebx, _ecx, _edx;
    
    if (p == NULL) return 0;
    
    // clear p struct
    memset(p, 0, sizeof(cpuid_t));
    
    // check for 486
    // no need for 386 check, dos extender does it by default
    if (!check486()) p->family = 3; else p->family = 4;
    
    // check for cpuid availability
    if (!checkCpuid()) return p->family;
    
    // cpuid is supported
    p->supported = 1;
    
    // get id string
    cpuid(0, &_eax, &_ebx, &_ecx, &_edx);

    p->highestLeaf         = _eax;
    *(uint32_t*)&p->str[0] = _ebx;
    *(uint32_t*)&p->str[4] = _edx;
    *(uint32_t*)&p->str[8] = _ecx;
    
    // get additional info
    cpuid(1, &_eax, &_ebx, &_ecx, &_edx);
    
    p->family   = (_eax >> 8) & 0xF;
    p->model    = (_eax >> 4) & 0xF;
    p->stepping = (_eax >> 0) & 0xF;
    
    p->flags    = (_edx);
    p->extflags = (_ecx);
    
    return (_eax >> 8) & 0xF;
}

