
// this file has the drawing code for our custom widget

#include "preset_save_widget.h"
#include <cairomm/context.h>

#include <iostream>



preset_save::preset_save()
{
	text = "";
	text_stream.str("");
	pos_mode = 0;

	set_can_focus(true);

	top_colour.set( "#550000");
	bottom_colour.set( "#220000");
  
	// make the click events call the on_button_press_event function
	set_events (Gdk::BUTTON_PRESS_MASK | Gdk::BUTTON_RELEASE_MASK | Gdk::KEY_PRESS_MASK);

	// make the click event do that function
	// signal_button_press_event().connect( sigc::mem_fun(*this, &preset_save::on_button_press_event) );
  
	set_size_request(40,10);
}

preset_save::~preset_save()
{
}

void preset_save::position_top(bool mode)
{
	if (mode)
	{
		grad_top_colour = 0.3;
		grad_bottom_colour = 0.1*2;
	}
	else
	{
		grad_top_colour = 0.1*2;
		grad_bottom_colour = 0.1;
	}
}

void preset_save::set_text(string text)
{
	text = text;
	queue_draw();
}

void preset_save::set_value(int _value)
{
	val = _value;	
	queue_draw();
}

bool preset_save::on_key_press_event(GdkEventKey* event)
{
	int key = event->keyval;

	cout << key << endl;

	if (key == 65293)
	{	
	}

	if (key == 65535 || key ==65288)
	{
		if (text.length() > 0)
		{
			text = text_stream.str().std::string::erase(text.length()-1);
			text_stream.str("");
			text_stream << text;
		}
	}
	
	if (key < 256 && key != 163) text_stream << (char)key;
	text = text_stream.str();
	
	queue_draw();
	
}

bool preset_save::on_button_press_event(GdkEventButton* event)
{
}

bool preset_save::on_button_release_event(GdkEventButton* event)
{	
	queue_draw();
  
  return true;
}

bool preset_save::on_expose_event(GdkEventExpose* event)
{
	// This is where we draw on the window
	Glib::RefPtr<Gdk::Window> window = get_window();

	if(window)
	{
		Gtk::Allocation allocation = get_allocation();
    		const int width = allocation.get_width();
    		const int height = allocation.get_height() - (allocation.get_height()/3);
    		const int height_offset = allocation.get_height()/6;

    		// coordinates for the center of the window
    		int xc, yc;
    		xc = width / 2;
    		yc = height / 2;

    		Cairo::RefPtr<Cairo::Context> cr = window->create_cairo_context();

    		// clip to the area indicated by the expose event so that we only redraw
    		// the portion of the window that needs to be redrawn
    		cr->rectangle(event->area.x, event->area.y,
            		event->area.width, event->area.height);
    		cr->clip();
    
   		 // background grad

     		Cairo::RefPtr<Cairo::LinearGradient> back_grad =
			Cairo::LinearGradient::create( 0,0,0,allocation.get_height() );

	switch (pos_mode)
	{
		case 0:
	        back_grad->add_color_stop_rgba(0,top_colour.get_red_p(),top_colour.get_green_p(),top_colour.get_blue_p(),1);
	        back_grad->add_color_stop_rgba(1,bottom_colour.get_red_p(),bottom_colour.get_green_p(),bottom_colour.get_blue_p(),1);
		break;

		case 1:
	        back_grad->add_color_stop_rgba(0,top_colour.get_red_p(),top_colour.get_green_p(),top_colour.get_blue_p(),1);
	        back_grad->add_color_stop_rgba(1,
			(bottom_colour.get_red_p() + top_colour.get_red_p())/2,
			(bottom_colour.get_green_p() + top_colour.get_green_p())/2,
			(bottom_colour.get_blue_p() + top_colour.get_blue_p())/2,
			1);
		break;

		case 2:
	        back_grad->add_color_stop_rgba(0,
			(bottom_colour.get_red_p() + top_colour.get_red_p())/2,
			(bottom_colour.get_green_p() + top_colour.get_green_p())/2,
			(bottom_colour.get_blue_p() + top_colour.get_blue_p())/2,
			1);
	        back_grad->add_color_stop_rgba(1,bottom_colour.get_red_p(),bottom_colour.get_green_p(),bottom_colour.get_blue_p(),1);
		break;
	}

		// fill background
		cr->set_source(back_grad);
		cr->rectangle(event->area.x, event->area.y,
			event->area.width, event->area.height);

		cr->fill();

    		Cairo::FontOptions font_options;
    		font_options.set_hint_style(Cairo::HINT_STYLE_NONE);
    		font_options.set_hint_metrics(Cairo::HINT_METRICS_OFF);
    		font_options.set_antialias(Cairo::ANTIALIAS_GRAY);

    		cr->set_font_options(font_options);
    		cr->move_to(5,height/2);
		cr->set_font_size(width/8);
		cr->set_source_rgba(0.9,0.9,0.9,0.8);
    		cr->show_text("New preset");
	
    		cr->set_font_options(font_options);
    		cr->move_to(5,height - (height/16));
		cr->set_font_size(width/11);
		cr->set_source_rgba(0.9,0.9,0.9,0.8);
    		cr->show_text(text + "_");

	}
  return true;
}


