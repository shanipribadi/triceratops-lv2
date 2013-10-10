
#include <vector>
#include <cmath>
#include <iostream>
#include "eq.h"

class nixecho
{

public:
	nixecho();
	virtual ~nixecho();
	double do_left(double);
	double do_right(double);
	void clear_buffer();
	void set_sample_rate(double);
	void reset();
	int echo_speed;
	double echo_decay;
	EQSTATE* eq;
	EQSTATE* eq2;

	double feedback_left,feedback_right;

private:

	std::vector <float> buffer_left,buffer_right;

	int play_head,stereo_width;
	int delay_left,delay_right;
	int old_in_left,old_in_right;

};





