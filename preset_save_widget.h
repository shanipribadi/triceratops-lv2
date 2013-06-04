
// This is the header for our custom preset_save.
// We override the on_expose_event function so GTK calls into this class
// to draw the preset_save

#ifndef preset_saveWIDGET
#define preset_saveWIDGET

#include <gtkmm/drawingarea.h>
#include <iostream>
#include <string>
#include <sstream>

using namespace std;

// LV2UI stuff
#include "lv2/lv2plug.in/ns/extensions/ui/ui.h"

class preset_save : public Gtk::DrawingArea
{
  public:
    
    // public Lv2 communication stuff
    LV2UI_Controller controller;
    LV2UI_Write_Function write_function;

	Gdk::Color top_colour;
	Gdk::Color bottom_colour;
    
    preset_save();
    ~preset_save();
    void position_top(bool);
    int pos_mode;

    void set_text(string);
    void set_value(int);

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



