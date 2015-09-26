#include "precomp.h"
#include "cache.h"
#include <iostream>
#include <string>

Cache<16, 4> l3;
Cache<8, 4> l2(&l3);
Cache<4, 4> l1(&l2);

template<typename T>
T READ(std::uintptr_t address)
{
	return l1.ReadData<T>(address);
	// prevent ReadFromRAM using caching
	return ReadFromRAM<T>(reinterpret_cast<T*>(address));
}

template<typename T>
void WRITE(std::uintptr_t address, T value)
{
	l1.WriteData(address, value);
	// prevent WriteToRAM using caching
	//WriteToRAM<T>(reinterpret_cast<T*>(address), value);
}

// -----------------------------------------------------------
// Map access
// -----------------------------------------------------------
int Map::Get( int x, int y ) 
{
	return READ<int>(reinterpret_cast<std::uintptr_t>(&map[x + y * 513]));
}

void Map::Set( int x, int y, int v )
{
	WRITE<int>(reinterpret_cast<std::uintptr_t>(&map[x + y * 513]), v);
}

#define READ_SIZE 2

// -----------------------------------------------------------
// Initialize the application
// -----------------------------------------------------------
void Game::Init()
{
	screen->Clear( 0 );
	// initialize recursion stack
	map.Init();
	taskPtr = 0;
	Push( 0, 0, 512, 512, 256 );

//	std::uintptr_t address = 0x06;
//	std::uintptr_t tag = address >> 4;
//	Cache<4, 4> cache2;
//	Cache<4, 4> cache(&cache2);
//	//cache.EvictWrite = &Cache<4, 4>::WriteData2;
//	//cache.FallbackRead = &Cache<4, 4>::ReadData2;
//
//	std::string ans;
//	std::uint32_t value = 0x04030201;
//	//cache.WriteData<std::uint32_t>(0x04, value);
////	cache.WriteData<std::uint32_t>(0x08, value);
////	cache.WriteData<std::uint32_t>(0x0C, value);
////	std::cout << "Level 1 cache:\n";
////	cache.Print();
////	std::cout << std::endl;
////	std::cout << "Level 2 cache:\n";
////	cache2.Print();
////	std::getline(std::cin, ans);
//
//	cache.WriteData<std::uint32_t>(0x00, 0x04030201);
//	std::cout << "Level 1 cache:\n";
//	cache.Print();
//	std::cout << std::endl;
//	std::cout << "Level 2 cache:\n";
//	cache2.Print();
//	//std::getline(std::cin, ans);
//
//	cache.WriteData<std::uint32_t>(0x10, 0x04030202);
//	std::cout << "Level 1 cache:\n";
//	cache.Print();
//	std::cout << std::endl;
//	std::cout << "Level 2 cache:\n";
//	cache2.Print();
////	std::getline(std::cin, ans);
//
//	cache.WriteData<std::uint32_t>(0x20, 0x04030203);
//	std::cout << "Level 1 cache:\n";
//	cache.Print();
//	std::cout << std::endl;
//	std::cout << "Level 2 cache:\n";
//	cache2.Print();
////	std::getline(std::cin, ans);
//
//	cache.WriteData<std::uint32_t>(0x30, 0x04030204);
//	std::cout << "Level 1 cache:\n";
//	cache.Print();
//	std::cout << std::endl;
//	std::cout << "Level 2 cache:\n";
//	cache2.Print();
//	//std::getline(std::cin, ans);
//
//	cache.WriteData<std::uint32_t>(0x40, 0x04030205);
//	//std::uint32_t r = cache.ReadData<std::uint32_t>(0x00);
//
//	std::cout << "Level 1 cache:\n";
//	cache.Print();
//	std::cout << std::endl;
//	std::cout << "Level 2 cache:\n";
//	cache2.Print();
//	//std::uint16_t r = cache.ReadData<std::uint16_t>(address);

	//std::cout << "Reading cache at 0x" << std::hex << setfill('0') << setw(sizeof(address) * 2) << address << std::dec << std::endl;
	//std::cout << "Value: 0x" << std::hex << setfill('0') << setw(sizeof(r) * 2) << r << std::dec << std::endl;
}

// -----------------------------------------------------------
// Recursive subdivision
// -----------------------------------------------------------
void Game::Subdivide( int x1, int y1, int x2, int y2, int scale )
{
	// termination
	if ((x2 - x1) == 1) return;
	// calculate diamond vertex positions
	int cx = (x1 + x2) / 2, cy = (y1 + y2) / 2;
	// set vertices
	if (map.Get( cx, y1 ) == 0) map.Set( cx, y1, (map.Get( x1, y1 ) + map.Get( x2, y1 )) / 2 + IRand( scale ) - scale / 2 );
	if (map.Get( cx, y2 ) == 0) map.Set( cx, y2, (map.Get( x1, y2 ) + map.Get( x2, y2 )) / 2 + IRand( scale ) - scale / 2 );
	if (map.Get( x1, cy ) == 0) map.Set( x1, cy, (map.Get( x1, y1 ) + map.Get( x1, y2 )) / 2 + IRand( scale ) - scale / 2 );
	if (map.Get( x2, cy ) == 0) map.Set( x2, cy, (map.Get( x2, y1 ) + map.Get( x2, y2 )) / 2 + IRand( scale ) - scale / 2 );
	if (map.Get( cx, cy ) == 0) map.Set( cx, cy, (map.Get( x1, y1 ) + map.Get( x2, y2 )) / 2 + IRand( scale ) - scale / 2 );
	// push new tasks
	Push( x1, y1, cx, cy, scale / 2 );
	Push( cx, y1, x2, cy, scale / 2 );
	Push( x1, cy, cx, y2, scale / 2 );
	Push( cx, cy, x2, y2, scale / 2 );
}

// -----------------------------------------------------------
// Main application tick function
// -----------------------------------------------------------
void Game::Tick( float _DT )
{
	for( int i = 0; i < 1024; i++ )
	{
		// execute one subdivision task
		if (taskPtr == 0) break;
		int x1 = task[--taskPtr].x1, x2 = task[taskPtr].x2;
		int y1 = task[taskPtr].y1, y2 = task[taskPtr].y2;
		Subdivide( x1, y1, x2, y2, task[taskPtr].scale );
	}
	// visualize
	Pixel* d = screen->GetBuffer() + (SCRWIDTH - 513) / 2 + ((SCRHEIGHT - 513) / 2) * screen->GetWidth();
	for( int y = 0; y < 513; y++ ) for( int x = 0; x < 513; x++ )
	{
		int c = CLAMP( map.Get( x, y ) / 2, 0, 255 );
		d[x + y * screen->GetWidth()] = c + (c << 8) + (c << 16);
	}
}