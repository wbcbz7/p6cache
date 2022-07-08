/*
    coreboot l2_cache.c heavily hacked by wbcbz7
    original copyrights:
*/

/* SPDX-License-Identifier: GPL-2.0-or-later */

/*
 * Intel Pentium L2 Cache initialization.
 * This code was developed by reverse engineering
 * the BIOS. Where the code accesses documented
 * registers I have added comments as best I can.
 * Some undocumented registers on the Pentium II are
 * used so some of the documentation is incomplete.
 *
 * References:
 * Intel Architecture Software Developer's Manual
 * Volume 3B: System Programming Guide, Part 2 (#253669)
 * Appendix B.9
 */

/* This code is ported from coreboot v1.
 * The L2 cache initialization sequence here only apply to SECC/SECC2 P6 family
 * CPUs with Klamath (63x), Deschutes (65x) and Katmai (67x) cores.
 * It is not required for Coppermine (68x) and Tualatin (6bx) cores.
 * It is currently not known if Celerons with Mendocino (66x) core require the
 * special initialization.
 * Covington-core Celerons do not have L2 cache.
 */

#include <stdio.h>
#include <stdint.h>
#include "lowlevel.h"
#include "l2_cache.h"
#include "cache.h"
#include "msr.h"

/* Latency Tables */
struct latency_entry {
	uint8_t key;
	uint8_t value;
};
/*
Latency maps for Deschutes and Katmai.
No such mapping is available for Klamath.

Cache latency to
be written to L2 -----++++
control register      ||||
0000 xx 00 -----> 000 cccc 0
||||    00 66MHz
||||    10 100MHz
||||    01 133MHz (Katmai "B" only)
++++------ CPU frequency multiplier

0000 2x
0001 3x
0010 4x
0011 5x
0100 2.5x
0101 3.5x
0110 4.5x
0111 5.5x
1000 6x
1001 7x
1010 8x
1011 Reserved
1100 6.5x
1101 7.5x
1110 1.5x
1111 2x

*/
static const struct latency_entry latency_650_t0[] = {
	{0x10, 0x02}, {0x50, 0x02}, {0x20, 0x04}, {0x60, 0x06},
	{0x00, 0x08}, {0x40, 0x0C}, {0x12, 0x06}, {0x52, 0x0A},
	{0x22, 0x0E}, {0x62, 0x10}, {0x02, 0x10}, {0xFF, 0x00}
};

static const struct latency_entry latency_650_t1[] = {
	{0x12, 0x14}, {0x52, 0x16}, {0x22, 0x16}, {0x62, 0x16},
	{0xFF, 0x00}
};

static const struct latency_entry latency_670_t0[] = {
	{0x60, 0x06}, {0x00, 0x08}, {0x12, 0x06}, {0x52, 0x0A},
	{0x22, 0x0E}, {0x62, 0x10}, {0x02, 0x10}, {0x42, 0x02},
	{0x11, 0x0E}, {0x51, 0x0C}, {0x21, 0x02}, {0x61, 0x10},
	{0x01, 0x10}, {0x41, 0x02}, {0xFF, 0x00}
};

static const struct latency_entry latency_670_t1[] = {
	{0x22, 0x18}, {0x62, 0x18}, {0x02, 0x1A}, {0x11, 0x18},
	{0xFF, 0x00}
};

static const struct latency_entry latency_670_t2[] = {
	{0x22, 0x12}, {0x62, 0x14}, {0x02, 0x16}, {0x42, 0x1E},
	{0x11, 0x12}, {0x51, 0x16}, {0x21, 0x1E}, {0x61, 0x14},
	{0x01, 0x16}, {0x41, 0x1E}, {0xFF, 0x00}
};

/* Latency tables for 650 model/type */
static const struct latency_entry *latency_650[] = {
	latency_650_t0, latency_650_t1, latency_650_t1
};

/* Latency tables for 670 model/type */
static const struct latency_entry *latency_670[] = {
	latency_670_t0, latency_670_t1, latency_670_t2
};

