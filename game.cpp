#include "precomp.h"
#include "cache.h"

// -----------------------------------------------------------
// Map access
// -----------------------------------------------------------
int Map::Get( int x, int y ) 
{ 
	return READ( &map[x + y * 513] ); 
}

void Map::Set( int x, int y, int v ) 
{ 
	WRITE( &map[x + y * 513], v ); 
}

#define READ_SIZE 1

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

	std::uint32_t address = 0x15;
	std::uint32_t tag = address >> 4;

	Cache<4, 1> cache;
	CacheLine one, two, three, four;

	one.tag = tag;
	one.data[0] = 0x1;
	one.data[1] = 0x2;
	one.data[2] = 0x3;
	one.data[3] = 0x4;
	one.valid = true;
	one.dirty = false;

	two.tag = tag;
	two.data[0] = 0x1;
	two.data[1] = 0x2;
	two.data[2] = 0x3;
	two.data[3] = 0x4;
	two.valid = true;
	two.dirty = false;

	three.tag = tag;
	three.data[0] = 0x1;
	three.data[1] = 0x2;
	three.data[2] = 0x3;
	three.data[3] = 0x4;
	three.valid = true;
	three.dirty = false;

	four.tag = tag;
	four.data[0] = 0x1;
	four.data[1] = 0x2;
	four.data[2] = 0x3;
	four.data[3] = 0x4;
	four.valid = true;
	four.dirty = false;

	cache.cache[0][0] = one;
	cache.cache[1][0] = two;
	cache.cache[2][0] = three;
	cache.cache[3][0] = four;

	cache.Print();

	byte result[READ_SIZE];
	cache.ReadData(address, READ_SIZE, result);
	std::cout << "Reading cache at 0x" << std::hex << setfill('0') << setw(8) << address << std::dec << std::endl;
	std::cout << "Value: 0x" << std::hex;
	for (int i = READ_SIZE - 1; i >= 0; i--)
		std::cout << setfill('0') << setw(2) << (int)result[i];
	std::cout << std::dec << std::endl;
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