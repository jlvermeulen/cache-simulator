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
	

	CacheLine() { }
	CacheLine(std::uintptr_t tag, byte* data, bool valid, bool dirty)
		: tag(tag), valid(valid), dirty(dirty)
	{
		for (int i = 0; i < LINESIZE; ++i)
			this->data[i] = data[i];
	}
};

class CacheBase
{
public:
	int offsetBits = 0, indexBits = 0;

	virtual CacheLine* GetLineForWrite(std::uintptr_t index, std::uintptr_t tag, std::uintptr_t address) = 0;

	virtual void WriteData(CacheLine* cl, std::uintptr_t offset, std::uint32_t nrOfBytes, byte* value) = 0;

	virtual CacheLine* PlaceLine(std::uintptr_t index, CacheLine cl) = 0;

	virtual CacheLine ReadData(std::uintptr_t address) = 0;

	virtual CacheLine* FindCacheLine(std::uintptr_t index, std::uintptr_t tag) = 0;
};

template<std::uint32_t size, std::uint32_t assoc>
class Cache : public CacheBase
{
public:
	PLRUtree trees[size];
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

	template<typename T>
	T ReadData(std::uintptr_t address)
	{
		byte data[sizeof(T)];
		CacheLine l = ReadData(address);// , sizeof(T), data);
		std::uintptr_t tag, index, offset;
		AddressToOffsetIndexTag(address, offset, index, tag);

		for (int i = 0; i < sizeof(T); ++i)
			data[i] = l.data[offset + i];
		T* result = reinterpret_cast<T*>(data);
		return *result;
	}

	template<typename T>
	void WriteData(std::uintptr_t address, T value)
	{
		std::uintptr_t tag, index, offset;
		AddressToOffsetIndexTag(address, offset, index, tag);

		CacheLine* line = GetLineForWrite(index, tag, address);

		WriteData(line, offset, sizeof(T), reinterpret_cast<byte*>(&value));
	}

	CacheLine* GetLineForWrite(std::uintptr_t index, std::uintptr_t tag, std::uintptr_t address)
	{
		CacheLine* line = FindCacheLine(index, tag);
		if (line == nullptr)
		{
			CacheLine cl;
			
			if (nextLevel == nullptr)
			{
				byte data[LINESIZE] = {};
				cl = CacheLine(tag, data, true, false);
			}
			else
				cl = nextLevel->ReadData(address);
			line = PlaceLine(index, cl);
		}
		return line;
	}
	
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

	void WriteData(CacheLine* cl, std::uintptr_t offset, std::uint32_t nrOfBytes, byte* value)
	{
		for (std::uint32_t i = 0; i < nrOfBytes; ++i) // write the data
			cl->data[offset + i] = value[i];

		cl->dirty = true;
	}

	CacheLine* PlaceLine(std::uintptr_t index, CacheLine cl)
	{
		CacheLine* line = nullptr;
		CacheLine* row = cache[index];
		for (std::uint32_t i = 0; i < assoc; ++i) // find open slot to write to
			if (!row[i].valid)
			{
				row[i] = cl;
				line = &row[i];
				trees[index].setPath(i);
				break;
			}

		if (line == nullptr) // no open slots
		{
			std::uint32_t evict = trees[index].getOverwriteTarget();
			if (row[evict].dirty)
			{
				std::uint32_t oldaddress = 0;
				oldaddress = index << offsetBits;
				oldaddress += row[evict].tag << (offsetBits + indexBits);
				line = nextLevel->GetLineForWrite(index, row[evict].tag, oldaddress);
				nextLevel->WriteData(line, 0,LINESIZE, row[evict].data);
			}
			row[evict] = cl;
			line = &row[evict];
			trees[index].setPath(evict);
		}

		return line;
	}

	CacheLine ReadData(std::uintptr_t address)
	{
		std::uintptr_t offset, index, tag;
		AddressToOffsetIndexTag(address, offset, index, tag);

		CacheLine* line = FindCacheLine(index, tag);
		if (line == nullptr) // not in cache, relay to higher level
		{
			CacheLine l;
			if (nextLevel == nullptr)
			{
				byte data[LINESIZE];
				std::uintptr_t lineStart = address - (address % LINESIZE); // start of cache line in RAM
				for (int i = 0; i < LINESIZE; i++)
					data[i] = ReadFromRAM<byte>(reinterpret_cast<byte*>(lineStart + i));

				l = CacheLine(tag, data, true, false);
			}
			else
				CacheLine l = nextLevel->ReadData(address); // read from next level
			return *PlaceLine(index, l); // store in cache
		}

		return *line;
	}

private:
	CacheLine cache[size][assoc];
	CacheBase* nextLevel;

	void AddressToOffsetIndexTag(std::uintptr_t address, std::uintptr_t& offset, std::uintptr_t& index, std::uintptr_t& tag) const
	{
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