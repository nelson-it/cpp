#include <pthread.h>
//
// File : rijndael.cpp
// Creation date : Sun Nov 5 2000 03:22:10 CEST
// Author : Szymon Stefanek (stefanek@tin.it)
//
// Another implementation of the Rijndael cipher.
// This is intended to be an easily usable library file.
// This code is public domain.
// Based on the Vincent Rijmen and K.U.Leuven implementation 2.4.
//

//
// Original Copyright notice:
//
//    rijndael-alg-fst.c   v2.4   April '2000
//    rijndael-alg-fst.h
//    rijndael-api-fst.c
//    rijndael-api-fst.h
//
//    Optimised ANSI C code
//
//    authors: v1.0: Antoon Bosselaers
//             v2.0: Vincent Rijmen, K.U.Leuven
//             v2.3: Paulo Barreto
//             v2.4: Vincent Rijmen, K.U.Leuven
//
//    This code is placed in the public domain.
//

//
// This implementation works on 128 , 192 , 256 bit keys
// and on 128 bit blocks
//

#define _RIJNDAEL_CPP_

#include "rijndael.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rijndael.dat"

Rijndael::Rijndael()
{
	 m_state = Invalid;
}

Rijndael::~Rijndael()
{
}

int Rijndael::init(Mode mode, Direction dir,
                   const unsigned char * key,
		   KeyLength keyLen,
		   unsigned char * initVector)
{
    unsigned long uKeyLenInBytes;
    unsigned char keyMatrix[_MAX_KEY_COLUMNS][4];

    m_state = Invalid;

    if((mode != CBC) && (mode != ECB) && (mode != CFB1))
	return RIJNDAEL_UNSUPPORTED_MODE;

    m_mode = mode;

    if((dir != Encrypt) && (dir != Decrypt))
	return RIJNDAEL_UNSUPPORTED_DIRECTION;
    m_direction = dir;

    if(initVector)
	for(int i = 0;i < MAX_IV_SIZE;i++)
	    m_initVector[i] = initVector[i];
    else
	for(int i = 0;i < MAX_IV_SIZE;i++)
	    m_initVector[i] = 0;


    switch(keyLen)
    {
	case Key16Bytes:
	    uKeyLenInBytes = 16;
	    m_uRounds = 10;
	    break;
	case Key24Bytes:
	    uKeyLenInBytes = 24;
	    m_uRounds = 12;
	    break;
	case Key32Bytes:
	    uKeyLenInBytes = 32;
	    m_uRounds = 14;
	    break;
	default:
	    return RIJNDAEL_UNSUPPORTED_KEY_LENGTH;
	    break;
    }

    // The number of rounds is calculated as
    // m_uRounds = (m_uKeyLenInBits / 32) + 6;

    if(!key)
        return RIJNDAEL_BAD_KEY;

    for(unsigned long i = 0;i < uKeyLenInBytes;i++)
	keyMatrix[i >> 2][i & 3] = key[i]; 

    keySched(keyMatrix);

    if(m_direction == Decrypt)
        keyEncToDec();

    m_state = Valid;

    return RIJNDAEL_SUCCESS;
}

