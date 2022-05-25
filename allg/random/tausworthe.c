#include <stdio.h>

#include "puran2.h"
#include "tausworthe.h"

static unsigned long reg[5];      /* 159-Bit-Schieberegister                 */

static void step_reg()
{
	register unsigned long * q;
	register unsigned long * r;

	r = reg;
	q = r+1;

	*r ^= (*r>>31) | (*q<<1);
	*q ^= (*q>>31) | (*(q+1)<<1);
	q++;
	*q ^= (*q>>31) | (*(q+1)<<1);
	q++;
	*q ^= (*q>>31) | (*(q+1)<<1);
	q++;

	*q ^= (*r & 0x7fffffff);
	*q &= 0x7fffffff;
}

DLLIMPORT int tausworthe_init(unsigned long i0, unsigned long i1, unsigned long i2,
		unsigned long i3, unsigned long i4)
{
	if (i0 == 0 && i2 == 0 && i3 == 0 && i4 == 0)
	{
		reg[0] = 0x01234567;
		reg[1] = 0x89abcdef;
		reg[2] = 0xfedcba98;
		reg[3] = 0x76543210;
		reg[4] = 0xf0f00f0f;
	}
	else
	{
		reg[0]=i0;
		reg[1]=i1;
		reg[2]=i2;
		reg[3]=i3;
		reg[4]=i4;
	}
	return 0;
}

DLLIMPORT unsigned long tausworthe_random()
{
	step_reg();
	return (reg[0]);
}

DLLIMPORT unsigned long tausworthe_puran2_random()
{
	return tausworthe_random() ^ puran2_random();
}

DLLIMPORT long tausworthe_long()
{
	return tausworthe_random() & 0x7fffffff;
}

DLLIMPORT long tausworthe_puran2_long()
{
	return tausworthe_puran2_random() & 0x7fffffff;
}