int calculate_l2_latency(uint32_t default_latency)
{
	uint32_t eax, signature; uint32_t l = (default_latency == -1) ? 0 : (default_latency << 1) & 0x1e;
	const struct latency_entry *latency_table, *le;
	msr_t msr;

	/* First, attempt to get cache latency value from
	   IA32_PLATFORM_ID[56:53]. (L2 Cache Latency Read)
	 */
	msr.u64 = rdmsr(IA32_PLATFORM_ID);

	printf("rdmsr(IA32_PLATFORM_ID) = %x:%x\n", msr.hi, msr.lo);

    if (default_latency == -1)
	    l = (msr.hi >> 20) & 0x1e;

	if ((default_latency == -1) && (l == 0)) {
		/* If latency value isn't available from
		   IA32_PLATFORM_ID[56:53], read it from
		   L2 control register 0 for lookup from
		   tables. */
		int t, a;

		/* The raw code is read from L2 register 0, bits [7:4]. */
		a = read_l2(0);
		if (a < 0)
			return -1;

		a &= 0xf0;

		if ((a & 0x20) == 0)
			t = 0;
		else if (a == 0x20)
			t = 1;
		else if (a == 0x30)
			t = 2;
		else
			return -1;

		printf("L2 latency type = %x\n", t);

		/* Get CPUID family/model */
		signature = cpuid_eax(1) & 0xfff0;

		/* Read EBL_CR_POWERON */
		msr.u64 = rdmsr(EBL_CR_POWERON);
		/* Get clock multiplier and FSB frequency.
		 * Multiplier is in [25:22].
		 * FSB is in [19:18] in Katmai, [19] in Deschutes ([18] is zero
		 * for them).
		 */
		eax = msr.lo >> 18;
		if (signature == 0x650) {
			eax &= ~0xf2;
			latency_table = latency_650[t];
		} else if (signature == 0x670) {
			eax &= ~0xf3;
			latency_table = latency_670[t];
		} else
			return -1;

		/* Search table for matching entry */
		for (le = latency_table; le->key != eax; le++) {
			/* Fail if we get to the end of the table */
			if (le->key == 0xff) {
				printf("Could not find key %02x in latency table\n",
				   eax);
				return -1;
			}
		}

		l = le->value;
	}

	printf("L2 Cache latency is %d\n", l >> 1);

	/* Writes the calculated latency in BBL_CR_CTL3[4:1]. */
	msr.u64 = rdmsr(BBL_CR_CTL3);
	msr.lo &= 0xffffffe1;
	msr.lo |= l;
	wrmsr(BBL_CR_CTL3, msr.u64);

	return 0;
}

/* Setup address, data_high:data_low into the L2
 * control registers and then issue command with correct cache way
 */
int signal_l2(uint32_t address, uint32_t data_high, uint32_t data_low, int way, uint8_t command)
{
	int i;
	msr_t msr;

	/* Write L2 Address to BBL_CR_ADDR */
	msr.lo = address;
	msr.hi = 0;
	wrmsr(BBL_CR_ADDR, msr.u64);

	/* Write data to BBL_CR_D{0..3} */
	msr.lo = data_low;
	msr.hi = data_high;
	for (i = BBL_CR_D0; i <= BBL_CR_D3; i++)
		wrmsr(i, msr.u64);

	/* Put the command and way into BBL_CR_CTL */
	msr.u64 = rdmsr(BBL_CR_CTL);
	msr.lo = (msr.lo & 0xfffffce0) | command | (way << 8);
	wrmsr(BBL_CR_CTL, msr.u64);

	/* Trigger L2 controller */
	msr.lo = 0;
	msr.hi = 0;
	wrmsr(BBL_CR_TRIG, msr.u64);

	/* Poll the controller to see when done */
	for (i = 0; i < 0x100; i++) {
		/* Read BBL_CR_BUSY */
		msr.u64 = rdmsr(BBL_CR_BUSY);
		/* If not busy then return */
		if ((msr.lo & 1) == 0)
			return 0;
	}

	/* Return timeout code */
	return -1;
}