int Rijndael::blockEncrypt(const unsigned char *input,int inputLen,unsigned char *outBuffer)
{
	int i, k, numBlocks;
	unsigned char block[16], iv[4][4];

	if(m_state != Valid)return RIJNDAEL_NOT_INITIALIZED;
	if(m_direction != Encrypt)return RIJNDAEL_BAD_DIRECTION;

	if(input == 0 || inputLen <= 0)return 0;

	numBlocks = inputLen/128;
	
	switch(m_mode){
		case ECB: 
			for(i = numBlocks;i > 0;i--)
			{
				encrypt(input,outBuffer);
				input += 16;
				outBuffer += 16;
			}
		break;
		case CBC:
			((unsigned long*)block)[0] = ((unsigned long*)m_initVector)[0] ^ ((unsigned long*)input)[0];
			((unsigned long*)block)[1] = ((unsigned long*)m_initVector)[1] ^ ((unsigned long*)input)[1];
			((unsigned long*)block)[2] = ((unsigned long*)m_initVector)[2] ^ ((unsigned long*)input)[2];
			((unsigned long*)block)[3] = ((unsigned long*)m_initVector)[3] ^ ((unsigned long*)input)[3];
			encrypt(block,outBuffer);
			input += 16;
			for(i = numBlocks - 1;i > 0;i--)
			{
				((unsigned long*)block)[0] = ((unsigned long*)outBuffer)[0] ^ ((unsigned long*)input)[0];
				((unsigned long*)block)[1] = ((unsigned long*)outBuffer)[1] ^ ((unsigned long*)input)[1];
				((unsigned long*)block)[2] = ((unsigned long*)outBuffer)[2] ^ ((unsigned long*)input)[2];
				((unsigned long*)block)[3] = ((unsigned long*)outBuffer)[3] ^ ((unsigned long*)input)[3];
				outBuffer += 16;
				encrypt(block,outBuffer);
				input += 16;
			}
		break;
		case CFB1:
#if STRICT_ALIGN 
			memcpy(iv,m_initVector,16); 
#else  /* !STRICT_ALIGN */
			*((unsigned long*)iv[0]) = *((unsigned long*)(m_initVector   ));
			*((unsigned long*)iv[1]) = *((unsigned long*)(m_initVector + 4));
			*((unsigned long*)iv[2]) = *((unsigned long*)(m_initVector + 8));
			*((unsigned long*)iv[3]) = *((unsigned long*)(m_initVector +12));
#endif /* ?STRICT_ALIGN */
			for(i = numBlocks; i > 0; i--)
			{
				for(k = 0; k < 128; k++)
				{
					*((unsigned long*) block    ) = *((unsigned long*)iv[0]);
					*((unsigned long*)(block+ 4)) = *((unsigned long*)iv[1]);
					*((unsigned long*)(block+ 8)) = *((unsigned long*)iv[2]);
					*((unsigned long*)(block+12)) = *((unsigned long*)iv[3]);
					encrypt(block,block);
					outBuffer[k/8] ^= (block[0] & 0x80) >> (k & 7);
					iv[0][0] = (iv[0][0] << 1) | (iv[0][1] >> 7);
					iv[0][1] = (iv[0][1] << 1) | (iv[0][2] >> 7);
					iv[0][2] = (iv[0][2] << 1) | (iv[0][3] >> 7);
					iv[0][3] = (iv[0][3] << 1) | (iv[1][0] >> 7);
					iv[1][0] = (iv[1][0] << 1) | (iv[1][1] >> 7);
					iv[1][1] = (iv[1][1] << 1) | (iv[1][2] >> 7);
					iv[1][2] = (iv[1][2] << 1) | (iv[1][3] >> 7);
					iv[1][3] = (iv[1][3] << 1) | (iv[2][0] >> 7);
					iv[2][0] = (iv[2][0] << 1) | (iv[2][1] >> 7);
					iv[2][1] = (iv[2][1] << 1) | (iv[2][2] >> 7);
					iv[2][2] = (iv[2][2] << 1) | (iv[2][3] >> 7);
					iv[2][3] = (iv[2][3] << 1) | (iv[3][0] >> 7);
					iv[3][0] = (iv[3][0] << 1) | (iv[3][1] >> 7);
					iv[3][1] = (iv[3][1] << 1) | (iv[3][2] >> 7);
					iv[3][2] = (iv[3][2] << 1) | (iv[3][3] >> 7);
					iv[3][3] = (iv[3][3] << 1) | (outBuffer[k/8] >> (7-(k&7))) & 1;
				}
			}
		break;
		default:
			return -1;
		break;
	}
	
	return 128 * numBlocks;
}

