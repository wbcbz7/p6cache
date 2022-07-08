#pragma once

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

uint32_t CS();
#pragma aux CS = "mov eax, cs" value [eax]

uint64_t rdtsc();
#pragma aux rdtsc = "rdtsc" value [edx eax]

uint32_t cpuid_eax(uint32_t leaf);
#pragma aux cpuid_eax = "cpuid" parm[eax] value [eax]
