#include "precomp.h"

int READ( int* address )
{
	// prevent ReadFromRAM using caching
	return ReadFromRAM( address );	
}

void WRITE( int* address, int value )
{
	// prevent WriteToRAM using caching
	WriteToRAM( address, value );
}