int Rijndael::padEncrypt(const unsigned char *input, int inputOctets, unsigned char *outBuffer)
{
	int i, numBlocks, padLen;
	unsigned char block[16], *iv;

	if(m_state != Valid)return RIJNDAEL_NOT_INITIALIZED;
	if(m_direction != Encrypt)return RIJNDAEL_NOT_INITIALIZED;

	if(input == 0 || inputOctets <= 0)return 0;

	numBlocks = inputOctets/16;

	switch(m_mode)
	{
		case ECB: 
			for(i = numBlocks; i > 0; i--)
			{
				encrypt(input, outBuffer);
				input += 16;
				outBuffer += 16;
			}
			padLen = 16 - (inputOctets - 16*numBlocks);
//			assert(padLen > 0 && padLen <= 16);
			memcpy(block, input, 16 - padLen);
			memset(block + 16 - padLen, padLen, padLen);
			encrypt(block,outBuffer);
		break;
		case CBC:
			iv = m_initVector;
			for(i = numBlocks; i > 0; i--)
			{
				((unsigned long*)block)[0] = ((unsigned long*)input)[0] ^ ((unsigned long*)iv)[0];
				((unsigned long*)block)[1] = ((unsigned long*)input)[1] ^ ((unsigned long*)iv)[1];
				((unsigned long*)block)[2] = ((unsigned long*)input)[2] ^ ((unsigned long*)iv)[2];
				((unsigned long*)block)[3] = ((unsigned long*)input)[3] ^ ((unsigned long*)iv)[3];
				encrypt(block, outBuffer);
				iv = outBuffer;
				input += 16;
				outBuffer += 16;
			}
			padLen = 16 - (inputOctets - 16*numBlocks);
//			assert(padLen > 0 && padLen <= 16); // DO SOMETHING HERE ?
			for (i = 0; i < 16 - padLen; i++) {
				block[i] = input[i] ^ iv[i];
			}
			for (i = 16 - padLen; i < 16; i++) {
				block[i] = (unsigned char)padLen ^ iv[i];
			}
			encrypt(block,outBuffer);
		break;
		default:
			return -1;
		break;
	}
	
	return 16*(numBlocks + 1);
}
	
