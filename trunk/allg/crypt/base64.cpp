#ifdef PTHREAD
#include <pthreads/pthread.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "base64.h"

#define LINELEN     76  // max Zeichen pro String
#define LINELEN_4   19

const char *CryptBase64::base64tab =  "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
	"abcdefghijklmnopqrstuvwxyz"
	"0123456789+/=";

CryptBase64::CryptBase64()
{
	int i;

	rbase64tab = new char[256];

	for ( i=0; i<256; i++)
		rbase64tab[i] = -1;

	for ( i=0; i<64; i++)
		rbase64tab[((int)base64tab[i])] = i;

	rbase64tab['='] = 0;
}

CryptBase64::~CryptBase64()
{
	delete rbase64tab;
}

std::string CryptBase64::encode (std::string in)
{
	int i,j,k,l,m;
	int len;

	char buffer[5];
	char line[LINELEN + 1];
	const char *crlf = "\n";

	std::string out;

	len = in.size();

	for( i=j=k=l=0; l < len; )
	{
		memset(line, 0, sizeof(line));
		for(i=0,m=0; i<LINELEN_4 && l < len; i++)
		{
			memset(buffer, 0, sizeof(buffer));

			for(j=0; j<3; j++)
			{
				if( l == len )
				{
					for(k=j + 1; k<4; k++)
						buffer[k]=0x40;

					crlf = "";
					break;
				}

				else if(j==0)
				{
					buffer[0]+=((in.c_str()[l] & 0xff ) >> 2 );
					buffer[1]+=((in.c_str()[l] & 0x03 ) << 4 );
				}
				else if(j==1)
				{
					buffer[1]+=((in.c_str()[l] & 0xff ) >> 4);
					buffer[2]+=((in.c_str()[l] & 0x0F ) << 2 );
				}
				else if(j==2)
				{
					buffer[2]+=((in.c_str()[l] & 0xff ) >> 6);
					buffer[3]+=(in.c_str()[l] & 0x3F);
				}

				++l;
			}

			for(j=m, k=0; k<4; k++)
				line[j+k] = base64tab[((int)buffer[k])];
			m = m + 4;
		}
		out = out + line + crlf;
	}
	return out;
}

int CryptBase64::encode (const unsigned char *in, unsigned char *out, int len)
{
	int i,j,k,l,m, count;

	char buffer[5];
	char line[LINELEN + 1];
	const char *crlf = "\n";

	for( count=i=j=k=l=0; l < len; )
	{
		memset(line, 0, sizeof(line));
		for(i=0,m=0; i<LINELEN_4 && l < len; i++)
		{
			memset(buffer, 0, sizeof(buffer));

			for(j=0; j<3 && l<len; j++)
			{
				if(j==0)
				{
					buffer[0]+=((in[l] & 0xff ) >> 2 );
					buffer[1]+=((in[l] & 0x03 ) << 4 );
				}
				else if(j==1)
				{
					buffer[1]+=((in[l] & 0xff ) >> 4);
					buffer[2]+=((in[l] & 0x0F ) << 2 );
				}
				else if(j==2)
				{
					buffer[2]+=((in[l] & 0xff ) >> 6);
					buffer[3]+=( in[l] & 0x3F);
				}

				++l;
			}

			for(k=0; k<j+1; k++)
				line[m+k] = base64tab[((int)buffer[k])];
			for(; k <4; k++)
				line[m+k] = '=';
			m = m + 4;
		}
		memcpy(&(out[count]), line, m);
		count += m;
		memcpy(&(out[count]), crlf, strlen(crlf));
		count += strlen(crlf);
	}
	return count;
}

std::string CryptBase64::decode (std::string in)
{
	int  i,n,m;
	int len;

	char buffer[4];
	std::string out;

	len = in.size();

	for( i=0; i<len && in.c_str()[i] !='='; )
	{
		memset(buffer,0,sizeof(buffer));
		for(n=0; n<4; n++)
		{
			while( isspace(in.c_str()[i]) && i < len ) i++;

			m = rbase64tab[((unsigned char)(in.c_str()[i])) & 0xff];
			if(m == -1 ) // falsches Zeichen
				return "";

			if(n==0)
			{
				buffer[0]+= (m<<2);
			}
			else if(n==1)
			{
				buffer[0]+= (m>>4);
				buffer[1]+=(m<<4);
			}
			else if(n==2)
			{
				buffer[1]+= (m>>2);
				buffer[2]+=(m<<6);
			}
			else
			{
				buffer[2]+=  m;
			}
			i++;
		}
		out = out + buffer;
		while( isspace(in.c_str()[i]) && i < len ) i++;
	}
	return out;
}

int CryptBase64::decode (const unsigned char *in, unsigned char *out, int len)
{
	int  i,n,m,count;

	char buffer[4];

	for( count=0, i=0; i<len && in[i] !='='; )
	{
		memset(buffer,0,sizeof(buffer));
		for(n=0; n<4 && i < len; n++)
		{
			while( ( isspace(in[i]) || in[i] == '\0' ) && i < len ) i++;

			if ( i < len )
			    m = rbase64tab[((unsigned char)(in[i])) & 0xff];

			if(m == -1 ) // falsches Zeichen
			{
			    return 0;
			}

			if(n==0)
			{
				buffer[0]+= (m<<2);
			}
			else if(n==1)
			{
				buffer[0]+= (m>>4);
				buffer[1]+=(m<<4);
			}
			else if(n==2)
			{
				buffer[1]+= (m>>2);
				buffer[2]+=(m<<6);
			}
			else
			{
				buffer[2]+=  m;
			}
			i++;

			if ( in[i] == '=' ) break;
		}
		memcpy(&(out[count]), buffer, n - 1);
		count += n - 1;
		if ( in[i] == '=' ) break;
		while( isspace(in[i]) && i < len ) i++;
	}
	return count;
}

int CryptBase64::encode_needsize(int len)
{
    int i = (( len / 3 ) + 1 ) * 4;
    return (i + ( i / 76 ) + 1) + 78;
}

