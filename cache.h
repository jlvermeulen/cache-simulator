#include <stdio.h>
#include <iomanip>

#pragma once

#define LINESIZE 4

struct CacheLine
{
	std::uint32_t tag;
	byte data[LINESIZE];
	bool valid, dirty;
};

template<std::uint32_t size, std::uint32_t assoc>
class Cache
{
public:
	CacheLine cache[size][assoc];

public:
	Cache() { }
	~Cache() { }

	bool ReadData(std::uint32_t address, std::uint32_t nrOfBytes, byte* value)
	{
		if (address % nrOfBytes != 0) // not aligned, should never happen
		{
			// BIG FAIL CODE HERE
			return false;
		}

		std::uint32_t offsetBits = 0, indexBits = 0;
		for (std::uint32_t i = LINESIZE; i != 1; i >>= 1, ++offsetBits); // determine number of bits in offset
		for (std::uint32_t i = size; i != 1; i >>= 1, ++indexBits); // determine number of bits in index

		std::uint32_t offset = address & (LINESIZE - 1);
		std::uint32_t index = (address >> offsetBits) & (size - 1);
		std::uint32_t tag = (address >> (offsetBits + indexBits));

		CacheLine* row = cache[index];
		CacheLine* line = nullptr;
		for (std::uint32_t i = 0; i < assoc; ++i) // find correct column
			if (row[i].valid && row[i].tag == tag)
			{
				line = &row[i]; // found line containing our data
				break;
			}

		if (line == nullptr)
			return false;

		for (std::uint32_t i = 0; i < nrOfBytes; ++i) // copy found data
			value[i] = line->data[offset + i];

		return true;
	}

	void Print()
	{
		for (int i = 0; i < size; ++i)
			for (int j = 0; j < assoc; ++j)
			{
				std::cout << "(0x" << std::hex << setfill('0') << setw(8) << cache[i][j].tag << std::dec << ", 0x";
				for (int k = LINESIZE - 1; k >= 0; --k)
					std::cout << std::hex << setfill('0') << setw(2) << (int)cache[i][j].data[k];
				std::cout << std::dec << ", " << cache[i][j].valid << ", " << cache[i][j].dirty << ")" << std::endl;
			}
	}
};

int READ( int* );
void WRITE( int*, int );