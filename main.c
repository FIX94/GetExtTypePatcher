/*
 * Copyright (C) 2017 FIX94
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <inttypes.h>
#include <string.h>
#include <malloc.h>

static const uint8_t dolstart[4] = { 0x00, 0x00, 0x01, 0x00 };
static bool confirm = true;

typedef struct FuncPattern
{
	uint32_t Length;
	uint32_t Loads;
	uint32_t Stores;
	uint32_t FCalls;
	uint32_t Branch;
	uint32_t Moves;
	const char *Name;
	uint32_t Group;
	uint32_t Found;
} FuncPattern;

FuncPattern GetExtTypeA = {  0xD4, 18, 4,  3,  7,  2, "GetExtType A", 0, 0 };
FuncPattern GetExtTypeB = {  0xFC, 19, 4,  3,  9,  2, "GetExtType B", 0, 0 };
FuncPattern GetExtTypeC = { 0x26C, 59, 2, 16, 12, 10, "GetExtType C", 0, 0 };

void MPattern( uint8_t *Data, size_t Length, FuncPattern *FunctionPattern )
{
	uint32_t i;
	memset( FunctionPattern, 0, sizeof(FuncPattern) );

	for( i = 0; i < Length; i+=4 )
	{
		uint32_t word = __builtin_bswap32(*(uint32_t*)(Data + i));
		
		if( (word & 0xFC000003) ==  0x48000001 )
			FunctionPattern->FCalls++;

		if( (word & 0xFC000003) ==  0x48000000 )
			FunctionPattern->Branch++;
		if( (word & 0xFFFF0000) ==  0x40800000 )
			FunctionPattern->Branch++;
		if( (word & 0xFFFF0000) ==  0x41800000 )
			FunctionPattern->Branch++;
		if( (word & 0xFFFF0000) ==  0x40810000 )
			FunctionPattern->Branch++;
		if( (word & 0xFFFF0000) ==  0x41820000 )
			FunctionPattern->Branch++;
		
		if( (word & 0xFC000000) ==  0x80000000 )
			FunctionPattern->Loads++;
		if( (word & 0xFF000000) ==  0x38000000 )
			FunctionPattern->Loads++;
		if( (word & 0xFF000000) ==  0x3C000000 )
			FunctionPattern->Loads++;

		if( (word & 0xFC000000) ==  0x90000000 )
			FunctionPattern->Stores++;
		if( (word & 0xFC000000) ==  0x94000000 )
			FunctionPattern->Stores++;

		if( (word & 0xFF000000) ==  0x7C000000 )
			FunctionPattern->Moves++;
		if( word == 0x4E800020 )
			break;
	}
	FunctionPattern->Length = i;
}

int CPattern( FuncPattern *FPatA, FuncPattern *FPatB  )
{
	return ( memcmp( FPatA, FPatB, sizeof(uint32_t) * 6 ) == 0 );
}

static void waitforenter(void)
{
	if(confirm)
	{
		puts("Press enter to exit");
		getc(stdin);
	}
}

static void printerr(char *msg)
{
	puts(msg);
	waitforenter();
}

int main(int argc, char *argv[])
{
	puts("GetExtType Patcher v1.1 by FIX94");
	if(argc < 2)
	{
		puts("No input file specified!");
		puts("Usage: GetExtType <filename> <-nc>");
		puts("-nc - Dont wait for enter press on error/exit");
		waitforenter();
		return -1;
	}
	if(argc > 2 && memcmp(argv[2],"-nc",4) == 0)
		confirm = false;
	FILE *f = fopen(argv[1],"rb+");
	if(!f)
	{
		printerr("Unable to open input file!");
		return -2;
	}
	fseek(f,0,SEEK_END);
	size_t fsize = ftell(f);
	fseek(f,0,SEEK_SET);
	uint8_t *buf = malloc(fsize);
	fread(buf,1,fsize,f);
	if(memcmp(buf,dolstart,4) != 0)
	{
		free(buf);
		printerr("Input is not a dol file!");
		return -3;
	}
	puts("Read in dol");
	bool patched = false;
	size_t i;
	FuncPattern func;
	for(i = 0x100; i < fsize; i+=4)
	{
		if(__builtin_bswap32(*(uint32_t*)(buf+i)) != 0x4E800020)
			continue;
		i+=4;
		MPattern(buf+i, 0x2000, &func);
		if(CPattern(&GetExtTypeA, &func) || CPattern(&GetExtTypeB, &func))
		{
			if (((__builtin_bswap32(*(uint32_t*)(buf+i+0x40)) & 0xFFE00000) == 0x88000000) && // lbz r0, x(rX)
				((__builtin_bswap32(*(uint32_t*)(buf+i+0x48)) & 0xFFE00000) == 0x88000000))   // lbz r0, x(rX)
			{
				*(uint32_t*)(buf+i+0x40) = __builtin_bswap32(0x38000001); // li r0, 1
				*(uint32_t*)(buf+i+0x48) = __builtin_bswap32(0x38000001); // li r0, 1
				if(func.Length == 0xD4)
					puts("GetExtTypeA patched");
				else
					puts("GetExtTypeB patched");
				patched = true;
				break;
			}
		}
		else if(CPattern(&GetExtTypeC, &func))
		{
			if (((__builtin_bswap32(*(uint32_t*)(buf+i+0x38)) & 0xFFE00000) == 0x88000000) && // lbz r0, x(rX)
				((__builtin_bswap32(*(uint32_t*)(buf+i+0x44)) & 0xFFE00000) == 0x88000000))   // lbz r0, x(rX)
			{
				*(uint32_t*)(buf+i+0x38) = __builtin_bswap32(0x38000001); // li r0, 1
				*(uint32_t*)(buf+i+0x44) = __builtin_bswap32(0x38000001); // li r0, 1
				puts("GetExtTypeC patched");
				patched = true;
				break;
			}
		}
	}
	if(patched)
	{
		fseek(f,0,SEEK_SET);
		fwrite(buf,1,fsize,f);
		puts("Wrote back into dol");
	}
	else
		puts("Nothing found to patch!");
	free(buf);
	fclose(f);
	puts("All Done!");
	waitforenter();
	return 0;
}
