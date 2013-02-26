
#include "fastmaths.hpp"


//------------------------------------------------------------------------------------------------
// APPROX POW

double fast_pow(double a, double b) {
  union {
    double d;
    int x[2];
  } u = { a };
  u.x[1] = (int)(b * (u.x[1] - 1072632447) + 1072632447);
  u.x[0] = 0;
  return u.d;
}

//------------------------------------------------------------------------------------------------
// APPROX ATAN

double fast_atan( double x )
{
    return (x / (1.0 + 0.28 * (x * x)));
}

//------------------------------------------------------------------------------------------------
// WHITE NOISE

noise::noise()
{	
	
	for (int x=0; x<8192; ++x)
	{
		buffer[x] = ((float)(rand() % 65536)/32768)-1;
	}

	position = rand() % 8190;
	endpos = rand() % 8190;
	speed = rand() % 4 - rand() % 8;
	if (speed==0) {speed=1; }
}

float noise::tick()
{
	position+=speed;
	if (position < 0 || position > endpos)
	{ 
		endpos = rand() % 8190;
		position = rand() % 8190;
		speed = rand() % 4 - rand() % 8;
		if (speed==0) {speed=1; }
	}
	return buffer[position];
}

noise::~noise()
{
}

//-----------------------------------------------------------------------------------------------
// INERTIA

inertia::inertia()
{
	value = 440;
	speed = 0;
}

inertia::~inertia()
{

}

void inertia::set_speed(double new_speed)
{
	speed = new_speed;
}

double inertia::slide(double new_value)
{

	if (speed==50) { value = new_value; return value; }

	if (value == new_value) { return value; }

	if (value < new_value)
	{
		value+=speed*4;
		if (value > new_value) { value = new_value; }
	}

	if (value > new_value)
	{
		value-=speed*4;
		if (value < new_value) {value = new_value; }
	}

	return value;
}




