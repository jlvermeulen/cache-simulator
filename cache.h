#include <stdio.h>
#include <iomanip>
#include "PLRUtree.h"

#pragma once

#define LINESIZE 64

struct CacheLine
{
	std::uintptr_t tag;
	byte data[LINESIZE];
	bool valid, dirty;
	int set; // the set this cache line is stored in

	CacheLine() { }

	CacheLine(const CacheLine& cl)
	{
		tag = cl.tag;
		valid = cl.valid;
		dirty = cl.dirty;
		set = cl.set;

		for (int i = 0; i < LINESIZE; ++i)
			data[i] = cl.data[i];
	}

	CacheLine(std::uintptr_t tag, byte* data, bool valid, bool dirty)
		: tag(tag), valid(valid), dirty(dirty)
	{
		for (int i = 0; i < LINESIZE; ++i)
			this->data[i] = data[i];
	}
};

// base class with public interface for access between cache levels
class CacheBase
{
public:
	int reads = 0, writes = 0, writemisses = 0, readmisses = 0, evicts = 0; // counters for stats
	int offsetBits = 0, indexBits = 0; // number of bits in offset and index for this cache

	virtual byte* ReadData(std::uintptr_t address) = 0;
	virtual void WriteData(std::uintptr_t address, int nrOfBytes, byte* data) = 0;
};

template<std::uint32_t size, std::uint32_t assoc>
class Cache : public CacheBase
{
public:
	Cache(CacheBase* nl = nullptr)
	{
		nextLevel = nl;
		byte data[LINESIZE] = {};
		for (int i = 0; i < size; ++i)
		{
			trees[i] = PLRUtree(assoc);
			for (int j = 0; j < assoc; ++j)
				cache[i][j] = CacheLine(0, data, false, false);
		}

		for (int i = LINESIZE; i != 1; i >>= 1, ++offsetBits); // determine number of bits in offset
		for (int i = size; i != 1; i >>= 1, ++indexBits); // determine number of bits in index
	}

	~Cache() { }

	// read data of type T at address
	template<typename T>
	T ReadData(std::uintptr_t address)
	{
		std::uintptr_t offset, index, tag;
		AddressToOffsetIndexTag(address, offset, index, tag);

		byte data[sizeof(T)];
		byte* line = ReadData(address);
		for (int i = 0; i < sizeof(T); ++i)
			data[i] = line[offset + i];

		return *reinterpret_cast<T*>(data);
	}

	// write data of type T to address
	template<typename T>
	void WriteData(std::uintptr_t address, T value)
	{
		WriteData(address, sizeof(T), reinterpret_cast<byte*>(&value));
	}

	// prints all the data in the cache to console
	void Print() const
	{
		for (int i = 0; i < size; ++i)
		{
			for (int j = 0; j < assoc; ++j)
			{
				std::cout << "(0x" << std::hex << setfill('0') << setw(sizeof(cache[i][j].tag) * 2) << cache[i][j].tag << std::dec << ", 0x";
				for (int k = LINESIZE - 1; k >= 0; --k)
					std::cout << std::hex << setfill('0') << setw(sizeof(cache[i][j].data[k]) * 2) << (int)cache[i][j].data[k];
				std::cout << std::dec << ", " << cache[i][j].valid << ", " << cache[i][j].dirty << ")" << std::endl;
			}
			std::cout << std::endl;
		}
	}

private:
	CacheLine cache[size][assoc]; // the data
	CacheBase* nextLevel; // pointer to next cache level, nullptr if next level is RAM
	PLRUtree trees[size]; // the trees for PLRU eviction

	// get cacheline data containing address
	byte* ReadData(std::uintptr_t address)
	{
		reads++;

		std::uintptr_t offset, index, tag;
		AddressToOffsetIndexTag(address, offset, index, tag);

		CacheLine* line = FindData(address);
		if (line == nullptr) // data not in cache yet
		{
			line = LoadData(address);
			readmisses++;
		}

		trees[index].setPath(line->set); // update eviction policy
		return line->data;
	}

	// write data to cache at address
	void WriteData(std::uintptr_t address, int nrOfBytes, byte* data)
	{
		writes++;

		std::uintptr_t offset, index, tag;
		AddressToOffsetIndexTag(address, offset, index, tag);

		CacheLine* line = FindData(address);
		if (line == nullptr) // data not in cache yet
		{
			line = LoadData(address);
			writemisses++;
		}

		trees[index].setPath(line->set); // update eviction policy
		for (int i = 0; i < nrOfBytes; ++i)
			line->data[offset + i] = data[i];

		line->dirty = true;
	}

	CacheLine* FindData(std::uintptr_t address)
	{
		std::uintptr_t offset, index, tag;
		AddressToOffsetIndexTag(address, offset, index, tag);

		CacheLine* row = cache[index];
		for (int i = 0; i < assoc; ++i) // check all sets
			if (row[i].valid && row[i].tag == tag)
				return &row[i];

		return nullptr;
	}

	// makes sure data at address is in cache and returns address of line
	// use only when data is not in cache!
	CacheLine* LoadData(std::uintptr_t address)
	{
		std::uintptr_t offset, index, tag;
		AddressToOffsetIndexTag(address, offset, index, tag);

		byte data[LINESIZE];
		if (nextLevel == nullptr) // retrieve from RAM
		{
			std::uintptr_t lineStart = address - (address % LINESIZE);
			for (int i = 0; i < LINESIZE; ++i)
				data[i] = ReadFromRAM<byte>(reinterpret_cast<byte*>(lineStart + i));
		}
		else // retrieve from higher level cache
		{
			byte* cacheData = nextLevel->ReadData(address);
			for (int i = 0; i < LINESIZE; ++i)
				data[i] = cacheData[i];
		}

		CacheLine cl(tag, data, true, false);

		CacheLine* row = cache[index];
		for (int i = 0; i < assoc; ++i) // find an open slot, if it exists
			if (!row[i].valid)
			{
				cl.set = i;
				row[i] = cl;
				return &row[i];
			}

		// no room left in set, evict something
		int evict = trees[index].getOverwriteTarget();
		if (row[evict].dirty) // need to write evicted data to higher level
		{
			// reconstruct address of first byte in evicted cache line
			std::uint32_t oldAddress = 0;
			oldAddress += row[evict].tag << (offsetBits + indexBits);
			oldAddress += index << offsetBits;

			if (nextLevel == nullptr) // evict to RAM
				for (int i = 0; i < LINESIZE; ++i)
					WriteToRAM<byte>(reinterpret_cast<byte*>(oldAddress + i), row[evict].data[i]);
			else // evict to higher cache level
				nextLevel->WriteData(oldAddress, LINESIZE, row[evict].data);

			evicts++;
		}

		cl.set = evict;
		row[evict] = cl; // put line in cache
		return &row[evict];
	}

	// split address into offset, index and tag
	void AddressToOffsetIndexTag(std::uintptr_t address, std::uintptr_t& offset, std::uintptr_t& index, std::uintptr_t& tag) const
	{
		offset = address & (LINESIZE - 1);
		index = (address >> offsetBits) & (size - 1);
		tag = (address >> (offsetBits + indexBits));
	}
};