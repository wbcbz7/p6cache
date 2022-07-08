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

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "mycpuid.h"
#include "lowlevel.h"

extern "C" {
#include "coreboot\l2_cache.h"
#include "coreboot\cr.h"
#include "coreboot\cache.h"
#include "coreboot\msr.h"
}

// current state
struct params {
    int32_t getInfo;           // -1/0 - disable, 1 - enable
    int32_t showHelp;          // -1/0 - disable, 1 - enable
    int32_t level1_state;      // -1 - don't care, 0 - disable, 1 - enable
    int32_t level2_state;      // -1 - don't care, 0 - disable, 1 - enable, do common init
    int32_t l2_latency;        // -1 - don't care, else desired L2 latency
    int32_t ring3_force;       // -1/0 - abort if ring3, 1 - try anyway
    int32_t l2_size;           // -1 - don't care, else desired L2 size in kbytes
    int32_t l2_ecc;            // -1 - don't care, 0 - disable, 1 - enable
    int32_t l2_force;          // -1 - don't care, 0 - disable, 1 - enable

    params() {
        memset(this, -1, sizeof(params));
    }
} params;

// ------------------------
// CPUID info
cpuid_t cpuidInfo;

// ------------------------
// L2 cache MSRs
msr_t bblcr3;

// ------------------------------
// command line options stuff

enum {
    CMD_FLAG_NONE,
    CMD_FLAG_BOOL,
    CMD_FLAG_INT,
};

#define arrayof(n) (sizeof(n) / sizeof(n[0]))
struct cmdline {
    char        shortname;
    char        flags;
    char*       longname;
    int32_t*    parmPtr;
};

const cmdline cmdparams[] = {
    {'I',   CMD_FLAG_NONE,  "INFO",     &params.getInfo},
    {'1',   CMD_FLAG_BOOL,  "L1",       &params.level1_state},
    {'2',   CMD_FLAG_BOOL,  "L2",       &params.level2_state},
    {'L',   CMD_FLAG_INT,   "LATENCY",  &params.l2_latency},
    {'E',   CMD_FLAG_BOOL,  "ECC",      &params.l2_ecc},
    {'R',   CMD_FLAG_NONE,  "RING3",    &params.ring3_force},
    {'H',   CMD_FLAG_NONE,  "HELP",     &params.showHelp},
    {'F',   CMD_FLAG_NONE,  "FORCE",    &params.l2_force},
    {'?',   CMD_FLAG_NONE,   NULL,      &params.showHelp},
    {0}
};

uint32_t parse_cmdline(int argc, char *argv[], const cmdline *params, uint32_t paramCount) {
    char parm[32];
    for (int i = 1; i < argc; i++) {
        // copy to temp buffer and uppercase it
        strncpy(parm, argv[i], sizeof(parm)); strupr(parm);
        char *p = parm;

        // strip separators
        while ((*p == '-') || (*p == '/')) p++;

        // get best matching option
        const cmdline* cmd = params; char* nextp = p;
        for (int j = 0; j < paramCount - 1; j++, cmd++) {
            if ((cmd->longname != NULL) && (strstr(p, cmd->longname) != NULL)) {
                p += strlen(cmd->longname);
                break;
            }
            if (*p == cmd->shortname) {
                p++; break;
            }
        }
        if (cmd->parmPtr == NULL) {
            printf("error: unknown parameter: %s\n", parm);
            return 1;
        }

        if (cmd->flags == CMD_FLAG_NONE) {
            *cmd->parmPtr = 1;
            continue;       // get next param
        }

        if (*p == 0) {
            if (cmd->flags == CMD_FLAG_BOOL) {
                *cmd->parmPtr = 1;
                continue;       // get next param
            } else {
                // oops - end of string
                i++; if (i == argc) {
                    printf("error: incorrect parameter: %s\n", parm);
                    return 1;
                }
                strncpy(parm, argv[i], sizeof(parm)); strupr(parm);
                p = parm;
            }
        }
        // else skip more separators
        while ((*p == '=') || (*p == ':')) p++;

        // finally extract value
        switch(cmd->flags) {
            case CMD_FLAG_BOOL: 
                if (strstr(p, "ON") || strstr(p, "1")) *cmd->parmPtr = 1;
                else if (strstr(p, "OFF") || strstr(p, "0")) *cmd->parmPtr = 0;
                else {
                    printf("error: incorrect parameter: %s\n", p);
                    return 1;
                }
                break;
            case CMD_FLAG_INT: 
                if (sscanf(p, "%d", cmd->parmPtr) != 1) {
                    printf("error: incorrect parameter: %s\n", p);
                    return 1;
                }
            default:    break;
        }
    }

    return 0;
}

// ------------------------------

uint32_t log2i(uint32_t i) {
    uint32_t l = -1;
    while (i != 0) {l++; i>>=1;}
    return l;
}

