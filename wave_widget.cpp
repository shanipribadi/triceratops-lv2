
// this file has the drawing code for our custom widget

#include "wave_widget.h"
#include <cmath>
#include <cairomm/context.h>

#include <iostream>



wave_widget::wave_widget()
{
	val = 0;
	last_waveform = 6;
	lfo_mode = false;
	label = "WAVE";
	pos_mode=0;

	top_colour.set( "#550000");
	bottom_colour.set( "#220000");

	// make the click events call the on_button_press_event function
	set_events (Gdk::BUTTON_PRESS_MASK | Gdk::BUTTON_RELEASE_MASK);

	// make the click event do that function
	// signal_button_press_event().connect( sigc::mem_fun(*this, &wave_widget::on_button_press_event) );
  
	set_size_request(40,140);
}

wave_widget::~wave_widget()
{
}

void wave_widget::set_label(string text)
{
	label = text;
	queue_draw();
}

void wave_widget::set_value(int _value)
{
	val = _value;	
	queue_draw();
}

 void wave_widget::set_lfo_mode(bool _lfo_mode)
{
	lfo_mode = _lfo_mode;
	if (lfo_mode) { last_waveform = 6; }
		else {last_waveform = 3; }
}

bool wave_widget::on_button_press_event(GdkEventButton* event)
{
}

bool wave_widget::on_button_release_event(GdkEventButton* event)
{

		val += 1;
		if (val > last_waveform) { val = 0; }

	float new_value = (float)val;

	// here we use the LV2UI_Controller and LV2UI_write_function "things"
	// to write some data to a port
	write_function( controller, port_number, sizeof(float), 0, (const void*)&new_value);
	
	queue_draw();
  
  return true;
}

bool wave_widget::on_expose_event(GdkEventExpose* event)
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
    cr->set_line_width(6.0);

    // clip to the area indicated by the expose event so that we only redraw
    // the portion of the window that needs to be redrawn
    cr->rectangle(event->area.x, event->area.y,
            event->area.width, event->area.height);
    cr->clip();
    
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
	cr->rectangle(event->area.x, event->area.y,
		event->area.width, event->area.height);
	cr->set_source(back_grad);
	cr->fill();

    cr->set_source_rgb(0.0, 0.0, 0.0);
    cr->set_line_width(width/12);
    cr->move_to(0,0);
    cr->line_to(0,allocation.get_height());
    cr->move_to(width,0);
    cr->line_to(width,allocation.get_height());
    cr->stroke();    

    // ------------------------------------------------------------------


    // draw horizontal black line at frequency height

    // draw text label
    cr->select_font_face("Bitstream Vera Sans", Cairo::FONT_SLANT_NORMAL,
    Cairo::FONT_WEIGHT_NORMAL);
    cr->set_font_size(width/5);
    cr->set_source_rgba(0.9,0.9,0.9,1.0);
    Cairo::FontOptions font_options;
    font_options.set_hint_style(Cairo::HINT_STYLE_NONE);
    font_options.set_hint_metrics(Cairo::HINT_METRICS_OFF);
    font_options.set_antialias(Cairo::ANTIALIAS_GRAY);
	
    int x_font_centre = (width/2) - ((width/5) * (label.length()/3.5));

    cr->set_font_options(font_options);
    cr->move_to(x_font_centre,height/6);
    cr->show_text(label);
    cr->move_to(x_font_centre,allocation.get_height()  - height_offset );


    cr->set_line_width(width/14);
    cr->set_source_rgba(0.8,0.8,0.8,1.0);

     Cairo::RefPtr<Cairo::LinearGradient> grad2 =
Cairo::LinearGradient::create(width/4, 0,width-(width/4), 0 );
        grad2->add_color_stop_rgba(0.0,0.2,0.2,0.2,1);
        grad2->add_color_stop_rgba(0.4,0.8,0.8,0.8,1);
        grad2->add_color_stop_rgba(1.0,0.4,0.4,0.4,1);

    cr->set_source(grad2);

    stringstream slider_value;
    switch (val)
    {
    	case 0:
		slider_value.str("SAW");
		cr->move_to(width/4, allocation.get_height()/1.5);
		cr->line_to(width-(width/4), allocation.get_height()/2.5);
		cr->stroke();
		break;

	case 1:
		slider_value.str("SQUARE"); 
		cr->move_to(width/4, allocation.get_height()/1.5);
		cr->line_to(width/4, allocation.get_height()/2.5);
		cr->line_to(width-(width/4), allocation.get_height()/2.5);
		cr->line_to(width-(width/4), allocation.get_height()/1.5);
		cr->stroke();
		break;

    	case 2:
		slider_value.str("SINE"); 
		cr->move_to(width/4, allocation.get_height()/2.5);
  		cr->curve_to(width/2, allocation.get_height(), width/2,0, width-(width/4),allocation.get_height()/1.5);
		cr->stroke();
		break;

    	case 3:
		if (lfo_mode) { slider_value.str("S&H"); }
			//else {  slider_value.str("NOISE"); }
		cr->move_to(width/4, allocation.get_height()/2);
		for (int x=(width/4)+(allocation.get_width()/16); x<(width-(width/4))-(allocation.get_width()/16); x+=(allocation.get_width()/16))
		{
			float y = (rand() % allocation.get_height()/4) - (2*(rand() % allocation.get_height()/4));  		
			cr->line_to(x, (allocation.get_height()/1.75) + (y/2));			
		}
		cr->line_to(width-(width/4), allocation.get_height()/2);
		cr->stroke();
		break;
    	case 4:
		slider_value.str("NOISE");
		cr->move_to(width/4, allocation.get_height()/2);
		for (int x=(width/4)+(allocation.get_width()/16); x<(width-(width/4))-(allocation.get_width()/16); x+=(allocation.get_width()/16))
		{
			float y = (rand() % allocation.get_height()/4) - (2*(rand() % allocation.get_height()/4));  		
			cr->line_to(x, (allocation.get_height()/1.75) + (y/2));			
		}
		cr->line_to(width-(width/4), allocation.get_height()/2);
		cr->stroke();
		break;
	case 5:
		slider_value.str("ISAW");
		cr->move_to(width/4, allocation.get_height()/2.5);
		cr->line_to(width-(width/4), allocation.get_height()/1.5);
		cr->stroke();
		break;
	case 6:
		slider_value.str("ISQUARE"); 
		cr->move_to(width/4, allocation.get_height()/2.5);
		cr->line_to(width/4, allocation.get_height()/1.5);
		cr->line_to(width-(width/4), allocation.get_height()/1.5);
		cr->line_to(width-(width/4), allocation.get_height()/2.5);
		cr->stroke();
		break;
    }


    x_font_centre = (width/2) - ((width/5) * (slider_value.str().length()/3.5));
    cr->move_to(x_font_centre,allocation.get_height() - (height_offset/3) );

    cr->set_source_rgba(0.9,0.9,0.9,1.0);
    cr->show_text(slider_value.str());

  }

  return true;
}


