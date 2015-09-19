#include <stdio.h>
#include <iomanip>
#include "PLRUtree.h"
#pragma once

#define LINESIZE 4

struct CacheLine
{
	std::uint32_t tag;
	byte data[LINESIZE];
	bool valid, dirty;
	

	CacheLine() { }
	CacheLine(std::uint32_t tag, byte* data, bool valid, bool dirty)
		: tag(tag), valid(valid), dirty(dirty)
	{
		for (int i = 0; i < LINESIZE; ++i)
			this->data[i] = data[i];
	}
};

template<std::uint32_t size, std::uint32_t assoc>
class Cache
{
public:
	CacheLine cache[size][assoc];
	PLRUtree trees[size];

public:
	Cache()
	{
		byte data[LINESIZE] = {};
		for (int i = 0; i < size; ++i)
		{
			trees[i] = PLRUtree(assoc);
			for (int j = 0; j < assoc; ++j)
				cache[i][j] = CacheLine(0, data, false, false);
		}
	}
	~Cache() { }

	template<typename T>
	T ReadData(std::uint32_t address)
	{
		byte data[sizeof(T)];
		ReadData(address, sizeof(T), data);
		T* result = reinterpret_cast<T*>(data);
		return *result;
	}

	template<typename T>
	void WriteData(std::uint32_t address, T value)
	{
		WriteData(address, sizeof(T), reinterpret_cast<byte*>(&value));
	}

	void ReadData(std::uint32_t address, std::uint32_t nrOfBytes, byte* result)
	{
		if (address % nrOfBytes != 0) // not aligned, should never happen
		{
			std::cout << "UNALIGNED READ ATTEMPT" << std::endl;
			return;
		}

		std::uint32_t offset, index, tag;
		AddressToOffsetIndexTag(address, offset, index, tag);

		CacheLine* line = FindCacheLine(index, tag);
		if (line == nullptr)
			return; // TODO: relay to other cache/RAM

		for (std::uint32_t i = 0; i < nrOfBytes; ++i) // copy found data
			result[i] = line->data[offset + i];
	}

	void WriteData(std::uint32_t address, std::uint32_t nrOfBytes, byte* value)
	{
		if (address % nrOfBytes != 0) // not aligned, should never happen
		{
			// BIG FAIL CODE HERE
			return;
		}

		std::uint32_t offset, index, tag;
		AddressToOffsetIndexTag(address, offset, index, tag);

		CacheLine* line = FindCacheLine(index, tag);
		if (line == nullptr) // data wasn't in cache
		{
			CacheLine* row = cache[index];
			for (std::uint32_t i = 0; i < assoc; ++i) // find open slot to write to
				if (!row[i].valid)
				{
					line = &row[i];
					trees[index].setPath(i);
					break;
				}

			if (line == nullptr) // no open slots
			{
				std::uint32_t evict = trees[index].getOverwriteTarget();
				line = &row[evict];
			}
		}

		for (std::uint32_t i = 0; i < nrOfBytes; ++i) // write the data
			line->data[offset + i] = value[i];

		line->dirty = true;
		line->valid = true;
	}

	void Print() const
	{
		for (int i = 0; i < size; ++i)
		{
			for (int j = 0; j < assoc; ++j)
			{
				std::cout << "(0x" << std::hex << setfill('0') << setw(8) << cache[i][j].tag << std::dec << ", 0x";
				for (int k = LINESIZE - 1; k >= 0; --k)
					std::cout << std::hex << setfill('0') << setw(2) << (int)cache[i][j].data[k];
				std::cout << std::dec << ", " << cache[i][j].valid << ", " << cache[i][j].dirty << ")" << std::endl;
			}
			std::cout << std::endl;
		}
	}

private:
	void AddressToOffsetIndexTag(std::uint32_t address, std::uint32_t& offset, std::uint32_t& index, std::uint32_t& tag) const
	{
		std::uint32_t offsetBits = 0, indexBits = 0;
		for (std::uint32_t i = LINESIZE; i != 1; i >>= 1, ++offsetBits); // determine number of bits in offset
		for (std::uint32_t i = size; i != 1; i >>= 1, ++indexBits); // determine number of bits in index

		offset = address & (LINESIZE - 1);
		index = (address >> offsetBits) & (size - 1);
		tag = (address >> (offsetBits + indexBits));
	}

	CacheLine* FindCacheLine(std::uint32_t index, std::uint32_t tag)
	{
		CacheLine* row = cache[index];
		CacheLine* line = nullptr;
		for (std::uint32_t i = 0; i < assoc; ++i) // find correct column
			if (row[i].valid && row[i].tag == tag)
			{
				line = &row[i]; // found line containing our data
				trees[index].setPath(i);
				break;
			}

		return line;
	}
};

int READ( int* );
void WRITE( int*, int );