/* Read the L2 Cache controller register at given address */
int read_l2(uint32_t address)
{
	msr_t msr;

	/* Send a L2 Control Register Read to L2 controller */
	if (signal_l2(address << 5, 0, 0, 0, L2CMD_CR) != 0)
		return -1;

	/* If OK then get the result from BBL_CR_ADDR */
	msr.u64 = rdmsr(BBL_CR_ADDR);
	return (msr.lo >> 0x15);

}

/* Write data into the L2 controller register at address */
int write_l2(uint32_t address, uint32_t data)
{
	int v1, v2, i;

	v1 = read_l2(0);
	if (v1 < 0)
		return -1;

	v2 = read_l2(2);
	if (v2 < 0)
		return -1;

	if ((v1 & 0x20) == 0) {
		v2 &= 0x3;
		v2++;
	} else
		v2 &= 0x7;

	/* This write has to be replicated to a number of places. Not sure what.
	 */

	for (i = 0; i < v2; i++) {

		uint32_t data1, data2;
		// Bits legend
		// data1   = ffffffff
		// data2   = 000000dc
		// address = 00aaaaaa
		// Final address signaled:
		// 000fffff fff000c0 000dcaaa aaa00000
		data1 = data & 0xff;
		data1 = data1 << 21;
		data2 = (i << 11) & 0x1800;
		data1 |= data2;
		data2 <<= 6;
		data2 &= 0x20000;
		data1 |= data2;

		/* Signal L2 controller */
		if (signal_l2((address << 5) | data1, 0, 0, 0, 3))
			return -1;
	}
	return 0;
}

/* Write data_high:data_low into the cache at address1. Test address2
 * to see if the same data is returned. Return 0 if the data matches.
 * return lower 16 bits if mismatched data if mismatch. Return -1
 * on error
 */
int test_l2_address_alias(uint32_t address1, uint32_t address2,
				 uint32_t data_high, uint32_t data_low)
{
	int d;
	msr_t msr;

	/* Tag Write with Data Write for L2 */
	if (signal_l2(address1, data_high, data_low, 0, L2CMD_TWW))
		return -1;

	/* Tag Read with Data Read for L2 */
	if (signal_l2(address2, 0, 0, 0, L2CMD_TRR))
		return -1;

	/* Read data from BBL_CR_D[0-3] */
	for (d = BBL_CR_D0; d <= BBL_CR_D3; d++) {
		msr.u64 = rdmsr(d);
		if (msr.lo != data_low || msr.hi != data_high)
			return (msr.lo & 0xffff);
	}

	return 0;
}

/* Calculates the L2 cache size.
 *
 * Reference: Intel(R) 64 and IA-32 Architectures Software Developer's Manual
 *            Volume 3B: System Programming Guide, Part 2, Intel pub. 253669,
 *            pg. B-172.
 *
 */
