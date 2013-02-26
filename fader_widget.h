
// This is the header for our custom fader.
// We override the on_expose_event function so GTK calls into this class
// to draw the fader

#ifndef FADER
#define FADER

#include <gtkmm/drawingarea.h>
#include <iostream>
#include "round_rect.h"
#include "filter_widget.h"

using namespace std;

// LV2UI stuff
#include "lv2/lv2plug.in/ns/extensions/ui/ui.h"

class fader : public Gtk::DrawingArea
{
  public:
    
    // public Lv2 communication stuff
    LV2UI_Controller controller;
    LV2UI_Write_Function write_function;
    
    fader();
    ~fader();

	Gdk::Color top_colour;
	Gdk::Color bottom_colour;

    void set_label(string);
    void set_value(float);
    void set_rounded(int);
    void set_snap(bool);
    void set_min(float);
    void set_max(float);
    void show_route_dest();
    bool invert;
    int pos_mode;

    int route_max;

    int port_number;
    int route_number;
    int route_port_number;

    bool filter_widget_connect;
    filter_widget* filter_widget_object;

  protected:

	// stores frequency we're currently at.
	float fader_value,value,min,max;
	bool drag;
	bool snap;
	int rounded;
	string label;    

	bool on_expose_event(GdkEventExpose* event);
	bool on_button_press_event(GdkEventButton* event);
	virtual bool on_button_release_event(GdkEventButton*);
	virtual bool on_motion_notify_event(GdkEventMotion*);
	void draw_slider(int,int);
};

#endif