uint32_t getCpuid(cpuid_t & cpuidInfo) {
    cpuidget(&cpuidInfo);
    printf("CPU family %d, model %u, stepping %u, CPUID %s supported\n", cpuidInfo.family, cpuidInfo.model, cpuidInfo.stepping, cpuidInfo.supported ? "is" : "not");
    printf("CPU vendor %s, extended family %d, extended model %u\n", cpuidInfo.str, cpuidInfo.extfamily, cpuidInfo.extmodel);

    // do sanity checks
    if (strstr(cpuidInfo.str, "GenuineIntel") == NULL) {printf("error: vendor is not Intel\n"); return 1;}
    if ((cpuidInfo.family != 6) || (cpuidInfo.extmodel != 0) || (cpuidInfo.model > 0xB)) {
        printf("error: unsupported CPU family/model!\n");
        return 1;
    }
    return 0;
}

uint32_t getCacheInfo(msr_t &bblcr3) {
    // retrieve cache info!
    uint32_t cr0 = read_cr0();
    printf("L1 cache: %s, %s, CR0=%08X\n",
        (cr0 & CR0_CD) ? "disabled" : "enabled", (cr0 & CR0_NW) ? "not write-through" : "write-through",
        cr0);

    // get L2 cache info (the tricky part)
    bblcr3.u64 = rdmsr(BBL_CR_CTL3);
    printf("L2 cache: %s, %spresent, %sconfigured, %d banks %d KB each, latency %d\n",
        (bblcr3.lo & BBLCR3_L2_ENABLED) ? "enabled" : "disabled",
        (bblcr3.lo & BBLCR3_L2_HARDWARE_DISABLE) ? "not " : "",
        (bblcr3.lo & BBLCR3_L2_CONFIGURED) ? "" : "not ",
        ((bblcr3.lo >> 11) & 3) + 1,
        (1 << (8 + log2i((bblcr3.lo >> 13) & 0x1F))),
        (bblcr3.lo >> 1) & 0xF
    );
    const char *assoc[] = {"direct", "2-way", "4-way", "8-way"};
    printf("          %d MB cacheable, %s, %s speed, %s, BBL_CR_CTL3=%08X\n",
        (1 << (9 + ((bblcr3.lo >> 20) & 7))),
        (bblcr3.lo & BBLCR3_L2_ECC_CHECK_ENABLE ? "ECC" : "no ECC"),
        (bblcr3.lo & (1 << 25)) ? "half" : "full",
        assoc[(bblcr3.lo >> 9) & 3],
        bblcr3.lo
    );

    return 0;
}

void showHelp() {
    printf("\n");
    printf("command line:\n");
    printf("-2, --l2=[on/off]   - turn on/off L2 cache (if available)\n");
    printf("-e, --ecc=[on/off]  - turn on/off L2 ECC checking\n");
    printf("-l, --latency=[num] - override L2 latency to (num)\n");
    printf("-f, --force         - force L2 cache reconfiguration\n");
    printf("-r, --ring3         - ignore warnings if CPU is not in ring0\n");
}

int main(int argc, char *argv[]) {
    printf("Pentium II/III/Celeron L2 Cache management utility v0.01 -- wbcbz7 o9.o7.2022\n");
    printf("portions from coreboot project (c) whatever-2022\n");
    printf("--------------------------------------------\n");

    // get cmdline parameters
    if (parse_cmdline(argc, argv, cmdparams, arrayof(cmdparams)) != 0) return 1;

    // print help if requested
    if (params.showHelp == 1) {showHelp(); return 0;}

    // get info about CPU
    if (getCpuid(cpuidInfo) != 0) return 1;

    // check if we're in ring0
    if ((params.ring3_force != 1) && ((CS() & 3) != 0)) {
        printf("error: CPU is not in ring 0 - unable to access system registers\n");
        return 1;
    }

    // get cache info
    if (getCacheInfo(bblcr3) != 0) return 1;

    // disabled since DOS extenders restore CR0 at exit
#if 0
    // fiddle with L1 cache
    if (params.level1_state == 0) {
        disable_cache();
        printf("L1 cache disabled.\n");
    }
#endif

    // check ECC stuff
    if (params.l2_ecc != -1) {
        if (params.l2_ecc == 0) {
            disable_l2_ecc();
            printf("L2 ECC disabled.\n");
        } else {
            // ECC enable needs restarting L2 cache
            params.level2_state = 1;                // request restarting L2
        }
    }

    if (params.l2_latency != -1) {
        // L2 latency override needs restarting L2 cache
        params.level2_state = 1;
    }

    // request if L2 cache needs to be enabled
    if (params.level2_state != -1) {
        if (params.level2_state == 0) {
            if (p6_disable_l2_cache() == 0)
                printf("L2 cache disabled.\n");
        } else {
            if (p6_configure_l2_cache(params.l2_latency, params.l2_ecc, params.l2_force) == 0)
                printf("L2 cache enabled.\n");
        }
    }
    
    // get cache info again
    if ((params.level1_state != -1) || (params.level2_state != -1))
        if (getCacheInfo(bblcr3) != 0) return 1;

    return 0;
}