int calculate_l2_cache_size(void)
{
	int v;
	msr_t msr;
	uint32_t cache_setting;
	uint32_t address, size, eax, bblcr3;

	v = read_l2(0);
	if (v < 0)
		return -1;
	if ((v & 0x20) == 0) {
		msr.u64 = rdmsr(BBL_CR_CTL3);
		bblcr3 = msr.lo & ~BBLCR3_L2_SIZE;
		/*
		 * Successively write in all the possible cache size per bank
		 * into BBL_CR_CTL3[17:13], starting from 256KB (00001) to 4MB
		 * (10000), and read the last value written and accepted by the
		 * cache.
		 *
		 * No idea why these bits are writable at all.
		 */
		for (cache_setting = BBLCR3_L2_SIZE_256K;
		     cache_setting <= BBLCR3_L2_SIZE_4M; cache_setting <<= 1) {

			eax = bblcr3 | cache_setting;
			msr.lo = eax;
			wrmsr(BBL_CR_CTL3, msr.u64);
			msr.u64 = rdmsr(BBL_CR_CTL3);

			/* Value not accepted */
			if (msr.lo != eax)
				break;
		}

		/* Backtrack to the last value that worked... */
		cache_setting >>= 1;

		/* and write it into BBL_CR_CTL3 */
		msr.lo &= ~BBLCR3_L2_SIZE;
		msr.lo |= (cache_setting & BBLCR3_L2_SIZE);

		wrmsr(BBL_CR_CTL3, msr.u64);

#if 0
		printk(BIOS_DEBUG, "Maximum cache mask is %x\n", cache_setting);
#endif

		/* For now, BBL_CR_CTL3 has the highest cache "size" that
		 * register will accept. Now we'll ping the cache and see where
		 * it wraps.
		 */

		/* Write aaaaaaaa:aaaaaaaa to address 0 in the l2 cache.
		 * If this "alias test" returns an "address", it means the
		 * cache cannot be written to properly, and we have a problem.
		 */
		v = test_l2_address_alias(0, 0, 0xaaaaaaaa, 0xaaaaaaaa);
		if (v != 0)
			return -1;

		/* Start with 32K wrap point (256KB actually) */
		size = 1;
		address = 0x8000;

		while (1) {
			v = test_l2_address_alias(address, 0, 0x55555555,
						  0x55555555);
			// Write failed.
			if (v < 0)
				return -1;
			// It wraps here.
			else if (v == 0)
				break;

			size <<= 1;
			address <<= 1;

			if (address > 0x200000)
				return -1;
		}

		/* Mask size */
		size &= 0x3e;

		/* Shift to [17:13] */
		size <<= 12;

		/* Set this into BBL_CR_CTL3 */
		msr.u64 = rdmsr(BBL_CR_CTL3);
		msr.lo &= ~BBLCR3_L2_SIZE;
		msr.lo |= size;
		wrmsr(BBL_CR_CTL3, msr.u64);

#if 0
		printk(BIOS_DEBUG, "L2 Cache Mask is %x\n", size);
#endif

		/* Shift to [6:2] */
		size >>= 11;

		v = read_l2(2);

		if (v < 0)
			return -1;

#if 0
		printk(BIOS_DEBUG, "L2(2): %x ", v);
#endif

		v &= 0x3;

		/* Shift size right by v */
		size >>= v;

		/* Or in this size */
		v |= size;

#if 0
		printk(BIOS_DEBUG, "-> %x\n", v);
#endif

		if (write_l2(2, v) != 0)
			return -1;
	} else {
		// Some cache size information is available from L2 registers.
		// Work from there.
		int b, c;

		v = read_l2(2);

#if 0
		printk(BIOS_DEBUG, "L2(2) = %x\n", v);
#endif

		if (v < 0)
			return -1;

		// L2 register 2 bitmap: cc---bbb
		b = v & 0x7;
		c = v >> 6;

		v = 1 << c * b;

		v &= 0xf;

#if 0
		printk(BIOS_DEBUG, "Calculated a = %x\n", v);
#endif

		if (v == 0)
			return -1;

		/* Shift to 17:14 */
		v <<= 14;

		/* Write this size into BBL_CR_CTL3 */
		msr.u64 = rdmsr(BBL_CR_CTL3);
		msr.lo &= ~BBLCR3_L2_SIZE;
		msr.lo |= v;
		wrmsr(BBL_CR_CTL3, msr.u64);
	}

	return 0;
}

// L2 physical address range can be found from L2 control register 3,
// bits [2:0].
int calculate_l2_physical_address_range(void)
{
	int r0, r3;
	msr_t msr;

	r3 = read_l2(3);
	if (r3 < 0)
		return -1;

	r0 = read_l2(0);
	if (r0 < 0)
		return -1;

	if (r0 & 0x20)
		r3 = 0x7;
	else
		r3 &= 0x7;

#if 0
	printk(BIOS_DEBUG, "L2 Physical Address Range is %dM\n",
		(1 << r3) * 512);
#endif

	/* Shift into [22:20] to be saved into BBL_CR_CTL3. */
	r3 = r3 << 20;

	msr.u64 = rdmsr(BBL_CR_CTL3);
	msr.lo &= ~BBLCR3_L2_PHYSICAL_RANGE;
	msr.lo |= r3;
	wrmsr(BBL_CR_CTL3, msr.u64);

	return 0;
}