int Rijndael::blockDecrypt(const unsigned char *input, int inputLen, unsigned char *outBuffer)
{
	int i, k, numBlocks;
	unsigned char block[16], iv[4][4];

	if(m_state != Valid)return RIJNDAEL_NOT_INITIALIZED;
	if((m_mode != CFB1) && (m_direction == Encrypt))return RIJNDAEL_BAD_DIRECTION;

	if (input == 0 || inputLen <= 0)return 0;

	numBlocks = inputLen/128;

	switch(m_mode)
	{
		case ECB: 
			for (i = numBlocks; i > 0; i--)
			{
				decrypt(input,outBuffer);
				input += 16;
				outBuffer += 16;
			}
		break;
		case CBC:
#if STRICT_ALIGN 
			memcpy(iv,m_initVector,16); 
#else
			*((unsigned long*)iv[0]) = *((unsigned long*)(m_initVector  ));
			*((unsigned long*)iv[1]) = *((unsigned long*)(m_initVector+ 4));
			*((unsigned long*)iv[2]) = *((unsigned long*)(m_initVector+ 8));
			*((unsigned long*)iv[3]) = *((unsigned long*)(m_initVector+12));
#endif
			for (i = numBlocks; i > 0; i--)
			{
				decrypt(input, block);
				((unsigned long*)block)[0] ^= *((unsigned long*)iv[0]);
				((unsigned long*)block)[1] ^= *((unsigned long*)iv[1]);
				((unsigned long*)block)[2] ^= *((unsigned long*)iv[2]);
				((unsigned long*)block)[3] ^= *((unsigned long*)iv[3]);
#if STRICT_ALIGN
				memcpy(iv, input, 16);
				memcpy(outBuf, block, 16);
#else
				*((unsigned long*)iv[0]) = ((unsigned long*)input)[0]; ((unsigned long*)outBuffer)[0] = ((unsigned long*)block)[0];
				*((unsigned long*)iv[1]) = ((unsigned long*)input)[1]; ((unsigned long*)outBuffer)[1] = ((unsigned long*)block)[1];
				*((unsigned long*)iv[2]) = ((unsigned long*)input)[2]; ((unsigned long*)outBuffer)[2] = ((unsigned long*)block)[2];
				*((unsigned long*)iv[3]) = ((unsigned long*)input)[3]; ((unsigned long*)outBuffer)[3] = ((unsigned long*)block)[3];
#endif
				input += 16;
				outBuffer += 16;
			}
			break;
		case CFB1:
#if STRICT_ALIGN 
			memcpy(iv, m_initVector, 16); 
#else
			*((unsigned long*)iv[0]) = *((unsigned long*)(m_initVector));
			*((unsigned long*)iv[1]) = *((unsigned long*)(m_initVector+ 4));
			*((unsigned long*)iv[2]) = *((unsigned long*)(m_initVector+ 8));
			*((unsigned long*)iv[3]) = *((unsigned long*)(m_initVector+12));
#endif
			for(i = numBlocks; i > 0; i--)
			{
				for(k = 0; k < 128; k++)
				{
					*((unsigned long*) block    ) = *((unsigned long*)iv[0]);
					*((unsigned long*)(block+ 4)) = *((unsigned long*)iv[1]);
					*((unsigned long*)(block+ 8)) = *((unsigned long*)iv[2]);
					*((unsigned long*)(block+12)) = *((unsigned long*)iv[3]);
					encrypt(block, block);
					iv[0][0] = (iv[0][0] << 1) | (iv[0][1] >> 7);
					iv[0][1] = (iv[0][1] << 1) | (iv[0][2] >> 7);
					iv[0][2] = (iv[0][2] << 1) | (iv[0][3] >> 7);
					iv[0][3] = (iv[0][3] << 1) | (iv[1][0] >> 7);
					iv[1][0] = (iv[1][0] << 1) | (iv[1][1] >> 7);
					iv[1][1] = (iv[1][1] << 1) | (iv[1][2] >> 7);
					iv[1][2] = (iv[1][2] << 1) | (iv[1][3] >> 7);
					iv[1][3] = (iv[1][3] << 1) | (iv[2][0] >> 7);
					iv[2][0] = (iv[2][0] << 1) | (iv[2][1] >> 7);
					iv[2][1] = (iv[2][1] << 1) | (iv[2][2] >> 7);
					iv[2][2] = (iv[2][2] << 1) | (iv[2][3] >> 7);
					iv[2][3] = (iv[2][3] << 1) | (iv[3][0] >> 7);
					iv[3][0] = (iv[3][0] << 1) | (iv[3][1] >> 7);
					iv[3][1] = (iv[3][1] << 1) | (iv[3][2] >> 7);
					iv[3][2] = (iv[3][2] << 1) | (iv[3][3] >> 7);
					iv[3][3] = (iv[3][3] << 1) | (input[k/8] >> (7-(k&7))) & 1;
					outBuffer[k/8] ^= (block[0] & 0x80) >> (k & 7);
				}
			}
		break;
		default:
			return -1;
		break;
	}
	
	return 128*numBlocks;
}

