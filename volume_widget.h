
// This is the header for our custom volume.
// We override the on_expose_event function so GTK calls into this class
// to draw the volume

#ifndef VOLUMEWIDGET
#define VOLUMEWIDGET

#include <gtkmm/drawingarea.h>
#include <iostream>
#include "round_rect.h"

using namespace std;

// LV2UI stuff
#include "lv2/lv2plug.in/ns/extensions/ui/ui.h"

class volume : public Gtk::DrawingArea
{
  public:
    
    // public Lv2 communication stuff
    LV2UI_Controller controller;
    LV2UI_Write_Function write_function;

	Gdk::Color top_colour;
	Gdk::Color bottom_colour;
    
    volume();
    ~volume();
    void position_top(bool);
    int pos_mode;

    void set_label(string);
    void set_value(float);
    void set_rounded(int);
    void set_min(float);
    void set_max(float);

    int port_number;
    float volume_value,value,min,max;

  protected:

	// stores frequency we're currently at.

	bool drag;
	int rounded;
	string label;    
    	float grad_top_colour;
    	float grad_bottom_colour;

	bool on_expose_event(GdkEventExpose* event);
	bool on_button_press_event(GdkEventButton* event);
	virtual bool on_button_release_event(GdkEventButton*);
	virtual bool on_motion_notify_event(GdkEventMotion*);
	void draw_slider(int,int);
};

#endif


	
