#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#include "puran2.h"

#define PURAN2_BUF_SIZE 1024
#define PURAN2_PAR_BUF_SIZE 1088

#define PURAN2_BLCK_SIZE 128
#define PURAN2_PAR_SIZE 8
#define PURAN2_PAR_BLCK_SIZE (PURAN2_BLCK_SIZE + PURAN2_PAR_SIZE)

static int byte_swap;
static int wrong_parity_count;

static char *filename;
static int file = -1;
static unsigned int file_length;
static int wrap;

static unsigned long buffer [PURAN2_BUF_SIZE];
static unsigned long long_count;
static unsigned long long_max_count;

#define PURAN2_OK           0
#define PURAN2_INITERR     -1
#define PURAN2_INITNO      -2
#define PURAN2_SEEKINVALID -3
#define PURAN2_SEEKERR     -4
#define PURAN2_PARITYEOF   -5
#define PURAN2_PARITYREAD  -6
#define PURAN2_PARITYERR   -7

static char *error_str = "";
static int   error_num = 0;

static unsigned char parity_bit [256];
static int read_buffer()
{
	unsigned char *c;
	unsigned long *l;
	unsigned long horizontal, vertical;
	long bytes_read;
	int i, j, long_parity;

	/* in case of given parity read data into temporary buffer first: */
	unsigned char tmp_buffer [PURAN2_PAR_BUF_SIZE];
	unsigned char *end_of_buffer;
	unsigned char *tmp_buffer_pos;
	unsigned char *block;

	if ((bytes_read = read(file, tmp_buffer, PURAN2_PAR_BUF_SIZE)) < 0)
	{
		error_num = PURAN2_PARITYREAD;
		error_str = "error found during read";
	}
	else if (bytes_read == 0)
	{
		error_num = PURAN2_PARITYEOF;
		error_str = "end of file during read";
		return puran2_seek(0);
	}
	else
	{
		long_max_count = (bytes_read / PURAN2_PAR_BLCK_SIZE)
				* (PURAN2_BLCK_SIZE / 4);
		long_count = 0;
	}

	end_of_buffer = (unsigned char *) buffer + (bytes_read
			/ PURAN2_PAR_BLCK_SIZE) * PURAN2_BLCK_SIZE;
	tmp_buffer_pos = tmp_buffer;

	for (block = (unsigned char *) buffer; block < end_of_buffer; block
			+= PURAN2_BLCK_SIZE)
	{
		memcpy(block, tmp_buffer_pos, PURAN2_BLCK_SIZE);
		tmp_buffer_pos += PURAN2_BLCK_SIZE;
		l = (unsigned long *) block;
		c = block;
		horizontal = 0;
		vertical = 0;
		for (i = 31; i >= 0; i--)
		{
			long_parity = 0;
			for (j = 0; j < 4; j++)
			{
				long_parity ^= *c++;
			}
            horizontal |= (parity_bit[long_parity] & 0x1) << i;
			vertical ^= *l++;
		}
		if (byte_swap)
		{
			char tmp;
			c = (unsigned char*) &horizontal;
			tmp = c[0]; c[0] = c[3]; c[3] = tmp;
			tmp = c[1]; c[1] = c[2]; c[2] = tmp;
		}

		if (vertical != * (unsigned long *) tmp_buffer_pos)
		{
			wrong_parity_count++;
			error_str = "Wrong vertial parity";
			error_num = PURAN2_PARITYERR;
		}
		if (horizontal != * (unsigned long *) (tmp_buffer_pos + 4))
		{
			wrong_parity_count++;
			error_str = "Wrong vertial parity";
			error_num = PURAN2_PARITYERR;
		}
		tmp_buffer_pos += 8; /* skip parity data in temporary buffer */
	}

	return error_num;
}

