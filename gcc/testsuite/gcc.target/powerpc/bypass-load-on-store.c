/* { dg-do compile { target { powerpc*-*-* } } } */
/* { dg-options "-O0 -fschedule-insns -fbypass-load-on-store -fdump-rtl-sched1 -fsched-verbose=2" } */

void nescaf(void)
{
	long a, b, c, d,
             e, f, g, h,
	     i, j, k, l,
             m, n, o, p,
             q, r, s, t,

	     z, w;

	a = 41; b = 79; c = 20; d = 11;
	e = 13; f = 43; g = 13; h = 21;
	i = 12; j = 45; k = 55; l = 90;
	m = 23; n = 61; o = 89; p = 53;
	q = 83; r = 52; s = 76; t = 99;

	/* Now, we have a store followed by a load. The assignments to a-t are
	 * all independent of the store-load computation below. The question is:
	 * Under -fschedule-insns -fbypass-load-on-store, are 14 of the above
	 * instructions moved between the store-load?
	 */
	z = 121;
	w = z;
}

/* Note: There is going to be exactly one insn that will be assigned cost 15.
 *       Since its insn-number will likely change, we do not include the insn
 *       number in the scan - just the part of the dump that'll be invariant.
 */
/* { dg-final { scan-rtl-dump "into queue with cost=15" "sched1" { target powerpc*-*-* } } } */
/* { dg-final { cleanup-rtl-dump "sched1" } } */
