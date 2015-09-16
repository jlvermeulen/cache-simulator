#pragma once

namespace Tmpl8 {

struct Task
{
	int x1, y1, x2, y2, scale;
};

class Map
{
public:
	Map() : map( new int[513 * 513] ) {}
	void Init()
	{
		memset( map, 0, 513 * 513 * sizeof( int ) ); 
		Set( 0, 0, IRand( 256 ) );
		Set( 512, 0, IRand( 256 ) );
		Set( 0, 512, IRand( 256 ) );
		Set( 512, 512, IRand( 256 ) );
	}
	int Get( int x, int y );
	void Set( int x, int y, int v );
private:
	int* map;
};

class Surface;
class Game
{
public:
	void SetTarget( Surface* _Surface ) { screen = _Surface; }
	void Init();
	void Shutdown() {};
	void Push( int x1, int y1, int x2, int y2, int scale )
	{
		task[taskPtr].x1 = x1, task[taskPtr].x2 = x2;
		task[taskPtr].y1 = y1, task[taskPtr].y2 = y2;
		task[taskPtr++].scale = scale;
	}
	void GetMap( int x, int y );
	void SetMap( int x, int y );
	void Subdivide( int x1, int y1, int x2, int y2, int scale );
	void Tick( float _DT );
	void MouseUp( int _Button ) { /* implement if you want to detect mouse button presses */ }
	void MouseDown( int _Button ) { /* implement if you want to detect mouse button presses */ }
	void MouseMove( int _X, int _Y ) { /* implement if you want to detect mouse movement */ }
	void KeyUp( int _Key ) { /* implement if you want to handle keys */ }
	void KeyDown( int _Key ) { /* implement if you want to handle keys */ }
private:
	Surface* screen;
	Map map;
	Task task[512];
	int taskPtr;
};

}; // namespace Tmpl8