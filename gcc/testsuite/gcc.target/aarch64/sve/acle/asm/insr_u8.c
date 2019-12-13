/* { dg-final { check-function-bodies "**" "" "-DCHECK_ASM" } } */

#include "test_sve_acle.h"

/*
** insr_w0_u8_tied1:
**	insr	z0\.b, w0
**	ret
*/
TEST_UNIFORM_ZX (insr_w0_u8_tied1, svuint8_t, uint8_t,
		 z0 = svinsr_n_u8 (z0, x0),
		 z0 = svinsr (z0, x0))

/*
** insr_w0_u8_untied:
**	movprfx	z0, z1
**	insr	z0\.b, w0
**	ret
*/
TEST_UNIFORM_ZX (insr_w0_u8_untied, svuint8_t, uint8_t,
		 z0 = svinsr_n_u8 (z1, x0),
		 z0 = svinsr (z1, x0))

/*
** insr_0_u8_tied1:
**	insr	z0\.b, wzr
**	ret
*/
TEST_UNIFORM_Z (insr_0_u8_tied1, svuint8_t,
		z0 = svinsr_n_u8 (z0, 0),
		z0 = svinsr (z0, 0))

/*
** insr_0_u8_untied:
**	movprfx	z0, z1
**	insr	z0\.b, wzr
**	ret
*/
TEST_UNIFORM_Z (insr_0_u8_untied, svuint8_t,
		z0 = svinsr_n_u8 (z1, 0),
		z0 = svinsr (z1, 0))

/*
** insr_1_u8:
** (
**	mov	(w[0-9]+), #?1
**	insr	z0\.b, \1
** |
**	movi	v([0-9]+)\.8b, 0x1
**	insr	z0\.b, b\2
** )
**	ret
*/
TEST_UNIFORM_Z (insr_1_u8, svuint8_t,
		z0 = svinsr_n_u8 (z0, 1),
		z0 = svinsr (z0, 1))
