

#include <string>
#include <iostream>
#include <gtkmm.h>

// include the URI and global data of this plugin
#include "triceratops.hpp"

// these are our custom widget includes
#include "fader_widget.h"
#include "wave_widget.h"
#include "filter_widget.h"
#include "knob_widget.h"
#include "toggle_widget.h"
#include "spacer_widget.h"

using namespace std;

//--------------------------------------------------------

class adsr_lfo_gui
{
	public:

	adsr_lfo_gui(int,string,string);
	virtual ~adsr_lfo_gui();
	void set_controller(LV2UI_Controller,LV2UI_Write_Function);

	Gtk::HBox* tab;

	fader* gui_attack;
	fader* gui_decay;
	fader* gui_sustain;
	fader* gui_release;

	toggle* gui_lfo1;
	toggle* gui_lfo2;
	toggle* gui_lfo3;

	fader* gui_route1;
	fader* gui_route2;

	private:

};