int set_l2_ecc(void)
{
	uint32_t eax;
	const uint32_t data1 = 0xaa55aa55;
	const uint32_t data2 = 0xaaaaaaaa;
	msr_t msr;

	/* Set User Supplied ECC in BBL_CR_CTL */
	msr.u64 = rdmsr(BBL_CR_CTL);
	msr.lo |= BBLCR3_L2_SUPPLIED_ECC;
	wrmsr(BBL_CR_CTL, msr.u64);

	/* Write a value into the L2 Data ECC register BBL_CR_DECC */
	msr.lo = data1;
	msr.hi = 0;
	wrmsr(BBL_CR_DECC, msr.u64);

	if (test_l2_address_alias(0, 0, data2, data2) < 0)
		return -1;

	/* Read back ECC from BBL_CR_DECC */
	msr.u64 = rdmsr(BBL_CR_DECC);
	eax = msr.lo;

	if (eax == data1) {
#if 0
		printk(BIOS_DEBUG, "L2 ECC Checking is enabled\n");
#endif
		/* Set ECC Check Enable in BBL_CR_CTL3 */
		msr.u64 = rdmsr(BBL_CR_CTL3);
		msr.lo |= BBLCR3_L2_ECC_CHECK_ENABLE;
		wrmsr(BBL_CR_CTL3, msr.u64);
	}

	/* Clear User Supplied ECC in BBL_CR_CTL */
	msr.u64 = rdmsr(BBL_CR_CTL);
	msr.lo &= ~BBLCR3_L2_SUPPLIED_ECC;
	wrmsr(BBL_CR_CTL, msr.u64);

	return 0;
}

int disable_l2_ecc(void) {
    msr_t msr;

	// disable ECC if requested
	msr.u64 = rdmsr(BBL_CR_CTL3);
	msr.lo &= ~BBLCR3_L2_ECC_CHECK_ENABLE;
	wrmsr(BBL_CR_CTL3, msr.u64);

    return 0;
}

void enable_cache(void)
{
	CRx_TYPE cr0;
	cr0 = read_cr0();
	cr0 &= ~(CR0_CD | CR0_NW);
	write_cr0(cr0);
}

void disable_cache(void)
{
	/* Disable and write back the cache */
	CRx_TYPE cr0;
	cr0 = read_cr0();
	cr0 |= (CR0_CD | CR0_NW);
	wbinvd();
	write_cr0(cr0);
	wbinvd();
}

/*
 * Disable L2 cache on P6
*/
int p6_disable_l2_cache(void) {
    msr_t bblctl3;
    
    // invalidate cache data
    wbinvd();

    bblctl3.u64 = rdmsr(BBL_CR_CTL3);
	bblctl3.lo &= ~BBLCR3_L2_ENABLED;
    wrmsr(BBL_CR_CTL3, bblctl3.u64);

    // invalidate cache data again
    wbinvd();

    return 0;
}

/*
 * This is the function called from CPU initialization
 * driver to set up P6 family L2 cache.
 */

