

#ifndef PRESETSELECT
#define PRESETSELECT

#include <gtkmm/drawingarea.h>
#include <iostream>
#include <string>
#include <sstream>

using namespace std;

// LV2UI stuff
#include "lv2/lv2plug.in/ns/extensions/ui/ui.h"

#include "dco_gui.h"
#include "lfo_gui.h"
#include "unison_gui.h"
#include "adsr_gui.h"
#include "adsr_lfo_gui.h"
#include "amp_gui.h"
#include "echo_gui.h"
#include "reverb_gui.h"
#include "modifier_gui.h"

class preset_select : public Gtk::DrawingArea
{
	public:

	vector<string>preset_file_names;
    
	// public Lv2 communication stuff
	LV2UI_Controller controller;
	LV2UI_Write_Function write_function;

	Gdk::Color top_colour;
	Gdk::Color bottom_colour;

	int current_preset;
	int current_preset_offset;

	dco_gui* dco1;
	dco_gui* dco2;
	dco_gui* dco3;
	unison_gui* unison;

	lfo_gui* lfo1;
	lfo_gui* lfo2;
	lfo_gui* lfo3;

	adsr_gui* adsr_amp;
	adsr_gui* adsr_filter;
	adsr_lfo_gui* adsr_lfo;

	amp_gui* amp_and_filter;
	echo_gui* echo;
	reverb_gui* reverb;
	modifier_gui* modifier;
    
	preset_select();
	~preset_select();
	void position_top(bool);
	int pos_mode;

	void set_text(string);
	void set_value(int);
	int get_symbol_port(string);
	void load_preset(int);
	void set_gui_widget(int,float);
	void get_preset_list(string);
	void get_preset_list_local(string);
	void create_new_preset();
	void init();

	int port_number;

  protected:

	int val;
	string text;    
	stringstream text_stream; 
    	float grad_top_colour;
    	float grad_bottom_colour;

	virtual bool on_expose_event(GdkEventExpose*);
	virtual bool on_button_press_event(GdkEventButton*);
	virtual bool on_button_release_event(GdkEventButton*);
	virtual bool on_key_press_event(GdkEventKey* eventData); 
};

#endif



