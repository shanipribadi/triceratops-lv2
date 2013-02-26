
#include "nix-echo.hpp"

//------------------------------------------------------------------------------------
// INITIALISE
nixecho::nixecho()
{
	float newFloat = 0;
	buffer_left.resize ( 1024*64, newFloat );
	buffer_right.resize ( 1024*64, newFloat );

	old_in_left = 0;
	old_in_right = 0;

	for (int x=0; x<buffer_left.size(); x++)
	{
		buffer_left[x] = 0;
		buffer_right[x] = 0;
	}

	play_head = 1024 * 32;
	delay_left = 0;
	stereo_width = 64;
	delay_right = stereo_width;

	eq = new EQSTATE();
}

//------------------------------------------------------------------------------------
// DECONSTRUCTOR
nixecho::~nixecho()
{
}

//------------------------------------------------------------------------------------
// EMPTY BUFFER
void nixecho::clear_buffer()
{
	for (int x=0; x<buffer_left.size(); x++)
	{
		buffer_left[x] = 0;
		buffer_right[x] = 0;
	}
}

//------------------------------------------------------------------------------------

void nixecho::set_sample_rate(double s_rate)
{
	init_3band_state(eq,880,5000,s_rate);		
	eq->lg = 0.0; // BASS
	eq->mg = 1.2; // MIDS
	eq->hg = 0.0; // HIGHS 
}

//------------------------------------------------------------------------------------
// CLEAR BUFFER AND RESET ECHO
void nixecho::reset()
{


	for (int x=0; x<buffer_left.size(); x++)
	{
		buffer_left[x] = 0;
		buffer_right[x] = 0;
	}
}

//------------------------------------------------------------------------------------
// DO ECHO STUFF - left

double nixecho::do_left(double in)
{

	if (std::isnan(in)) {in=0; feedback_left = 0; }

	buffer_left[play_head] = feedback_left;

 	play_head++;
	if (play_head >= echo_speed) {play_head = 0; }

	delay_left++;
	if (delay_left >= echo_speed) {delay_left = 0; }

	feedback_left = in + do_3band(eq, buffer_left[delay_left] * echo_decay);
		
	return feedback_left;

}

//------------------------------------------------------------------------------------
// DO ECHO STUFF - right

double nixecho::do_right(double in)
{
	if (std::isnan(in)) {in=0; feedback_right = 0; }

	buffer_right[play_head] = feedback_right;

	delay_right++;
	if (delay_right >= echo_speed) {delay_right = 0; }

	feedback_right = in + do_3band(eq, buffer_right[delay_right] * echo_decay);

	return feedback_right;

}