int p6_configure_l2_cache(int32_t default_latency, int32_t l2_ecc, int32_t l2_force)
{
	msr_t msr, bblctl3;
	unsigned int eax;
	uint16_t signature;
	int cache_size, bank, l1state;
	int result, calc_eax;
	int v, a;

	int badclk1, badclk2, clkratio;
	int crctl3_or;

    // check if L1 cache enabled
    l1state = read_cr0() & CR0_CD;

    // disable P6 L2 cache, just for sure
    p6_disable_l2_cache();

	printf("Configuring L2 cache... ");

	/* Read BBL_CR_CTL3 */
	bblctl3.u64 = rdmsr(BBL_CR_CTL3);
	/* If bit 23 (L2 Hardware disable) is set then done */
	/* These would be Covington core Celerons with no L2 cache */
	if ((l2_force != 1) && (bblctl3.lo & BBLCR3_L2_NOT_PRESENT)) {
		printf("hardware disabled\n");
		return 0;
	}

    // if L2 is configured already, l2_force is not 1 and latency is not overriden,
    // skip configuration and straight enable the cache
    if ((l2_force != 1) && (default_latency == -1) && (bblctl3.lo & BBLCR3_L2_CONFIGURED)) {
        printf("already configured... \n");
        // set L2 ECC state if requested
        if (l2_ecc != -1) (l2_ecc == 0 ? disable_l2_ecc() : set_l2_ecc());
        goto enable_l2;
    }

	signature = cpuid_eax(1) & 0xfff0;

	/* Klamath-specific bit settings for certain
	   preliminary checks.
	 */
	if (signature == 0x630) {
		clkratio = 0x1c00000;
		badclk2 = 0x1000000;
		crctl3_or = 0x44000;
	} else {
		clkratio = 0x3c00000;
		badclk2 = 0x3000000;
		crctl3_or = 0x40000;
	}
	badclk1 = 0xc00000;

	/* Read EBL_CR_POWERON */
	msr.u64 = rdmsr(EBL_CR_POWERON);
	eax = msr.lo;
	/* Mask out [22-25] Clock frequency ratio */
	eax &= clkratio;
#if 0
	if (eax == badclk1 || eax == badclk2) {
		printf("Incorrect clock frequency ratio %x\n", eax);
		return -1;
	}
#endif
    /* Now, disable L1 cache */
	disable_cache();

	/* Mask out from BBL_CR_CTL3:
	 * [0] L2 Configured
	 * [5] ECC Check Enable
	 * [6] Address Parity Check Enable
	 * [7] CRTN Parity Check Enable
	 * [8] L2 Enabled
	 * [12:11] Number of L2 banks
	 * [17:13] Cache size per bank
	 * [18] (Set below)
	 * [22:20] L2 Physical Address Range Support
	 */
	bblctl3.lo &= 0xff88061e;
	/* Set:
	 * [17:13] = 00010 = 512Kbyte Cache size per bank (63x)
	 * [17:13] = 00000 = 128Kbyte Cache size per bank (all others)
	 * [18] Cache state error checking enable
	 */
	bblctl3.lo |= crctl3_or;

	/* Write BBL_CR_CTL3 */
	wrmsr(BBL_CR_CTL3, bblctl3.u64);

	if (signature != 0x630) {
		eax = bblctl3.lo;

		/* Set the l2 latency in BBL_CR_CTL3 */
		if (calculate_l2_latency(default_latency) != 0)
			goto bad;

		/* Read the new latency values back */
		bblctl3.u64 = rdmsr(BBL_CR_CTL3);
		calc_eax = bblctl3.lo;

		/* Write back the original default value */
		bblctl3.lo = eax;
		wrmsr(BBL_CR_CTL3, bblctl3.u64);

		/* Write BBL_CR_CTL3[27:26] (reserved??) to bits [1:0] of L2
		 * register 4.  Apparently all other bits must be preserved,
		 * hence these code.
		 */

		v = (calc_eax >> 26) & 0x3;
		printf("write_l2(4, %x)\n", v);

		a = read_l2(4);
		if (a >= 0) {
			a &= 0xfffc;
			a |= v;
			a = write_l2(4, a);
			/* a now contains result code from write_l2() */
		}
		if (a != 0)
			goto bad;

		/* Restore the correct latency value into BBL_CR_CTL3 */
		bblctl3.lo = calc_eax;
		wrmsr(BBL_CR_CTL3, bblctl3.u64);
	} /* ! 63x CPU */

	/* Read L2 register 0 */
	v = read_l2(0);

	/* If L2(0)[5] set (and can be read properly), enable CRTN and address
	 * parity
	 */
	if (v >= 0 && (v & 0x20)) {
		bblctl3.u64 = rdmsr(BBL_CR_CTL3);
		bblctl3.lo |= (BBLCR3_L2_ADDR_PARITY_ENABLE |
			       BBLCR3_L2_CRTN_PARITY_ENABLE);
		wrmsr(BBL_CR_CTL3, bblctl3.u64);
	}

	/* If something goes wrong at L2 ECC setup, cache ECC
	 * will just remain disabled.
	 */
	if (l2_ecc != 0) set_l2_ecc();

	if (calculate_l2_physical_address_range() != 0) {
		printf("Failed to calculate L2 physical address range\n");
		goto bad;
	}

	if (calculate_l2_cache_size() != 0) {
		printf("Failed to calculate L2 cache size\n");
		goto bad;
	}

	/* Turn on cache. Only L1 is active at this time. */
	enable_cache();

enable_l2:
	/* Get the calculated cache size from BBL_CR_CTL3[17:13] */
	bblctl3.u64 = rdmsr(BBL_CR_CTL3);
	cache_size = (bblctl3.lo & BBLCR3_L2_SIZE);
	if (cache_size == 0)
		cache_size = 0x1000;
	cache_size = cache_size << 3;

	/* TODO: Cache size above is per bank. We're supposed to get
	 * the number of banks from BBL_CR_CTL3[12:11].
	 * Confirm that this still provides the correct answer.
	 */
	bank = ((bblctl3.lo >> 11) & 0x3) + 1;
	printf("size %dK... ", cache_size * bank * 4 / 1024);

	/* Write to all cache lines to initialize */

	while (cache_size > 0) {

		/* Each cache line is 32 bytes. */
		cache_size -= 32;

		/* Update each way */

		/* We're supposed to get L2 associativity from
		 * BBL_CR_CTL3[10:9].  But this code only applies to certain
		 * members of the P6 processor family and since all P6
		 * processors have 4-way L2 cache, we can safely assume
		 * 4 way for all cache operations.
		 */

		for (v = 0; v < 4; v++) {
			/* Send Tag Write w/Data Write (TWW) to L2 controller
			 * MESI = Invalid
			 */
			if (signal_l2(cache_size, 0, 0, v, L2CMD_TWW
				| L2CMD_MESI_I) != 0) {
				printf("Failed on signal_l2(%x, %x)\n",
				       cache_size, v);
				goto bad;
			}
		}
	}
	printf("L2 Cache lines initialized\n");

	/* Disable cache */
	disable_cache();

	/* Set L2 cache configured in BBL_CR_CTL3 */
	bblctl3.u64 = rdmsr(BBL_CR_CTL3);
	bblctl3.lo |= BBLCR3_L2_CONFIGURED;
	wrmsr(BBL_CR_CTL3, bblctl3.u64);

	/* Invalidate cache and discard unsaved writes */
	_asm invd;

	/* Write 0 to L2 control register 5 */
	if (write_l2(5, 0) != 0) {
		printf("write_l2(5, 0) failed\n");
		goto done;
	}

	bblctl3.u64 = rdmsr(BBL_CR_CTL3);
	if (signature == 0x650) {
		/* Change the L2 latency to 0101 then back to
		 * original value. I don't know why this is needed - dpd
		 */
		eax = bblctl3.lo;
		bblctl3.lo &= ~BBLCR3_L2_LATENCY;
		bblctl3.lo |= 0x0a;
		wrmsr(BBL_CR_CTL3, bblctl3.u64);
		bblctl3.lo = eax;
		wrmsr(BBL_CR_CTL3, bblctl3.u64);
	}

	/* Enable L2 in BBL_CR_CTL3 */
	bblctl3.lo |= BBLCR3_L2_ENABLED;
	wrmsr(BBL_CR_CTL3, bblctl3.u64);

	/* Turn on cache. Both L1 and L2 are now active. Wahoo! */
    if (l1state == 0) enable_cache();
done:
	result = 0;
	goto out;
bad:
	result = -1;
out:
	printf("done.\n");
	return result;
    
    // actually, only L2 is active for now :)
}
