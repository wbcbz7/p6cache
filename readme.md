# P6Cache - Pentium II/III/Celeron L2 Cache management utility

This MS-DOS utility, based on Coreboot project sources, allows you to configure and tweak P6-family CPU Level 2 cache settings, such as turning L2 off/on, reconfiguring it in case it's disabled by BIOS, adjusting L2 latency or toggling L2 ECC option.

One of distinct features of P6Cache is the ability of enabling L2 cache on certain Pentium II CPUs while running on low multipliers (like x1.5 or x2). Standard BIOS code treats such multipliers as "fail-safe" mode, and omits necessary info in L2 initialization table, so if CPU is running on these multipliers, the L2 cache is left non-configured and thus disabled. P6Cache has it's own L2 init code, effectively "kickstarting" the cache and pulling the CPU from Celeron Covington mode :)



## System requirements:

* MS-DOS 3.x or higher (idk, at least anything Watcom C++ apps could run on). P6Cache will run under VCPI clients such as EMM386 or QEMM, but not under Win9x/NT DOS box, as it requires Ring0 access.
* A supported CPU (Intel Pentium II Klamath/Deschutes, Pentium III Katmai or Celeron Mendocino). PPro, Coppermine and Tualatin CPUs are also supported, but only L2 on/off and possibly ECC option are effective. Celeron Covington is supported as well, however as it lacks L2 cache, this tool is practically useless :)

## Usage

By default, P6Cache shows current L1 and L2 cache status, example:

```
Pentium II/III/Celeron L2 Cache management utility v0.01 -- wbcbz7 o9.o7.2022
portions from coreboot project (c) whatever-2022
--------------------------------------------
CPU family 6, model 5, stepping 0, CPUID is supported
CPU vendor GenuineIntel, extended family 0, extended model 0
L1 cache: enabled, write-through, CR0=00000011
L2 cache: enabled, present, not configured, 1 banks 256 KB each, latency 5
          512 MB cacheable, no ECC, full speed, 4-way, BBL_CR_CTL3=0100250A
```

Besides CPUID info (vendor string, family/model/stepping), current L1/L2 status is displayed, like cache configuration, current latency, cacheable memory range, as well as CR0 and BBL_CR_CTL3 MSR registers content. In this case, the CPU is Pentium II Deschutes running as 66x2.0, with L2 cache not configured by BIOS (hence the bogus L2 config values) 

Available command line switches are:

* `-2, --l2=[on/off]`  - turn on/off L2 cache, if available.
* `-e, --ecc=[on/off]` - turn on/off L2 ECC checking.
* `-l, --latency=[num]` - override L2 latency to [num], valid range is 0..15, and reconfigure L2 cache. Note that not all latency values are valid; if CPU's cache controller does not support particular latency setting, the L2 cache would not initialize and will left disabled. In that case, try increase or decrease latency setting and restart P6Cache again.
* `-f, --force` - force L2 cache reconfiguration; useful if CPU reports no cache, but it's physically available (be sure to check CPU cartridge' traces are not scratched or strap resistors broken!)
* `-r, --ring3` - ignore warnings if CPU is not in ring 0. Relies on PM supervisor allowing to access MSR registers under ring 3 (which is almost always not the case, although you may try this switch anyway)

For example, `p6cache --l2=on --latency=6`, on said PII yields following output:

```
Pentium II/III/Celeron L2 Cache management utility v0.01 -- wbcbz7 o9.o7.2022
portions from coreboot project (c) whatever-2022
--------------------------------------------
CPU family 6, model 5, stepping 0, CPUID is supported
CPU vendor GenuineIntel, extended family 0, extended model 0
L1 cache: enabled, write-through, CR0=00000011
L2 cache: enabled, present, not configured, 1 banks 256 KB each, latency 5
          512 MB cacheable, no ECC, full speed, 4-way, BBL_CR_CTL3=0100250A
Configuring L2 cache... rdmsr(IA32_PLATFORM_ID) = 8001ef2:901dd142
L2 Cache latency is 6
write_l2(4, 0)
size 512K... L2 Cache lines initialized
done.
L2 cache enabled.
L1 cache: enabled, write-through, CR0=00000011
L2 cache: enabled, present, configured, 1 banks 512 KB each, latency 6
          4096 MB cacheable, ECC, full speed, 4-way, BBL_CR_CTL3=0134452D
```



## Default CPU L2 Cache Latency values

These values are extracted from BIOS and coreboot sources, with an additional guess. Consider using it as a reference in case system locks up during L2 auto-configuration.

| CPU core        | cache type | clock, MHz | cache speed, ns | cache latency |
| --------------- | ---------- | ---------- | --------------- | ------------- |
| Deschutes (65x) | BSRAM      | 200        | 10              | 1             |
| "               | "          | 233        | 8               | 1             |
| "               | "          | 266        | 7,5             | 2             |
| "               | "          | 300        | 6,6             | 3             |
| "               | "          | 333        | 6               | 4             |
| "               | "          | 350        | 5,5             | 5             |
| "               | "          | 366        | 5               | 6             |
| "               | "          | 400        | 5               | 7             |
| "               | "          | 450        | 4,4             | 8             |
| "               | "          | 500        | 4               | 8             |
| "               | CSRAM      | 300        |                 | 10            |
| "               | "          | 350        |                 | 11            |
| "               | "          | 400        |                 | 11            |
| "               | "          | 450        |                 | 11            |
| Katmai (67x)    | BSRAM      | 300        | 6,6             | 3             |
| "               | "          | 333        | 6               | 4             |
| "               | "          | 350        | 5               | 5             |
| "               | "          | 400        | 5               | 7             |
| "               | "          | 450        | 4,4             | 8             |
| "               | "          | 466        | 4               | 6             |
| "               | "          | 500        | 4               | 8             |
| "               | "          | 533        | 3,6             | 2             |
| "               | "          | 550        | 3,6             | 2             |
| "               | "          | 600        | 3               | 8             |
| "               | "          | 667        | 3               | 8             |
| "               | "          | 733        | 2,8             | 2             |
| "               | C6 CSRAM   | 400        |                 | 12            |
| "               | "          | 450        |                 | 12            |
| "               | "          | 500        |                 | 13            |
| "               | CK1(?)     | 400        |                 | 9             |
| "               | "          | 450        |                 | 10            |
| "               | "          | 466        |                 | 11            |
| "               | "          | 500        |                 | 11            |
| "               | "          | 533        |                 | 15            |
| "               | "          | 550        |                 | 15            |
| "               | "          | 600        |                 | 10            |
| "               | "          | 667        |                 | 11            |
| "               | "          | 733        |                 | 15            |



# Building

Use Open Watcom C++ 1.9 or higher version. Run `wmake` in P6Cache sources folder, done :)



# License

p6cache.cpp, mycpuid.cpp/mycpuid.h and lowlevel.h are licensed under following terms of MIT license:

```
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
```

l2_cache.c and all files from "coreboot" folder are derived from coreboot project and licensed under GNU GPL 2.0 terms