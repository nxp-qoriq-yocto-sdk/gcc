/* { dg-final { check-function-bodies "**" "" "-DCHECK_ASM" } } */

#include "test_sve_acle.h"

/*
** set2_u8_z24_0:
**	mov	z25\.d, z5\.d
**	mov	z24\.d, z0\.d
**	ret
*/
TEST_SET (set2_u8_z24_0, svuint8x2_t, svuint8_t,
	  z24 = svset2_u8 (z4, 0, z0),
	  z24 = svset2 (z4, 0, z0))

/*
** set2_u8_z24_1:
**	mov	z24\.d, z4\.d
**	mov	z25\.d, z0\.d
**	ret
*/
TEST_SET (set2_u8_z24_1, svuint8x2_t, svuint8_t,
	  z24 = svset2_u8 (z4, 1, z0),
	  z24 = svset2 (z4, 1, z0))

/*
** set2_u8_z4_0:
**	mov	z4\.d, z0\.d
**	ret
*/
TEST_SET (set2_u8_z4_0, svuint8x2_t, svuint8_t,
	  z4 = svset2_u8 (z4, 0, z0),
	  z4 = svset2 (z4, 0, z0))

/*
** set2_u8_z4_1:
**	mov	z5\.d, z0\.d
**	ret
*/
TEST_SET (set2_u8_z4_1, svuint8x2_t, svuint8_t,
	  z4 = svset2_u8 (z4, 1, z0),
	  z4 = svset2 (z4, 1, z0))