int Rijndael::padDecrypt(const unsigned char *input, int inputOctets, unsigned char *outBuffer)
{
	int i, numBlocks, padLen;
	unsigned char block[16];
	unsigned long iv[4];

	if(m_state != Valid)return RIJNDAEL_NOT_INITIALIZED;
	if(m_direction != Decrypt)return RIJNDAEL_BAD_DIRECTION;

	if(input == 0 || inputOctets <= 0)return 0;

	if((inputOctets % 16) != 0)return RIJNDAEL_CORRUPTED_DATA;

	numBlocks = inputOctets/16;

	switch(m_mode){
		case ECB:
			for (i = numBlocks - 1; i > 0; i--)
			{
				decrypt(input, outBuffer);
				input += 16;
				outBuffer += 16;
			}

			decrypt(input, block);
			padLen = block[15];
			if (padLen >= 16)return RIJNDAEL_CORRUPTED_DATA;
			for(i = 16 - padLen; i < 16; i++)
			{
				if(block[i] != padLen)return RIJNDAEL_CORRUPTED_DATA;
			}
			memcpy(outBuffer, block, 16 - padLen);
		break;	
		case CBC:
			memcpy(iv, m_initVector, 16);
			/* all blocks but last */
			for (i = numBlocks - 1; i > 0; i--)
			{
				decrypt(input, block);
				((unsigned long*)block)[0] ^= iv[0];
				((unsigned long*)block)[1] ^= iv[1];
				((unsigned long*)block)[2] ^= iv[2];
				((unsigned long*)block)[3] ^= iv[3];
				memcpy(iv, input, 16);
				memcpy(outBuffer, block, 16);
				input += 16;
				outBuffer += 16;
			}
			/* last block */
			decrypt(input, block);
			((unsigned long*)block)[0] ^= iv[0];
			((unsigned long*)block)[1] ^= iv[1];
			((unsigned long*)block)[2] ^= iv[2];
			((unsigned long*)block)[3] ^= iv[3];
			padLen = block[15];
			if(padLen <= 0 || padLen > 16)return RIJNDAEL_CORRUPTED_DATA;
			for(i = 16 - padLen; i < 16; i++)
			{
				if(block[i] != padLen)return RIJNDAEL_CORRUPTED_DATA;
			}
			memcpy(outBuffer, block, 16 - padLen);
			break;
		
		default:
			return -1;
		break;
	}
	
	return 16*numBlocks - padLen;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ALGORITHM
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////


void Rijndael::keySched(unsigned char key[_MAX_KEY_COLUMNS][4])
{
	int j,rconpointer = 0;

	// Calculate the necessary round keys
	// The number of calculations depends on keyBits and blockBits
	int uKeyColumns = m_uRounds - 6;

	unsigned char tempKey[_MAX_KEY_COLUMNS][4];

	// Copy the input key to the temporary key matrix

	for(j = 0;j < uKeyColumns;j++)
	{
		*((unsigned long*)(tempKey[j])) = *((unsigned long*)(key[j]));
	}

	unsigned int r = 0;
	unsigned int t = 0;

	// copy values into round key array
	for(j = 0;(j < uKeyColumns) && (r <= m_uRounds); )
	{
		for(;(j < uKeyColumns) && (t < 4); j++, t++)
		{
			*((unsigned long*)m_expandedKey[r][t]) = *((unsigned long*)tempKey[j]);
		}


		if(t == 4)
		{
			r++;
			t = 0;
		}
	}
		
	while(r <= m_uRounds)
	{
		tempKey[0][0] ^= S[tempKey[uKeyColumns-1][1]];
		tempKey[0][1] ^= S[tempKey[uKeyColumns-1][2]];
		tempKey[0][2] ^= S[tempKey[uKeyColumns-1][3]];
		tempKey[0][3] ^= S[tempKey[uKeyColumns-1][0]];
		tempKey[0][0] ^= rcon[rconpointer++];

		if (uKeyColumns != 8)
		{
			for(j = 1; j < uKeyColumns; j++)
			{
				*((unsigned long*)tempKey[j]) ^= *((unsigned long*)tempKey[j-1]);
			}
		} else {
			for(j = 1; j < uKeyColumns/2; j++)
			{
				*((unsigned long*)tempKey[j]) ^= *((unsigned long*)tempKey[j-1]);
			}
			tempKey[uKeyColumns/2][0] ^= S[tempKey[uKeyColumns/2 - 1][0]];
			tempKey[uKeyColumns/2][1] ^= S[tempKey[uKeyColumns/2 - 1][1]];
			tempKey[uKeyColumns/2][2] ^= S[tempKey[uKeyColumns/2 - 1][2]];
			tempKey[uKeyColumns/2][3] ^= S[tempKey[uKeyColumns/2 - 1][3]];
			for(j = uKeyColumns/2 + 1; j < uKeyColumns; j++)
			{
				*((unsigned long*)tempKey[j]) ^= *((unsigned long*)tempKey[j-1]);
			}
		}
		for(j = 0; (j < uKeyColumns) && (r <= m_uRounds); )
		{
			for(; (j < uKeyColumns) && (t < 4); j++, t++)
			{
				*((unsigned long*)m_expandedKey[r][t]) = *((unsigned long*)tempKey[j]);
			}
			if(t == 4)
			{
				r++;
				t = 0;
			}
		}
	}		
}

void Rijndael::keyEncToDec()
{
	unsigned int r;
	unsigned char *w;

	for(r = 1; r < m_uRounds; r++)
	{
		w = m_expandedKey[r][0];
		*((unsigned long*)w) = *((unsigned long*)U1[w[0]]) ^ *((unsigned long*)U2[w[1]]) ^ *((unsigned long*)U3[w[2]]) ^ *((unsigned long*)U4[w[3]]);
		w = m_expandedKey[r][1];
		*((unsigned long*)w) = *((unsigned long*)U1[w[0]]) ^ *((unsigned long*)U2[w[1]]) ^ *((unsigned long*)U3[w[2]]) ^ *((unsigned long*)U4[w[3]]);
		w = m_expandedKey[r][2];
		*((unsigned long*)w) = *((unsigned long*)U1[w[0]]) ^ *((unsigned long*)U2[w[1]]) ^ *((unsigned long*)U3[w[2]]) ^ *((unsigned long*)U4[w[3]]);
		w = m_expandedKey[r][3];
		*((unsigned long*)w) = *((unsigned long*)U1[w[0]]) ^ *((unsigned long*)U2[w[1]]) ^ *((unsigned long*)U3[w[2]]) ^ *((unsigned long*)U4[w[3]]);
	}
}	

void Rijndael::encrypt(const unsigned char a[16], unsigned char b[16])
{
	unsigned int r;
	unsigned char temp[4][4];

    *((unsigned long*)temp[0]) = *((unsigned long*)(a   )) ^ *((unsigned long*)m_expandedKey[0][0]);
    *((unsigned long*)temp[1]) = *((unsigned long*)(a+ 4)) ^ *((unsigned long*)m_expandedKey[0][1]);
    *((unsigned long*)temp[2]) = *((unsigned long*)(a+ 8)) ^ *((unsigned long*)m_expandedKey[0][2]);
    *((unsigned long*)temp[3]) = *((unsigned long*)(a+12)) ^ *((unsigned long*)m_expandedKey[0][3]);
    *((unsigned long*)(b    )) = *((unsigned long*)T1[temp[0][0]])
						^ *((unsigned long*)T2[temp[1][1]])
						^ *((unsigned long*)T3[temp[2][2]]) 
						^ *((unsigned long*)T4[temp[3][3]]);
    *((unsigned long*)(b + 4)) = *((unsigned long*)T1[temp[1][0]])
						^ *((unsigned long*)T2[temp[2][1]])
						^ *((unsigned long*)T3[temp[3][2]]) 
						^ *((unsigned long*)T4[temp[0][3]]);
    *((unsigned long*)(b + 8)) = *((unsigned long*)T1[temp[2][0]])
						^ *((unsigned long*)T2[temp[3][1]])
						^ *((unsigned long*)T3[temp[0][2]]) 
						^ *((unsigned long*)T4[temp[1][3]]);
    *((unsigned long*)(b +12)) = *((unsigned long*)T1[temp[3][0]])
						^ *((unsigned long*)T2[temp[0][1]])
						^ *((unsigned long*)T3[temp[1][2]]) 
						^ *((unsigned long*)T4[temp[2][3]]);
	for(r = 1; r < m_uRounds-1; r++)
	{
		*((unsigned long*)temp[0]) = *((unsigned long*)(b   )) ^ *((unsigned long*)m_expandedKey[r][0]);
		*((unsigned long*)temp[1]) = *((unsigned long*)(b+ 4)) ^ *((unsigned long*)m_expandedKey[r][1]);
		*((unsigned long*)temp[2]) = *((unsigned long*)(b+ 8)) ^ *((unsigned long*)m_expandedKey[r][2]);
		*((unsigned long*)temp[3]) = *((unsigned long*)(b+12)) ^ *((unsigned long*)m_expandedKey[r][3]);

		*((unsigned long*)(b    )) = *((unsigned long*)T1[temp[0][0]])
							^ *((unsigned long*)T2[temp[1][1]])
							^ *((unsigned long*)T3[temp[2][2]]) 
							^ *((unsigned long*)T4[temp[3][3]]);
		*((unsigned long*)(b + 4)) = *((unsigned long*)T1[temp[1][0]])
							^ *((unsigned long*)T2[temp[2][1]])
							^ *((unsigned long*)T3[temp[3][2]]) 
							^ *((unsigned long*)T4[temp[0][3]]);
		*((unsigned long*)(b + 8)) = *((unsigned long*)T1[temp[2][0]])
							^ *((unsigned long*)T2[temp[3][1]])
							^ *((unsigned long*)T3[temp[0][2]]) 
							^ *((unsigned long*)T4[temp[1][3]]);
		*((unsigned long*)(b +12)) = *((unsigned long*)T1[temp[3][0]])
							^ *((unsigned long*)T2[temp[0][1]])
							^ *((unsigned long*)T3[temp[1][2]]) 
							^ *((unsigned long*)T4[temp[2][3]]);
	}
	*((unsigned long*)temp[0]) = *((unsigned long*)(b   )) ^ *((unsigned long*)m_expandedKey[m_uRounds-1][0]);
	*((unsigned long*)temp[1]) = *((unsigned long*)(b+ 4)) ^ *((unsigned long*)m_expandedKey[m_uRounds-1][1]);
	*((unsigned long*)temp[2]) = *((unsigned long*)(b+ 8)) ^ *((unsigned long*)m_expandedKey[m_uRounds-1][2]);
	*((unsigned long*)temp[3]) = *((unsigned long*)(b+12)) ^ *((unsigned long*)m_expandedKey[m_uRounds-1][3]);
	b[ 0] = T1[temp[0][0]][1];
	b[ 1] = T1[temp[1][1]][1];
	b[ 2] = T1[temp[2][2]][1];
	b[ 3] = T1[temp[3][3]][1];
	b[ 4] = T1[temp[1][0]][1];
	b[ 5] = T1[temp[2][1]][1];
	b[ 6] = T1[temp[3][2]][1];
	b[ 7] = T1[temp[0][3]][1];
	b[ 8] = T1[temp[2][0]][1];
	b[ 9] = T1[temp[3][1]][1];
	b[10] = T1[temp[0][2]][1];
	b[11] = T1[temp[1][3]][1];
	b[12] = T1[temp[3][0]][1];
	b[13] = T1[temp[0][1]][1];
	b[14] = T1[temp[1][2]][1];
	b[15] = T1[temp[2][3]][1];
	*((unsigned long*)(b   )) ^= *((unsigned long*)m_expandedKey[m_uRounds][0]);
	*((unsigned long*)(b+ 4)) ^= *((unsigned long*)m_expandedKey[m_uRounds][1]);
	*((unsigned long*)(b+ 8)) ^= *((unsigned long*)m_expandedKey[m_uRounds][2]);
	*((unsigned long*)(b+12)) ^= *((unsigned long*)m_expandedKey[m_uRounds][3]);
}

void Rijndael::decrypt(const unsigned char a[16], unsigned char b[16])
{
	int r;
	unsigned char temp[4][4];
	
    *((unsigned long*)temp[0]) = *((unsigned long*)(a   )) ^ *((unsigned long*)m_expandedKey[m_uRounds][0]);
    *((unsigned long*)temp[1]) = *((unsigned long*)(a+ 4)) ^ *((unsigned long*)m_expandedKey[m_uRounds][1]);
    *((unsigned long*)temp[2]) = *((unsigned long*)(a+ 8)) ^ *((unsigned long*)m_expandedKey[m_uRounds][2]);
    *((unsigned long*)temp[3]) = *((unsigned long*)(a+12)) ^ *((unsigned long*)m_expandedKey[m_uRounds][3]);

    *((unsigned long*)(b   )) = *((unsigned long*)T5[temp[0][0]])
           ^ *((unsigned long*)T6[temp[3][1]])
           ^ *((unsigned long*)T7[temp[2][2]]) 
           ^ *((unsigned long*)T8[temp[1][3]]);
	*((unsigned long*)(b+ 4)) = *((unsigned long*)T5[temp[1][0]])
           ^ *((unsigned long*)T6[temp[0][1]])
           ^ *((unsigned long*)T7[temp[3][2]]) 
           ^ *((unsigned long*)T8[temp[2][3]]);
	*((unsigned long*)(b+ 8)) = *((unsigned long*)T5[temp[2][0]])
           ^ *((unsigned long*)T6[temp[1][1]])
           ^ *((unsigned long*)T7[temp[0][2]]) 
           ^ *((unsigned long*)T8[temp[3][3]]);
	*((unsigned long*)(b+12)) = *((unsigned long*)T5[temp[3][0]])
           ^ *((unsigned long*)T6[temp[2][1]])
           ^ *((unsigned long*)T7[temp[1][2]]) 
           ^ *((unsigned long*)T8[temp[0][3]]);
	for(r = m_uRounds-1; r > 1; r--)
	{
		*((unsigned long*)temp[0]) = *((unsigned long*)(b   )) ^ *((unsigned long*)m_expandedKey[r][0]);
		*((unsigned long*)temp[1]) = *((unsigned long*)(b+ 4)) ^ *((unsigned long*)m_expandedKey[r][1]);
		*((unsigned long*)temp[2]) = *((unsigned long*)(b+ 8)) ^ *((unsigned long*)m_expandedKey[r][2]);
		*((unsigned long*)temp[3]) = *((unsigned long*)(b+12)) ^ *((unsigned long*)m_expandedKey[r][3]);
		*((unsigned long*)(b   )) = *((unsigned long*)T5[temp[0][0]])
           ^ *((unsigned long*)T6[temp[3][1]])
           ^ *((unsigned long*)T7[temp[2][2]]) 
           ^ *((unsigned long*)T8[temp[1][3]]);
		*((unsigned long*)(b+ 4)) = *((unsigned long*)T5[temp[1][0]])
           ^ *((unsigned long*)T6[temp[0][1]])
           ^ *((unsigned long*)T7[temp[3][2]]) 
           ^ *((unsigned long*)T8[temp[2][3]]);
		*((unsigned long*)(b+ 8)) = *((unsigned long*)T5[temp[2][0]])
           ^ *((unsigned long*)T6[temp[1][1]])
           ^ *((unsigned long*)T7[temp[0][2]]) 
           ^ *((unsigned long*)T8[temp[3][3]]);
		*((unsigned long*)(b+12)) = *((unsigned long*)T5[temp[3][0]])
           ^ *((unsigned long*)T6[temp[2][1]])
           ^ *((unsigned long*)T7[temp[1][2]]) 
           ^ *((unsigned long*)T8[temp[0][3]]);
	}
 
	*((unsigned long*)temp[0]) = *((unsigned long*)(b   )) ^ *((unsigned long*)m_expandedKey[1][0]);
	*((unsigned long*)temp[1]) = *((unsigned long*)(b+ 4)) ^ *((unsigned long*)m_expandedKey[1][1]);
	*((unsigned long*)temp[2]) = *((unsigned long*)(b+ 8)) ^ *((unsigned long*)m_expandedKey[1][2]);
	*((unsigned long*)temp[3]) = *((unsigned long*)(b+12)) ^ *((unsigned long*)m_expandedKey[1][3]);
	b[ 0] = S5[temp[0][0]];
	b[ 1] = S5[temp[3][1]];
	b[ 2] = S5[temp[2][2]];
	b[ 3] = S5[temp[1][3]];
	b[ 4] = S5[temp[1][0]];
	b[ 5] = S5[temp[0][1]];
	b[ 6] = S5[temp[3][2]];
	b[ 7] = S5[temp[2][3]];
	b[ 8] = S5[temp[2][0]];
	b[ 9] = S5[temp[1][1]];
	b[10] = S5[temp[0][2]];
	b[11] = S5[temp[3][3]];
	b[12] = S5[temp[3][0]];
	b[13] = S5[temp[2][1]];
	b[14] = S5[temp[1][2]];
	b[15] = S5[temp[0][3]];
	*((unsigned long*)(b   )) ^= *((unsigned long*)m_expandedKey[0][0]);
	*((unsigned long*)(b+ 4)) ^= *((unsigned long*)m_expandedKey[0][1]);
	*((unsigned long*)(b+ 8)) ^= *((unsigned long*)m_expandedKey[0][2]);
	*((unsigned long*)(b+12)) ^= *((unsigned long*)m_expandedKey[0][3]);
}