DLLIMPORT int puran2_init(char *f, int w)
{
	unsigned char byte;
	int byte_value;
	int bit;
	union
	{
		char c[4];
		int i;
	} byte_swap_test;

	if ( file != -1 ) return PURAN2_OK;
	error_num = PURAN2_OK;
	
	filename = strdup(f);
	wrap = w;
	wrong_parity_count = 0;

	/* check wether bytes are to be swapped (in case of differing architecture): */
	byte_swap_test.i = 0;
	byte_swap_test.c[0] = 1;
	byte_swap = (byte_swap_test.i != 1) ? 1 : 0;

	if ((file = open(filename, O_RDONLY | O_BINARY)) < 0)
	{
		error_str = "Puran2 can't open file";
		error_num = PURAN2_INITERR;
		return error_num;
	}

	/* check filesize */
	file_length = lseek(file, 0, SEEK_END);
	lseek(file, 0, SEEK_SET);
	if (file_length != (file_length / PURAN2_PAR_BLCK_SIZE)
				* PURAN2_PAR_BLCK_SIZE)
		{
			error_str = "Puran2 data truncated to multiple of block length";
			file_length = (file_length / PURAN2_PAR_BLCK_SIZE)
					* PURAN2_PAR_BLCK_SIZE;
		}

	/* compute parity bits for all values from 0 to 255: */

	for (byte_value = 0; byte_value < 256; byte_value++)
	{
		parity_bit [byte_value] = 0;
		byte = byte_value;
		for (bit = 0; bit < 8; bit++)
		{
			if (byte & 0x80)
				parity_bit [byte_value]++;
			byte <<= 0x1;
		}
		parity_bit [byte_value] &= 0x1; 
	}

	long_count = 0;
return PURAN2_OK;
}
DLLIMPORT int puran2_seek(long pos)
{
	unsigned long seek_pos;
	unsigned long i;

	if ( error_num != PURAN2_OK )
		return 0;
	
	if ( file == -1 ) 
	{
		error_str = "puran2 not initialisiert";
		error_num = PURAN2_INITNO;

	}
	else if ( pos < 0 || pos > file_length)
	{
		error_str = "illegal position for file_seek, ignored";
		error_num = PURAN2_SEEKINVALID;
	}
	else
	{
		seek_pos = (pos / PURAN2_BLCK_SIZE) * PURAN2_PAR_BLCK_SIZE;
		if (lseek(file, seek_pos, SEEK_SET) >= 0)
		{
			read_buffer();
			/* skip upto desired position */
			for (i = 0; i < pos % PURAN2_BLCK_SIZE; i++)
			{
				(void) puran2_random();
			}
			return pos;
		}
		else
		{
			error_num = PURAN2_SEEKERR;
			error_str ="can't seek to position";
		}
	}
    return error_num;
}

DLLIMPORT unsigned long puran2_random()
{
	union
	{
		unsigned char byte [4];
		unsigned long ldummy;
	} Long;
	
	if ( error_num != PURAN2_OK )
		return 0;
	
	if ( file == -1 ) 
	{
		error_str = "puran2 not initialisiert";
		error_num = PURAN2_INITNO;
        return 0;
	}
	
	if (long_max_count == long_count)
		if ( read_buffer() < 0 ) return 0;

	if (byte_swap)
	{
	   char tmp;
	   Long.ldummy = * (unsigned long *) (&buffer [long_count++]);
	   tmp = Long.byte[0]; Long.byte[0] = Long.byte[3]; Long.byte[3] = tmp;
	   tmp = Long.byte[1]; Long.byte[1] = Long.byte[2]; Long.byte[2] = tmp;
	   return * (unsigned long *) (Long.byte);
	}
	else
	{
	   return * (unsigned long *) (&buffer [long_count++]);
	}

}
DLLIMPORT long puran2_long()
{
	return puran2_random() & 0x7fffffff;
}
DLLIMPORT void puran2_stop()
{
	if ( file < 0 ) return;
	close(file);
	file = -1;
}

DLLIMPORT char *puran2_get_error()
{
	return error_str;
}
DLLIMPORT int puran2_get_errornum()
{
	return error_num;
}
