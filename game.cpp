#include "precomp.h"
#include "cache.h"
#include <iostream>
#include <string>

// Based on Intel Core i7 4770K (Haswell) specs and Table 2-3 (page 35) of the
// Intel® 64 and IA-32 Architectures Optimization Reference Manual, September 2014
// L3 set associativity found at http://www.cpu-world.com/CPUs/Core_i7/Intel-Core%20i7-4770K.html
// L3 latency found at http://7-cpu.com/cpu/Haswell.html
#define L1LATENCY 4
#define L2LATENCY 12
#define L3LATENCY 36
#define RAMLATENCYCYCLES 36
#define RAMLATENCTNANOSECONDS 57


Cache<2048, 16> l3(nullptr, L3LATENCY); // 2MB, 16-way set associative (latency 36 cycles, 8MB over 4 cores)
Cache<512, 8> l2(&l3, L2LATENCY); // 256KB, 8-way set associative (fastest latency 12 cycles)
Cache<64, 8> l1(&l2, L1LATENCY); // 32KB, 8-way set associative (fastest latency 4 cycles)

template<typename T>
T READ(std::uintptr_t address)
{
	return l1.ReadData<T>(address);
}

template<typename T>
void WRITE(std::uintptr_t address, T value)
{
	l1.WriteData(address, value);
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

bool printed = false;
// -----------------------------------------------------------
// Main application tick function
// -----------------------------------------------------------
void Game::Tick( float _DT )
{
	for( int i = 0; i < 1024; i++ )
	{
		// execute one subdivision task
		if (taskPtr == 0)
		{
			if (!printed)
			{
				PrintStats();
				printed = true;
			}
			break;
		}
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

// -----------------------------------------------------------
// Print cache stats after program completion
// -----------------------------------------------------------
void Game::PrintStats()
{
	std::cout << "L1 cache stats" << std::endl;
	l1.PrintStats();
	std::cout << std::endl;
	std::cout << "L2 cache stats" << std::endl;
	l2.PrintStats();
	std::cout << std::endl;
	std::cout << "L3 cache stats" << std::endl;
	l3.PrintStats();
	std::cout << std::endl;
	//ram
	double latencyfromcycles = (l3.readmisses + l3.writemisses)*RAMLATENCYCYCLES / (double)CYCLESPERMILLISECOND;
	double latencyfromns = RAMLATENCTNANOSECONDS * (l3.readmisses + l3.writemisses) / 1000000.0;
	std::cout << "RAM latency: " << latencyfromcycles +latencyfromns << "ms"<<std::endl;
}