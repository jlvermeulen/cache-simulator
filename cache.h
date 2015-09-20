#include <stdio.h>
#include <iomanip>
#include "PLRUtree.h"
#pragma once

#define LINESIZE 4

struct CacheLine
{
	std::uintptr_t tag;
	byte data[LINESIZE];
	bool valid, dirty;
	

	CacheLine() { }
	CacheLine(std::uintptr_t tag, byte* data, bool valid, bool dirty)
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
	PLRUtree trees[size];
	Cache(Cache *nl = nullptr)
	{
		nextLevel = nl;
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
	T ReadData(std::uintptr_t address)
	{
		byte data[sizeof(T)];
		ReadData(address, sizeof(T), data);
		T* result = reinterpret_cast<T*>(data);
		return *result;
	}

	template<typename T>
	void WriteData(std::uintptr_t address, T value)
	{
		WriteData(address, sizeof(T), reinterpret_cast<byte*>(&value));
	}

	void Print() const
	{
		for (int i = 0; i < size; ++i)
			for (int j = 0; j < assoc; ++j)
			{
				std::cout << "(0x" << std::hex << setfill('0') << setw(sizeof(cache[i][j].tag) * 2) << cache[i][j].tag << std::dec << ", 0x";
				for (int k = LINESIZE - 1; k >= 0; --k)
					std::cout << std::hex << setfill('0') << setw(sizeof(cache[i][j].data[k]) * 2) << (int)cache[i][j].data[k];
				std::cout << std::dec << ", " << cache[i][j].valid << ", " << cache[i][j].dirty << ")" << std::endl;
			}
	}
	void(Cache::*FallbackRead)(std::uintptr_t, std::uint32_t, byte*);
	void(Cache::*EvictWrite)(std::uintptr_t, std::uint32_t, byte*);

	void WriteData(std::uintptr_t address, std::uint32_t nrOfBytes, byte* value)
	{
		if (address % nrOfBytes != 0) // not aligned, should never happen
		{
			std::cout << "UNALIGNED WRITE ATTEMPT" << std::endl;
			return;
		}

		std::uintptr_t offset, index, tag;
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
				if (row[evict].dirty)
					nextLevel->WriteData(index +row[evict].tag, nrOfBytes, row[evict].data);
				line = &row[evict];
			}
		}

		for (std::uint32_t i = 0; i < nrOfBytes; ++i) // write the data
			line->data[offset + i] = value[i];
		line->tag = tag;
		line->dirty = true;
		line->valid = true;
	}

	void ReadData(std::uintptr_t address, std::uint32_t nrOfBytes, byte* result)
	{
		if (address % nrOfBytes != 0) // not aligned, should never happen

		{
			std::cout << "UNALIGNED READ ATTEMPT" << std::endl;
			return;
		}

		std::uintptr_t offset, index, tag;
		AddressToOffsetIndexTag(address, offset, index, tag);

		CacheLine* line = FindCacheLine(index, tag);
		if (line == nullptr) // not in cache, relay to higher level
		{
			nextLevel->ReadData(address, nrOfBytes, result); // read from next level
			WriteData(address, nrOfBytes, result); // store in cache
			return;
		}

		for (std::uint32_t i = 0; i < nrOfBytes; ++i) // copy found data
			result[i] = line->data[offset + i];
	}

private:
	CacheLine cache[size][assoc];
	Cache *nextLevel;

	void AddressToOffsetIndexTag(std::uintptr_t address, std::uintptr_t& offset, std::uintptr_t& index, std::uintptr_t& tag) const
	{
		int offsetBits = 0, indexBits = 0;
		for (int i = LINESIZE; i != 1; i >>= 1, ++offsetBits); // determine number of bits in offset
		for (int i = size; i != 1; i >>= 1, ++indexBits); // determine number of bits in index

		offset = address & (LINESIZE - 1);
		index = (address >> offsetBits) & (size - 1);
		tag = (address >> (offsetBits + indexBits));
	}

	CacheLine* FindCacheLine(std::uintptr_t index, std::uintptr_t tag)
	{
		CacheLine* row = cache[index];
		CacheLine* line = nullptr;
		for (int i = 0; i < assoc; ++i) // find correct column
			if (row[i].valid && row[i].tag == tag)
			{
				line = &row[i]; // found line containing our data
				trees[index].setPath(i);
				break;
			}

		return line;
	}
};

template<typename T>
T READ(std::uintptr_t address)
{
	// prevent ReadFromRAM using caching
	return ReadFromRAM<T>(reinterpret_cast<T*>(address));
}

template<typename T>
void WRITE(std::uintptr_t address, T value)
{
	// prevent WriteToRAM using caching
	WriteToRAM<T>(reinterpret_cast<T*>(address), value);
}