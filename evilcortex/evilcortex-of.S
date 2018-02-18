#define PL310_BASE		0xf0002000
#define EVILCONSOLE_ADDR	0x00000018

// #define USE_EVILCONSOLE

.org 0x0
start:
	b init_system

kernel_addr:	.long	0x00208000
dtb_addr:	.long	0x00f00000	/* BUG?! */
machtype:	.long 	4121
secondary_spin:	.long	0x00000000
arm11_busyloop:	b arm11_busyloop
ec_data:	.long	0x00000000

init_system:
	dsb
	isb
    mrc     p15, 0, r0, c1, c0, 0   @ Read SCTLR
    bic     r0, r0, #0x01           @ Clear the M bit
    mcr     p15, 0, r0, c1, c0, 0   @ Write SCTLR
    dsb
    isb

	@ Our L1 cache is probably dirty, coming out of reset
	bl	v7_invalidate_l1

	mcr     p15, 0, r0, c7, c1, 0		@ Invalidate I-cache and BTB
	dsb
	isb

	@ Initialize ACTLR
	mrc 	p15, 0, r0, c1, c0, 1		@ ACTLR
	orr	r0, #0x41			@ Enable SMP bit and Cache/TLB broadcast operations
	mcr 	p15, 0, r0, c1, c0, 1		@ ACTLR

	@ Which core are we?
	mrc	p15, 0, r0, c0, c0, 5		@ MPIDR
	and	r0, #0x03
	cmp	r0, #0
	bne	spin_loop	@ CPU1 goes into spin loop, until we're ready for it

	@ The rest of this code is executed by CPU0 only

	mov	r0, #'H'
	bl	putc

	@ Sigh.......
#	bl 	init_frickin_pl310_cache

	mov	r0, #'E'
	bl	putc


	@ Jump to kernel
	mov	r0, #0
	mov	r1, #0xffffffff
	ldr	r2, dtb_addr
	ldr	pc, kernel_addr
end:	b end



putc:
#ifdef USE_EVILCONSOLE
	ldr	r1, =EVILCONSOLE_ADDR
	str	r0, [r1]
1:	ldr	r0, [r1]
	cmp	r0, #0
	bne	1b
#endif
	mov	pc, lr


spin_loop:
	ldr	r0, secondary_spin
	cmp	r0, #0
	beq	spin_loop
	mov	r1, #0
	str	r1, secondary_spin
	mov	pc, r0


init_frickin_pl310_cache:
	ldr	r0, =PL310_BASE

	ldr	r1, =0x00000000
	str	r1, [r0, #0x100]	@ CTRL

	ldr	r1, =0x32050000
	str	r1, [r0, #0x104]	@ AUX_CTRL

	ldr	r1, =0x00000010
	str	r1, [r0, #0x108]	@ TAG_LATENCY_CTRL

	ldr	r1, =0x00000120
	str	r1, [r0, #0x10c]	@ DATA_LATENCY_CTRL

	ldr	r1, =0xffffffff
	str	r1, [r0, #0x77c]	@ INV_WAY

	mov	pc, lr


@ ARMv7 L1 dcache invalidation routine. I did not write this function myself.
@ Source:
@    http://lists.infradead.org/pipermail/linux-arm-kernel/2013-February/149044.html
v7_invalidate_l1:
	mov	r0, #0
	mcr	p15, 2, r0, c0, c0, 0
	mrc	p15, 1, r0, c0, c0, 0
	movw    r1, #0x7fff
	and	r2, r1, r0, lsr #13

	movw    r1, #0x3ff

	and	r3, r1, r0, lsr #3      @ NumWays - 1
	add	r2, r2, #1              @ NumSets

	and	r0, r0, #0x7
	add	r0, r0, #4      @ SetShift

	clz	r1, r3          @ WayShift
	add	r4, r3, #1      @ NumWays
1:	sub	r2, r2, #1      @ NumSets--
	mov	r3, r4          @ Temp = NumWays
2:	subs	r3, r3, #1      @ Temp--
	mov	r5, r3, lsl r1
	mov	r6, r2, lsl r0
	orr	r5, r5, r6      @ Reg = (Temp<<WayShift)|(NumSets<<SetShift)
	mcr	p15, 0, r5, c7, c6, 2
	bgt	2b
	cmp	r2, #0
	bgt	1b
	dsb	st
	isb
	mov	pc, lr

