
// this file has the drawing code for our custom widget

#include "preset_select.h"
#include <cairomm/context.h>

#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>


preset_select::preset_select()
{
	text = "";
	pos_mode = 0;
	current_preset = 0;
	current_preset_offset = 0;
	set_can_focus(true);


	top_colour.set( "#440000");
	bottom_colour.set( "#000000");
  
	// make the click events call the on_button_press_event function
	set_events (Gdk::BUTTON_PRESS_MASK | Gdk::BUTTON_RELEASE_MASK | Gdk::KEY_PRESS_MASK);

	// make the click event do that function
	// signal_button_press_event().connect( sigc::mem_fun(*this, &preset_select::on_button_press_event) );
  
	set_size_request(40,70);

	// load default presets
	preset_file_names.erase (preset_file_names.begin(),preset_file_names.begin()+preset_file_names.size());
	get_preset_list("/usr/lib/lv2/triceratops-presets.lv2/");
	get_preset_list("/usr/local/lib/lv2/triceratops-presets.lv2/");
	// load local presets
	stringstream home_lv2;
	home_lv2.str("");
	home_lv2 << getenv("HOME") << "/.lv2/";
	get_preset_list_local(home_lv2.str());
	std::sort(preset_file_names.begin(), preset_file_names.end());
}

preset_select::~preset_select()
{
}

void preset_select::init()
{
	// load_preset(current_preset);
	queue_draw();
}

int preset_select::get_symbol_port(string symbol)
{
	string triceratops_ttl_file_name = "";

	ifstream  triceratops_ttl("/usr/lib/lv2/triceratops.lv2/triceratops.ttl" );
	if (triceratops_ttl) triceratops_ttl_file_name = "/usr/lib/lv2/triceratops.lv2/triceratops.ttl" ;
	triceratops_ttl.open("/usr/local/lib/lv2/triceratops.lv2/triceratops.ttl" );
	if (triceratops_ttl) triceratops_ttl_file_name = "/usr/local/lib/lv2/triceratops.lv2/triceratops.ttl";

	int current_port = -1;
	int symbol_port = -1;

	if (triceratops_ttl_file_name !="")
	{
		string temp_str;
		string line;
		string current_symbol;
		while (getline(triceratops_ttl,line))  
		{
			int index_pos = line.rfind("lv2:index ");
			if (index_pos > 0) istringstream ( line.substr(index_pos + 10, line.length() - index_pos - 11) ) >> current_port;
			index_pos = line.rfind("lv2:symbol ");
			if (index_pos > 0)
			{
				current_symbol = line.substr(index_pos + 12, line.length() - index_pos - 14);
				if (current_symbol == symbol) symbol_port = current_port;
			}
		}
	}
	triceratops_ttl.close();

	if (current_port>-1) return symbol_port;
		else return -1;

}


void preset_select::load_preset(int preset_number)
{
	preset_number += current_preset_offset;
	if (preset_number < 0 || preset_number > preset_file_names.size()-1) return;

	stringstream preset_file_name_home;

	if ( preset_file_names[preset_number].substr(0,1) == ".")
	{

		preset_file_name_home.str("");
		preset_file_name_home << getenv("HOME") << "/.lv2/";
		preset_file_name_home << preset_file_names[preset_number].substr(1);
		preset_file_name_home << "/";
		int file_name_length = preset_file_names[preset_number].length();
		preset_file_name_home << preset_file_names[preset_number].substr(1,file_name_length-4);
		preset_file_name_home << "ttl";
	}

	current_preset = preset_number;
	stringstream preset_file_name;
	preset_file_name.str("");
	preset_file_name << "/usr/lib/lv2/triceratops-presets.lv2/";
	preset_file_name << preset_file_names[preset_number];

	stringstream preset_file_name_local;
	preset_file_name_local.str("");
	preset_file_name_local << "/usr/local/lib//lv2/triceratops-presets.lv2/";
	preset_file_name_local << preset_file_names[preset_number];

	string config_file_name = "";

	ifstream check_file(preset_file_name.str());
	if (check_file) config_file_name = preset_file_name.str();

	if (!check_file)
	{
		check_file.close();
		check_file.open(preset_file_name_local.str() );
		if (check_file) config_file_name = preset_file_name_local.str();
	}

	if (!check_file)
	{
		check_file.close();
		check_file.open(preset_file_name_home.str() );
		if (check_file) config_file_name = preset_file_name_home.str();
	}

	string preset_symbol;

	if (config_file_name !="")
	{
		string temp_str;
		string line;
		double preset_value;
		while (getline(check_file,line))  
		{
			int preset_index = line.rfind("lv2:symbol ");
			if (preset_index > 0) preset_symbol = line.substr(preset_index + 12, line.length() - preset_index - 15);

			preset_index = line.rfind("pset:value");
			if (preset_index > 0) istringstream ( line.substr(preset_index + 11, line.length()+1 ) ) >> preset_value;

			if (preset_index > 0)
			{
				int port = get_symbol_port(preset_symbol);
				float new_value = preset_value;
				write_function( controller, port, sizeof(float), 0, (const void*)&new_value);
				set_gui_widget(port,preset_value);
			}
		}
	}

	check_file.close();
}

void preset_select::get_preset_list(string dir)
{
	DIR *dp;
	struct dirent *dirp;

	if ((dp  = opendir(dir.c_str())) == NULL)
	{
		// cout << "Error(" << errno << ") opening " << dir << endl;
		closedir(dp);
		return;
	}

	while ((dirp = readdir(dp)) != NULL)
	{
		if (string(dirp->d_name) != ".directory" && 
			string(dirp->d_name) != "." &&
			string(dirp->d_name) != ".." &&
			string(dirp->d_name) != "manifest.ttl")
		{
			preset_file_names.push_back(string(dirp->d_name));
		}
	}
	closedir(dp);
}

void preset_select::get_preset_list_local(string dir)
{
	DIR *dp;
	struct dirent *dirp;

	if ((dp  = opendir(dir.c_str())) == NULL)
	{
		// cout << "Error(" << errno << ") opening " << dir << endl;
		closedir(dp);
		return;
	}

	while ((dirp = readdir(dp)) != NULL)
	{
		if (string(dirp->d_name) != ".directory" && 
			string(dirp->d_name) != "." &&
			string(dirp->d_name) != ".." &&
			string(dirp->d_name) != "manifest.ttl")
		{
			stringstream file_name;
			file_name.str("");
			file_name << "." << dirp->d_name; 

		stringstream preset_file_name_home;
		preset_file_name_home.str("");
		preset_file_name_home << getenv("HOME") << "/.lv2/";
		preset_file_name_home << file_name.str().substr(1);
		preset_file_name_home << "/";
		int file_name_length = file_name.str().length();
		preset_file_name_home << file_name.str().substr(1,file_name_length-4);
		preset_file_name_home << "ttl";

		bool our_preset = false;

		ifstream check_file(preset_file_name_home.str());
		string line;

		while (getline(check_file,line))  
		{
			int triceratops_applies_to = line.rfind("lv2:appliesTo <http://nickbailey.co.nr/triceratops>");
			if (triceratops_applies_to > 0) our_preset = true;
		}

		if (our_preset) preset_file_names.push_back(file_name.str());
		}
	}
	closedir(dp);
}


void preset_select::set_gui_widget(int port, float value)
{
	switch (port)
	{

		// OSCILLATORS

		case TRICERATOPS_ACTIVE_ONE:
			dco1->gui_active->set_value(value);
			dco1->gui_active->queue_draw();
			break;

		case TRICERATOPS_ACTIVE_TWO:
			dco2->gui_active->set_value(value);
			dco2->gui_active->queue_draw();
			break;

		case TRICERATOPS_ACTIVE_THREE:
			dco3->gui_active->set_value(value);
			dco3->gui_active->queue_draw();
			break;


		case TRICERATOPS_VOLUME_ONE:
			dco1->gui_volume->set_value(value);
			dco1->gui_volume->queue_draw();
			break;

		case TRICERATOPS_VOLUME_TWO:
			dco2->gui_volume->set_value(value);
			dco2->gui_volume->queue_draw();
			break;

		case TRICERATOPS_VOLUME_THREE:
			dco3->gui_volume->set_value(value);
			dco3->gui_volume->queue_draw();
			break;


		case TRICERATOPS_PULSEWIDTH_ONE:
			dco1->gui_pulsewidth->set_value(value);
			dco1->gui_pulsewidth->queue_draw();
			break;

		case TRICERATOPS_PULSEWIDTH_TWO:
			dco2->gui_pulsewidth->set_value(value);
			dco2->gui_pulsewidth->queue_draw();
			break;

		case TRICERATOPS_PULSEWIDTH_THREE:
			dco3->gui_pulsewidth->set_value(value);
			dco3->gui_pulsewidth->queue_draw();
			break;


		case TRICERATOPS_WAVE_ONE:
			dco1->gui_wave->set_value(value);
			dco1->gui_wave->queue_draw();
			break;

		case TRICERATOPS_WAVE_TWO:
			dco2->gui_wave->set_value(value);
			dco2->gui_wave->queue_draw();
			break;

		case TRICERATOPS_WAVE_THREE:
			dco3->gui_wave->set_value(value);
			dco3->gui_wave->queue_draw();
			break;


		case TRICERATOPS_OCTAVE_ONE:
			dco1->gui_octave->set_value(value);
			dco1->gui_octave->queue_draw();
			break;

		case TRICERATOPS_OCTAVE_TWO:
			dco2->gui_octave->set_value(value);
			dco2->gui_octave->queue_draw();
			break;

		case TRICERATOPS_OCTAVE_THREE:
			dco3->gui_octave->set_value(value);
			dco3->gui_octave->queue_draw();
			break;


		case TRICERATOPS_DETUNE_ONE:
			dco1->gui_detune->set_value(value);
			dco1->gui_detune->queue_draw();
			break;

		case TRICERATOPS_DETUNE_TWO:
			dco2->gui_detune->set_value(value);
			dco2->gui_detune->queue_draw();
			break;

		case TRICERATOPS_DETUNE_THREE:
			dco3->gui_detune->set_value(value);
			dco3->gui_detune->queue_draw();
			break;


		case TRICERATOPS_INERTIA_ONE:
			dco1->gui_inertia->set_value(value);
			dco1->gui_inertia->queue_draw();
			break;

		case TRICERATOPS_INERTIA_TWO:
			dco2->gui_inertia->set_value(value);
			dco2->gui_inertia->queue_draw();
			break;

		case TRICERATOPS_INERTIA_THREE:
			dco3->gui_inertia->set_value(value);
			dco3->gui_inertia->queue_draw();
			break;


		case TRICERATOPS_DCO1_PAN:
			dco1->gui_pan->set_value(value);
			dco1->gui_pan->queue_draw();
			break;

		case TRICERATOPS_DCO2_PAN:
			dco2->gui_pan->set_value(value);
			dco2->gui_pan->queue_draw();
			break;

		case TRICERATOPS_DCO3_PAN:
			dco3->gui_pan->set_value(value);
			dco3->gui_pan->queue_draw();
			break;

		// UNISON

		case TRICERATOPS_UNISON_ACTIVATE:
			unison->gui_active->set_value(value);
			unison->gui_active->queue_draw();
			break;
	

		case TRICERATOPS_UNISON_ONE:
			unison->gui_dco1_unison->set_value(value);
			unison->gui_dco1_unison->queue_draw();
			break;

		case TRICERATOPS_UNISON_TWO:
			unison->gui_dco2_unison->set_value(value);
			unison->gui_dco2_unison->queue_draw();
			break;

		case TRICERATOPS_UNISON_THREE:
			unison->gui_dco3_unison->set_value(value);
			unison->gui_dco3_unison->queue_draw();
			break;


		// LFOs

		case TRICERATOPS_LFO1_RETRIG:
			lfo1->gui_retrig->set_value(value);
			lfo1->gui_retrig->queue_draw();
			break;

		case TRICERATOPS_LFO2_RETRIG:
			lfo2->gui_retrig->set_value(value);
			lfo2->gui_retrig->queue_draw();
			break;

		case TRICERATOPS_LFO3_RETRIG:
			lfo3->gui_retrig->set_value(value);
			lfo3->gui_retrig->queue_draw();
			break;


		case TRICERATOPS_LFO1_SPEED:
			lfo1->gui_speed->set_value(value);
			lfo1->gui_speed->queue_draw();
			break;

		case TRICERATOPS_LFO2_SPEED:
			lfo2->gui_speed->set_value(value);
			lfo2->gui_speed->queue_draw();
			break;

		case TRICERATOPS_LFO3_SPEED:
			lfo3->gui_speed->set_value(value);
			lfo3->gui_speed->queue_draw();
			break;


		case TRICERATOPS_LFO1_DCO1_PITCH:
			lfo1->gui_dco1_pitch->set_value(value);
			lfo1->gui_dco1_pitch->queue_draw();
			break;

		case TRICERATOPS_LFO2_DCO1_PITCH:
			lfo2->gui_dco1_pitch->set_value(value);
			lfo2->gui_dco1_pitch->queue_draw();
			break;

		case TRICERATOPS_LFO3_DCO1_PITCH:
			lfo3->gui_dco1_pitch->set_value(value);
			lfo3->gui_dco1_pitch->queue_draw();
			break;


		case TRICERATOPS_LFO1_DCO2_PITCH:
			lfo1->gui_dco2_pitch->set_value(value);
			lfo1->gui_dco2_pitch->queue_draw();
			break;

		case TRICERATOPS_LFO2_DCO2_PITCH:
			lfo2->gui_dco2_pitch->set_value(value);
			lfo2->gui_dco2_pitch->queue_draw();
			break;

		case TRICERATOPS_LFO3_DCO2_PITCH:
			lfo3->gui_dco2_pitch->set_value(value);
			lfo3->gui_dco2_pitch->queue_draw();
			break;


		case TRICERATOPS_LFO1_DCO3_PITCH:
			lfo1->gui_dco3_pitch->set_value(value);
			lfo1->gui_dco3_pitch->queue_draw();
			break;

		case TRICERATOPS_LFO2_DCO3_PITCH:
			lfo2->gui_dco3_pitch->set_value(value);
			lfo2->gui_dco3_pitch->queue_draw();
			break;

		case TRICERATOPS_LFO3_DCO3_PITCH:
			lfo3->gui_dco3_pitch->set_value(value);
			lfo3->gui_dco3_pitch->queue_draw();
			break;


		case TRICERATOPS_LFO1_FILTER:
			lfo1->gui_cutoff->set_value(value);
			lfo1->gui_cutoff->queue_draw();
			break;

		case TRICERATOPS_LFO2_FILTER:
			lfo2->gui_cutoff->set_value(value);
			lfo2->gui_cutoff->queue_draw();
			break;

		case TRICERATOPS_LFO3_FILTER:
			lfo3->gui_cutoff->set_value(value);
			lfo3->gui_cutoff->queue_draw();
			break;


		case TRICERATOPS_LFO1_ROUTE_ONE:
			lfo1->gui_route1->set_value(value);
			lfo1->gui_route1->queue_draw();
			break;

		case TRICERATOPS_LFO2_ROUTE_ONE:
			lfo2->gui_route1->set_value(value);
			lfo2->gui_route1->queue_draw();
			break;

		case TRICERATOPS_LFO3_ROUTE_ONE:
			lfo3->gui_route1->set_value(value);
			lfo3->gui_route1->queue_draw();
			break;


		case TRICERATOPS_LFO1_ROUTE_ONE_DEST:
			lfo1->gui_route1->route_number = value;
			lfo1->gui_route1->queue_draw();
			break;

		case TRICERATOPS_LFO2_ROUTE_ONE_DEST:
			lfo2->gui_route1->route_number = value;
			lfo2->gui_route1->queue_draw();
			break;

		case TRICERATOPS_LFO3_ROUTE_ONE_DEST:
			lfo3->gui_route1->route_number = value;
			lfo3->gui_route1->queue_draw();
			break;



		case TRICERATOPS_LFO1_ROUTE_TWO:
			lfo1->gui_route2->set_value(value);
			lfo1->gui_route2->queue_draw();
			break;

		case TRICERATOPS_LFO2_ROUTE_TWO:
			lfo2->gui_route2->set_value(value);
			lfo2->gui_route2->queue_draw();
			break;

		case TRICERATOPS_LFO3_ROUTE_TWO:
			lfo3->gui_route2->set_value(value);
			lfo3->gui_route2->queue_draw();
			break;


		case TRICERATOPS_LFO1_ROUTE_TWO_DEST:
			lfo1->gui_route2->route_number = value;
			lfo1->gui_route2->queue_draw();
			break;

		case TRICERATOPS_LFO2_ROUTE_TWO_DEST:
			lfo2->gui_route2->route_number = value;
			lfo2->gui_route2->queue_draw();
			break;

		case TRICERATOPS_LFO3_ROUTE_TWO_DEST:
			lfo3->gui_route2->route_number = value;
			lfo3->gui_route2->queue_draw();
			break;


		case TRICERATOPS_LFO1_WAVE:
			lfo1->gui_wave->set_value(value);
			lfo1->gui_wave->queue_draw();
			break;

		case TRICERATOPS_LFO2_WAVE:
			lfo2->gui_wave->set_value(value);
			lfo2->gui_wave->queue_draw();
			break;

		case TRICERATOPS_LFO3_WAVE:
			lfo3->gui_wave->set_value(value);
			lfo3->gui_wave->queue_draw();
			break;

		// ADSRs

		case TRICERATOPS_ATTACK_ONE:
			adsr_amp->gui_attack->set_value(value);
			adsr_amp->gui_attack->queue_draw();
			break;

		case TRICERATOPS_ATTACK_TWO:
			adsr_filter->gui_attack->set_value(value);
			adsr_filter->gui_attack->queue_draw();
			break;

		case TRICERATOPS_ATTACK_THREE:
			adsr_lfo->gui_attack->set_value(value);
			adsr_lfo->gui_attack->queue_draw();
			break;


		case TRICERATOPS_DECAY_ONE:
			adsr_amp->gui_decay->set_value(value);
			adsr_amp->gui_decay->queue_draw();
			break;

		case TRICERATOPS_DECAY_TWO:
			adsr_filter->gui_decay->set_value(value);
			adsr_filter->gui_decay->queue_draw();
			break;

		case TRICERATOPS_DECAY_THREE:
			adsr_lfo->gui_decay->set_value(value);
			adsr_lfo->gui_decay->queue_draw();
			break;


		case TRICERATOPS_SUSTAIN_ONE:
			adsr_amp->gui_sustain->set_value(value);
			adsr_amp->gui_sustain->queue_draw();
			break;

		case TRICERATOPS_SUSTAIN_TWO:
			adsr_filter->gui_sustain->set_value(value);
			adsr_filter->gui_sustain->queue_draw();
			break;

		case TRICERATOPS_SUSTAIN_THREE:
			adsr_lfo->gui_sustain->set_value(value);
			adsr_lfo->gui_sustain->queue_draw();
			break;


		case TRICERATOPS_RELEASE_ONE:
			adsr_amp->gui_release->set_value(value);
			adsr_amp->gui_release->queue_draw();
			break;

		case TRICERATOPS_RELEASE_TWO:
			adsr_filter->gui_release->set_value(value);
			adsr_filter->gui_release->queue_draw();
			break;

		case TRICERATOPS_RELEASE_THREE:
			adsr_lfo->gui_release->set_value(value);
			adsr_lfo->gui_release->queue_draw();
			break;


		case TRICERATOPS_ADSR1_ROUTE_ONE:
			adsr_amp->gui_route1->set_value(value);
			adsr_amp->gui_route1->queue_draw();
			break;

		case TRICERATOPS_ADSR1_ROUTE_TWO:
			adsr_amp->gui_route2->set_value(value);
			adsr_amp->gui_route2->queue_draw();
			break;

		case TRICERATOPS_ADSR1_ROUTE_ONE_DEST:
			adsr_amp->gui_route1->route_number = value;
			adsr_amp->gui_route1->queue_draw();
			break;

		case TRICERATOPS_ADSR1_ROUTE_TWO_DEST:
			adsr_amp->gui_route2->route_number = value;
			adsr_amp->gui_route2->queue_draw();
			break;


		case TRICERATOPS_ADSR2_ROUTE_ONE:
			adsr_filter->gui_route1->set_value(value);
			adsr_filter->gui_route1->queue_draw();
			break;

		case TRICERATOPS_ADSR2_ROUTE_TWO:
			adsr_filter->gui_route2->set_value(value);
			adsr_filter->gui_route2->queue_draw();
			break;

		case TRICERATOPS_ADSR2_ROUTE_ONE_DEST:
			adsr_filter->gui_route1->route_number = value;
			adsr_filter->gui_route1->queue_draw();
			break;

		case TRICERATOPS_ADSR2_ROUTE_TWO_DEST:
			adsr_filter->gui_route2->route_number = value;
			adsr_filter->gui_route2->queue_draw();
			break;


		case TRICERATOPS_ADSR3_LFO1_AMOUNT:
			adsr_lfo->gui_lfo1->set_value(value);
			adsr_lfo->gui_lfo1->queue_draw();
			break;

		case TRICERATOPS_ADSR3_LFO2_AMOUNT:
			adsr_lfo->gui_lfo2->set_value(value);
			adsr_lfo->gui_lfo2->queue_draw();
			break;

		case TRICERATOPS_ADSR3_LFO3_AMOUNT:
			adsr_lfo->gui_lfo3->set_value(value);
			adsr_lfo->gui_lfo3->queue_draw();
			break;

		// AMP AND FILTER

		case TRICERATOPS_CUTOFF:
			amp_and_filter->gui_cutoff->set_value(value);
			amp_and_filter->gui_filter_type->val_cutoff = value;
			amp_and_filter->gui_cutoff->queue_draw();
			break;

		case TRICERATOPS_RESONANCE:
			amp_and_filter->gui_resonance->set_value(value);
			amp_and_filter->gui_resonance->queue_draw();
			break;

		case TRICERATOPS_LEGATO:
			amp_and_filter->gui_legato->set_value(value);
			amp_and_filter->gui_legato->queue_draw();
			break;

		case TRICERATOPS_FILTER_MODE:
			amp_and_filter->gui_filter_type->set_value(value);
			amp_and_filter->gui_filter_type->queue_draw();
			break;

		case TRICERATOPS_FILTER_KEY_FOLLOW:
			amp_and_filter->gui_key_follow->set_value(value);
			amp_and_filter->gui_key_follow->queue_draw();
			break;

		case TRICERATOPS_MASTER_TUNE:
			amp_and_filter->gui_tune->set_value(value);
			amp_and_filter->gui_tune->queue_draw();
			break;

		case TRICERATOPS_SYNC:
			amp_and_filter->gui_sync->set_value(value);
			amp_and_filter->gui_sync->queue_draw();
			break;

		case TRICERATOPS_FM:
			amp_and_filter->gui_fm->set_value(value);
			amp_and_filter->gui_fm->queue_draw();
			break;

		case TRICERATOPS_MASTER_VOLUME:
			amp_and_filter->gui_volume->set_value(value);
			amp_and_filter->gui_volume->queue_draw();
			break;

		case TRICERATOPS_AMP_DRIVE:
			amp_and_filter->gui_drive->set_value(value);
			amp_and_filter->gui_drive->queue_draw();
			break;

		// ECHO

		case TRICERATOPS_FX_ECHO_ACTIVE:
			echo->gui_active->set_value(value);
			echo->gui_active->queue_draw();
			break;

		case TRICERATOPS_FX_ECHO_SPEED:
			echo->gui_speed->set_value(value);
			echo->gui_speed->queue_draw();
			break;

		case TRICERATOPS_FX_ECHO_DECAY:
			echo->gui_decay->set_value(value);
			echo->gui_decay->queue_draw();
			break;

		case TRICERATOPS_FX_ECHO_EQ_LOW:
			echo->gui_eq_low->set_value(value);
			echo->gui_eq_low->queue_draw();
			break;

		case TRICERATOPS_FX_ECHO_EQ_MID:
			echo->gui_eq_mid->set_value(value);
			echo->gui_eq_mid->queue_draw();
			break;

		case TRICERATOPS_FX_ECHO_EQ_HIGH:
			echo->gui_eq_high->set_value(value);
			echo->gui_eq_high->queue_draw();
			break;

		// REVERB

		case TRICERATOPS_FX_REVERB_ACTIVE:
			reverb->gui_active->set_value(value);
			reverb->gui_active->queue_draw();
			break;

		case TRICERATOPS_FX_REVERB_DECAY:
			reverb->gui_decay->set_value(value);
			reverb->gui_decay->queue_draw();
			break;

		case TRICERATOPS_FX_REVERB_MIX:
			reverb->gui_mix->set_value(value);
			reverb->gui_mix->queue_draw();
			break;

		// MODIFIERS

		case TRICERATOPS_MODIFIER_DIRT:
			modifier->gui_modifier_dirt->set_value(value);
			modifier->gui_modifier_dirt->queue_draw();
			break;

		case TRICERATOPS_MODIFIER_STEREO_MODE:
			modifier->gui_modifier_stereo_mode->set_value(value);
			modifier->gui_modifier_stereo_mode->queue_draw();
			break;

		case TRICERATOPS_MODIFIER_RING:
			modifier->gui_modifier_ring->set_value(value);
			modifier->gui_modifier_ring->queue_draw();
			break;


	}
	
}


void preset_select::position_top(bool mode)
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

void preset_select::set_text(string text)
{
	text = text;
	queue_draw();
}

void preset_select::set_value(int _value)
{
	val = _value;	
	queue_draw();
}

bool preset_select::on_key_press_event(GdkEventKey* event)
{
	int key = event->keyval;

	if (key == 65293) // ENTER
	{	
		create_new_preset();
	}

	if (key == 65362 && current_preset_offset > 0) // UP ARROW
	{
		--current_preset_offset;
		queue_draw();
	}

	if (key == 65364 && current_preset_offset < preset_file_names.size()-1) // UP ARROW
	{
		++current_preset_offset;
		queue_draw();
	}

	if (key == 65535 || key ==65288) // BACKSPACE
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

bool preset_select::on_button_press_event(GdkEventButton* event)
{

	grab_focus();

	if (event->button == 1)
	{	
	  	const int width = get_allocation().get_width();
  		const int height = get_allocation().get_height();

		if (event->y < 30)
		{
			if (event->x < width/2 && current_preset_offset > 0) --current_preset_offset;
			if (event->x >= width/2 && current_preset_offset < preset_file_names.size()-1) ++current_preset_offset;
			queue_draw();
			return true;
		}

		if (event->y > 40 && event->y < height-50)
		{
			int preset_number = (int)(0.5+((event->y-width/2)/20));
			queue_draw();
			load_preset(preset_number);
		}
	}

	if (event->button == 3)
	{
		// load default presets
		preset_file_names.erase (preset_file_names.begin(),preset_file_names.begin()+preset_file_names.size());
		get_preset_list("/usr/lib/lv2/triceratops-presets.lv2/");
		get_preset_list("/usr/local/lib/lv2/triceratops-presets.lv2/");
		// load local presets
		stringstream home_lv2;
		home_lv2.str("");
		home_lv2 << getenv("HOME") << "/.lv2/";
		get_preset_list_local(home_lv2.str());
		std::sort(preset_file_names.begin(), preset_file_names.end());
		current_preset_offset = 0;
		current_preset = 0;
		load_preset(0);
		queue_draw();
	}
}

bool preset_select::on_button_release_event(GdkEventButton* event)
{
}


void preset_select::create_new_preset()
{
	if (text == "") return;

	string temp_str = text;
	 replace( temp_str.begin(), temp_str.end(), ' ', '_' );

	stringstream new_preset_file_name;
	new_preset_file_name.str("");
	new_preset_file_name << "mkdir -p ";
	new_preset_file_name << getenv("HOME") << "/.lv2/";
	new_preset_file_name << temp_str << ".lv2";

	int err = system(new_preset_file_name.str().c_str());
	
	stringstream new_preset_manifest;
	new_preset_manifest.str("");
	new_preset_manifest << new_preset_file_name.str() << "/manifest.ttl";
	new_preset_file_name << "/" << temp_str << ".ttl";

	ofstream outputFile;
	outputFile.open( new_preset_file_name.str().substr(9) );

	outputFile << "@prefix atom: <http://lv2plug.in/ns/ext/atom#> ." << endl;
	outputFile << "@prefix lv2: <http://lv2plug.in/ns/lv2core#> ." << endl;
	outputFile << "@prefix pset: <http://lv2plug.in/ns/ext/presets#> ." << endl;
	outputFile << "@prefix rdf: <http://www.w3.org/1999/02/22-rdf-syntax-ns#> ." << endl;
	outputFile << "@prefix rdfs: <http://www.w3.org/2000/01/rdf-schema#> ." << endl;
	outputFile << "@prefix state: <http://lv2plug.in/ns/ext/state#> ." << endl;
	outputFile << "@prefix xsd: <http://www.w3.org/2001/XMLSchema#> ." << endl;
	outputFile << "<>" << endl;
	outputFile << "	a pset:Preset ;" << endl;
	outputFile << "	lv2:appliesTo <http://nickbailey.co.nr/triceratops> ;" << endl;
	outputFile << "	rdfs:label ";
	outputFile << char(34) << temp_str;
	outputFile << char(34) << ";" << endl;
	outputFile << "lv2:port [" << endl;

	// AMP & FILTER

	outputFile << "		lv2:symbol " << char(34) << "master_volume" << char(34) << " ;" << endl;
	outputFile << "		pset:value " << amp_and_filter->gui_volume->value << endl;
	outputFile << "		] , [ " << endl;

	outputFile << "		lv2:symbol " << char(34) << "master_tune" << char(34) << " ;" << endl;
	outputFile << "		pset:value " << amp_and_filter->gui_tune->value << endl;
	outputFile << "		] , [ " << endl;

	outputFile << "		lv2:symbol " << char(34) << "amp_drive" << char(34) << " ;" << endl;
	outputFile << "		pset:value " << amp_and_filter->gui_drive->value << endl;
	outputFile << "		] , [ " << endl;

	outputFile << "		lv2:symbol " << char(34) << "filter_mode" << char(34) << " ;" << endl;
	outputFile << "		pset:value " << amp_and_filter->gui_filter_type->val << endl;
	outputFile << "		] , [ " << endl;

	outputFile << "		lv2:symbol " << char(34) << "cutoff" << char(34) << " ;" << endl;
	outputFile << "		pset:value " << amp_and_filter->gui_cutoff->value << endl;
	outputFile << "		] , [ " << endl;

	outputFile << "		lv2:symbol " << char(34) << "resonance" << char(34) << " ;" << endl;
	outputFile << "		pset:value " << amp_and_filter->gui_resonance->value << endl;
	outputFile << "		] , [ " << endl;

	outputFile << "		lv2:symbol " << char(34) << "filter_key_follow" << char(34) << " ;" << endl;
	outputFile << "		pset:value " << amp_and_filter->gui_key_follow->value << endl;
	outputFile << "		] , [ " << endl;

	outputFile << "		lv2:symbol " << char(34) << "legato" << char(34) << " ;" << endl;
	outputFile << "		pset:value " << amp_and_filter->gui_legato->val << endl;
	outputFile << "		] , [ " << endl;

	outputFile << "		lv2:symbol " << char(34) << "sync" << char(34) << " ;" << endl;
	outputFile << "		pset:value " << amp_and_filter->gui_sync->val << endl;
	outputFile << "		] , [ " << endl;

	outputFile << "		lv2:symbol " << char(34) << "fm" << char(34) << " ;" << endl;
	outputFile << "		pset:value " << amp_and_filter->gui_fm->val << endl;
	outputFile << "		] , [ " << endl;

	//-------- DCO ONE

	outputFile << "		lv2:symbol " << char(34) << "active_one" << char(34) << " ;" << endl;
	outputFile << "		pset:value " << dco1->gui_active->val << endl;
	outputFile << "		] , [ " << endl;

	outputFile << "		lv2:symbol " << char(34) << "volume_one" << char(34) << " ;" << endl;
	outputFile << "		pset:value " << dco1->gui_volume->value << endl;
	outputFile << "		] , [ " << endl;

	outputFile << "		lv2:symbol " << char(34) << "pulsewidth_one" << char(34) << " ;" << endl;
	outputFile << "		pset:value " << dco1->gui_pulsewidth->value << endl;
	outputFile << "		] , [ " << endl;

	outputFile << "		lv2:symbol " << char(34) << "wave_one" << char(34) << " ;" << endl;
	outputFile << "		pset:value " << dco1->gui_wave->val << endl;
	outputFile << "		] , [ " << endl;

	outputFile << "		lv2:symbol " << char(34) << "octave_one" << char(34) << " ;" << endl;
	outputFile << "		pset:value " << dco1->gui_octave->value << endl;
	outputFile << "		] , [ " << endl;

	outputFile << "		lv2:symbol " << char(34) << "detune_one" << char(34) << " ;" << endl;
	outputFile << "		pset:value " << dco1->gui_detune->value << endl;
	outputFile << "		] , [ " << endl;

	outputFile << "		lv2:symbol " << char(34) << "inertia_one" << char(34) << " ;" << endl;
	outputFile << "		pset:value " << dco1->gui_inertia->value << endl;
	outputFile << "		] , [ " << endl;

	//-------- DCO TWO

	outputFile << "		lv2:symbol " << char(34) << "active_two" << char(34) << " ;" << endl;
	outputFile << "		pset:value " << dco2->gui_active->val << endl;
	outputFile << "		] , [ " << endl;

	outputFile << "		lv2:symbol " << char(34) << "volume_two" << char(34) << " ;" << endl;
	outputFile << "		pset:value " << dco2->gui_volume->value << endl;
	outputFile << "		] , [ " << endl;

	outputFile << "		lv2:symbol " << char(34) << "pulsewidth_two" << char(34) << " ;" << endl;
	outputFile << "		pset:value " << dco2->gui_pulsewidth->value << endl;
	outputFile << "		] , [ " << endl;

	outputFile << "		lv2:symbol " << char(34) << "wave_two" << char(34) << " ;" << endl;
	outputFile << "		pset:value " << dco2->gui_wave->val << endl;
	outputFile << "		] , [ " << endl;

	outputFile << "		lv2:symbol " << char(34) << "octave_two" << char(34) << " ;" << endl;
	outputFile << "		pset:value " << dco2->gui_octave->value << endl;
	outputFile << "		] , [ " << endl;

	outputFile << "		lv2:symbol " << char(34) << "detune_two" << char(34) << " ;" << endl;
	outputFile << "		pset:value " << dco2->gui_detune->value << endl;
	outputFile << "		] , [ " << endl;

	outputFile << "		lv2:symbol " << char(34) << "inertia_two" << char(34) << " ;" << endl;
	outputFile << "		pset:value " << dco2->gui_inertia->value << endl;
	outputFile << "		] , [ " << endl;

	// DCO THREE

	outputFile << "		lv2:symbol " << char(34) << "active_three" << char(34) << " ;" << endl;
	outputFile << "		pset:value " << dco3->gui_active->val << endl;
	outputFile << "		] , [ " << endl;

	outputFile << "		lv2:symbol " << char(34) << "volume_three" << char(34) << " ;" << endl;
	outputFile << "		pset:value " << dco3->gui_volume->value << endl;
	outputFile << "		] , [ " << endl;

	outputFile << "		lv2:symbol " << char(34) << "pulsewidth_three" << char(34) << " ;" << endl;
	outputFile << "		pset:value " << dco3->gui_pulsewidth->value << endl;
	outputFile << "		] , [ " << endl;

	outputFile << "		lv2:symbol " << char(34) << "wave_three" << char(34) << " ;" << endl;
	outputFile << "		pset:value " << dco3->gui_wave->val << endl;
	outputFile << "		] , [ " << endl;

	outputFile << "		lv2:symbol " << char(34) << "octave_three" << char(34) << " ;" << endl;
	outputFile << "		pset:value " << dco3->gui_octave->value << endl;
	outputFile << "		] , [ " << endl;

	outputFile << "		lv2:symbol " << char(34) << "detune_three" << char(34) << " ;" << endl;
	outputFile << "		pset:value " << dco3->gui_detune->value << endl;
	outputFile << "		] , [ " << endl;

	outputFile << "		lv2:symbol " << char(34) << "inertia_three" << char(34) << " ;" << endl;
	outputFile << "		pset:value " << dco3->gui_inertia->value << endl;
	outputFile << "		] , [ " << endl;

	// UNISON

	outputFile << "		lv2:symbol " << char(34) << "unison_activate" << char(34) << " ;" << endl;
	outputFile << "		pset:value " << unison->gui_active->val << endl;
	outputFile << "		] , [ " << endl;

	outputFile << "		lv2:symbol " << char(34) << "unison_one" << char(34) << " ;" << endl;
	outputFile << "		pset:value " << unison->gui_dco1_unison->value << endl;
	outputFile << "		] , [ " << endl;

	outputFile << "		lv2:symbol " << char(34) << "unison_two" << char(34) << " ;" << endl;
	outputFile << "		pset:value " << unison->gui_dco2_unison->value << endl;
	outputFile << "		] , [ " << endl;

	outputFile << "		lv2:symbol " << char(34) << "unison_three" << char(34) << " ;" << endl;
	outputFile << "		pset:value " << unison->gui_dco3_unison->value << endl;
	outputFile << "		] , [ " << endl;




	// ENVELOPE ONE - AMP

	outputFile << "		lv2:symbol " << char(34) << "attack_one" << char(34) << " ;" << endl;
	outputFile << "		pset:value " << adsr_amp->gui_attack->value << endl;
	outputFile << "		] , [ " << endl;

	outputFile << "		lv2:symbol " << char(34) << "decay_one" << char(34) << " ;" << endl;
	outputFile << "		pset:value " << adsr_amp->gui_decay->value << endl;
	outputFile << "		] , [ " << endl;

	outputFile << "		lv2:symbol " << char(34) << "sustain_one" << char(34) << " ;" << endl;
	outputFile << "		pset:value " << adsr_amp->gui_sustain->value << endl;
	outputFile << "		] , [ " << endl;

	outputFile << "		lv2:symbol " << char(34) << "release_one" << char(34) << " ;" << endl;
	outputFile << "		pset:value " << adsr_amp->gui_release->value << endl;
	outputFile << "		] , [ " << endl;

	outputFile << "		lv2:symbol " << char(34) << "adsr1_route_one" << char(34) << " ;" << endl;
	outputFile << "		pset:value " << adsr_amp->gui_route1->value << endl;
	outputFile << "		] , [ " << endl;

	outputFile << "		lv2:symbol " << char(34) << "adsr1_route_one_dest" << char(34) << " ;" << endl;
	outputFile << "		pset:value " << adsr_amp->gui_route1->route_number << endl;
	outputFile << "		] , [ " << endl;

	outputFile << "		lv2:symbol " << char(34) << "adsr1_route_two" << char(34) << " ;" << endl;
	outputFile << "		pset:value " << adsr_amp->gui_route2->value << endl;
	outputFile << "		] , [ " << endl;

	outputFile << "		lv2:symbol " << char(34) << "adsr1_route_two_dest" << char(34) << " ;" << endl;
	outputFile << "		pset:value " << adsr_amp->gui_route2->route_number << endl;
	outputFile << "		] , [ " << endl;

	// ENVELOPE TWO - FILTER

	outputFile << "		lv2:symbol " << char(34) << "attack_two" << char(34) << " ;" << endl;
	outputFile << "		pset:value " << adsr_filter->gui_attack->value << endl;
	outputFile << "		] , [ " << endl;

	outputFile << "		lv2:symbol " << char(34) << "decay_two" << char(34) << " ;" << endl;
	outputFile << "		pset:value " << adsr_filter->gui_decay->value << endl;
	outputFile << "		] , [ " << endl;

	outputFile << "		lv2:symbol " << char(34) << "sustain_two" << char(34) << " ;" << endl;
	outputFile << "		pset:value " << adsr_filter->gui_sustain->value << endl;
	outputFile << "		] , [ " << endl;

	outputFile << "		lv2:symbol " << char(34) << "release_two" << char(34) << " ;" << endl;
	outputFile << "		pset:value " << adsr_filter->gui_release->value << endl;
	outputFile << "		] , [ " << endl;

	outputFile << "		lv2:symbol " << char(34) << "adsr2_route_one" << char(34) << " ;" << endl;
	outputFile << "		pset:value " << adsr_filter->gui_route1->value << endl;
	outputFile << "		] , [ " << endl;

	outputFile << "		lv2:symbol " << char(34) << "adsr2_route_one_dest" << char(34) << " ;" << endl;
	outputFile << "		pset:value " << adsr_filter->gui_route1->route_number << endl;
	outputFile << "		] , [ " << endl;

	outputFile << "		lv2:symbol " << char(34) << "adsr2_route_two" << char(34) << " ;" << endl;
	outputFile << "		pset:value " << adsr_filter->gui_route2->value << endl;
	outputFile << "		] , [ " << endl;

	outputFile << "		lv2:symbol " << char(34) << "adsr2_route_two_dest" << char(34) << " ;" << endl;
	outputFile << "		pset:value " << adsr_filter->gui_route2->route_number << endl;
	outputFile << "		] , [ " << endl;

	// ENVELOPE THREE - LFOs

	outputFile << "		lv2:symbol " << char(34) << "attack_three" << char(34) << " ;" << endl;
	outputFile << "		pset:value " << adsr_lfo->gui_attack->value << endl;
	outputFile << "		] , [ " << endl;

	outputFile << "		lv2:symbol " << char(34) << "decay_three" << char(34) << " ;" << endl;
	outputFile << "		pset:value " << adsr_lfo->gui_decay->value << endl;
	outputFile << "		] , [ " << endl;

	outputFile << "		lv2:symbol " << char(34) << "sustain_three" << char(34) << " ;" << endl;
	outputFile << "		pset:value " << adsr_lfo->gui_sustain->value << endl;
	outputFile << "		] , [ " << endl;

	outputFile << "		lv2:symbol " << char(34) << "release_three" << char(34) << " ;" << endl;
	outputFile << "		pset:value " << adsr_lfo->gui_release->value << endl;
	outputFile << "		] , [ " << endl;

	outputFile << "		lv2:symbol " << char(34) << "adsr3_lfo1_amount" << char(34) << " ;" << endl;
	outputFile << "		pset:value " << adsr_lfo->gui_lfo1->val << endl;
	outputFile << "		] , [ " << endl;

	outputFile << "		lv2:symbol " << char(34) << "adsr3_lfo2_amount" << char(34) << " ;" << endl;
	outputFile << "		pset:value " << adsr_lfo->gui_lfo2->val << endl;
	outputFile << "		] , [ " << endl;

	outputFile << "		lv2:symbol " << char(34) << "adsr3_lfo3_amount" << char(34) << " ;" << endl;
	outputFile << "		pset:value " << adsr_lfo->gui_lfo3->val << endl;
	outputFile << "		] , [ " << endl;

	// LFO 1

	outputFile << "		lv2:symbol " << char(34) << "lfo1_retrig" << char(34) << " ;" << endl;
	outputFile << "		pset:value " << lfo1->gui_retrig->val << endl;
	outputFile << "		] , [ " << endl;

	outputFile << "		lv2:symbol " << char(34) << "lfo1_speed" << char(34) << " ;" << endl;
	outputFile << "		pset:value " << lfo1->gui_speed->value << endl;
	outputFile << "		] , [ " << endl;

	outputFile << "		lv2:symbol " << char(34) << "lfo1_wave" << char(34) << " ;" << endl;
	outputFile << "		pset:value " << lfo1->gui_wave->val << endl;
	outputFile << "		] , [ " << endl;

	outputFile << "		lv2:symbol " << char(34) << "lfo1_speed" << char(34) << " ;" << endl;
	outputFile << "		pset:value " << lfo1->gui_speed->value << endl;
	outputFile << "		] , [ " << endl;

	outputFile << "		lv2:symbol " << char(34) << "lfo1_dco1_pitch" << char(34) << " ;" << endl;
	outputFile << "		pset:value " << lfo1->gui_dco1_pitch->value << endl;
	outputFile << "		] , [ " << endl;

	outputFile << "		lv2:symbol " << char(34) << "lfo1_dco2_pitch" << char(34) << " ;" << endl;
	outputFile << "		pset:value " << lfo1->gui_dco2_pitch->value << endl;
	outputFile << "		] , [ " << endl;

	outputFile << "		lv2:symbol " << char(34) << "lfo1_dco3_pitch" << char(34) << " ;" << endl;
	outputFile << "		pset:value " << lfo1->gui_dco3_pitch->value << endl;
	outputFile << "		] , [ " << endl;

	outputFile << "		lv2:symbol " << char(34) << "lfo1_filter" << char(34) << " ;" << endl;
	outputFile << "		pset:value " << lfo1->gui_cutoff->value << endl;
	outputFile << "		] , [ " << endl;

	outputFile << "		lv2:symbol " << char(34) << "lfo1_route_one" << char(34) << " ;" << endl;
	outputFile << "		pset:value " << lfo1->gui_route1->value << endl;
	outputFile << "		] , [ " << endl;

	outputFile << "		lv2:symbol " << char(34) << "lfo1_route_one_dest" << char(34) << " ;" << endl;
	outputFile << "		pset:value " << lfo1->gui_route1->route_number  << endl;
	outputFile << "		] , [ " << endl;

	outputFile << "		lv2:symbol " << char(34) << "lfo1_route_two" << char(34) << " ;" << endl;
	outputFile << "		pset:value " << lfo1->gui_route2->value << endl;
	outputFile << "		] , [ " << endl;

	outputFile << "		lv2:symbol " << char(34) << "lfo1_route_two_dest" << char(34) << " ;" << endl;
	outputFile << "		pset:value " << lfo1->gui_route2->route_number  << endl;
	outputFile << "		] , [ " << endl;


	// LFO 2

	outputFile << "		lv2:symbol " << char(34) << "lfo2_retrig" << char(34) << " ;" << endl;
	outputFile << "		pset:value " << lfo2->gui_retrig->val << endl;
	outputFile << "		] , [ " << endl;

	outputFile << "		lv2:symbol " << char(34) << "lfo2_speed" << char(34) << " ;" << endl;
	outputFile << "		pset:value " << lfo2->gui_speed->value << endl;
	outputFile << "		] , [ " << endl;

	outputFile << "		lv2:symbol " << char(34) << "lfo2_wave" << char(34) << " ;" << endl;
	outputFile << "		pset:value " << lfo2->gui_wave->val << endl;
	outputFile << "		] , [ " << endl;

	outputFile << "		lv2:symbol " << char(34) << "lfo2_speed" << char(34) << " ;" << endl;
	outputFile << "		pset:value " << lfo2->gui_speed->value << endl;
	outputFile << "		] , [ " << endl;

	outputFile << "		lv2:symbol " << char(34) << "lfo2_dco1_pitch" << char(34) << " ;" << endl;
	outputFile << "		pset:value " << lfo2->gui_dco1_pitch->value << endl;
	outputFile << "		] , [ " << endl;

	outputFile << "		lv2:symbol " << char(34) << "lfo2_dco2_pitch" << char(34) << " ;" << endl;
	outputFile << "		pset:value " << lfo2->gui_dco2_pitch->value << endl;
	outputFile << "		] , [ " << endl;

	outputFile << "		lv2:symbol " << char(34) << "lfo2_dco3_pitch" << char(34) << " ;" << endl;
	outputFile << "		pset:value " << lfo2->gui_dco3_pitch->value << endl;
	outputFile << "		] , [ " << endl;

	outputFile << "		lv2:symbol " << char(34) << "lfo2_filter" << char(34) << " ;" << endl;
	outputFile << "		pset:value " << lfo2->gui_cutoff->value << endl;
	outputFile << "		] , [ " << endl;

	outputFile << "		lv2:symbol " << char(34) << "lfo2_route_one" << char(34) << " ;" << endl;
	outputFile << "		pset:value " << lfo2->gui_route1->value << endl;
	outputFile << "		] , [ " << endl;

	outputFile << "		lv2:symbol " << char(34) << "lfo2_route_one_dest" << char(34) << " ;" << endl;
	outputFile << "		pset:value " << lfo2->gui_route1->route_number  << endl;
	outputFile << "		] , [ " << endl;

	outputFile << "		lv2:symbol " << char(34) << "lfo2_route_two" << char(34) << " ;" << endl;
	outputFile << "		pset:value " << lfo2->gui_route2->value << endl;
	outputFile << "		] , [ " << endl;

	outputFile << "		lv2:symbol " << char(34) << "lfo2_route_two_dest" << char(34) << " ;" << endl;
	outputFile << "		pset:value " << lfo2->gui_route2->route_number  << endl;
	outputFile << "		] , [ " << endl;

	// LFO 3

	outputFile << "		lv2:symbol " << char(34) << "lfo3_retrig" << char(34) << " ;" << endl;
	outputFile << "		pset:value " << lfo3->gui_retrig->val << endl;
	outputFile << "		] , [ " << endl;

	outputFile << "		lv2:symbol " << char(34) << "lfo3_speed" << char(34) << " ;" << endl;
	outputFile << "		pset:value " << lfo3->gui_speed->value << endl;
	outputFile << "		] , [ " << endl;

	outputFile << "		lv2:symbol " << char(34) << "lfo3_wave" << char(34) << " ;" << endl;
	outputFile << "		pset:value " << lfo3->gui_wave->val << endl;
	outputFile << "		] , [ " << endl;

	outputFile << "		lv2:symbol " << char(34) << "lfo3_speed" << char(34) << " ;" << endl;
	outputFile << "		pset:value " << lfo3->gui_speed->value << endl;
	outputFile << "		] , [ " << endl;

	outputFile << "		lv2:symbol " << char(34) << "lfo3_dco1_pitch" << char(34) << " ;" << endl;
	outputFile << "		pset:value " << lfo3->gui_dco1_pitch->value << endl;
	outputFile << "		] , [ " << endl;

	outputFile << "		lv2:symbol " << char(34) << "lfo3_dco2_pitch" << char(34) << " ;" << endl;
	outputFile << "		pset:value " << lfo3->gui_dco2_pitch->value << endl;
	outputFile << "		] , [ " << endl;

	outputFile << "		lv2:symbol " << char(34) << "lfo3_dco3_pitch" << char(34) << " ;" << endl;
	outputFile << "		pset:value " << lfo3->gui_dco3_pitch->value << endl;
	outputFile << "		] , [ " << endl;

	outputFile << "		lv2:symbol " << char(34) << "lfo3_filter" << char(34) << " ;" << endl;
	outputFile << "		pset:value " << lfo3->gui_cutoff->value << endl;
	outputFile << "		] , [ " << endl;

	outputFile << "		lv2:symbol " << char(34) << "lfo3_route_one" << char(34) << " ;" << endl;
	outputFile << "		pset:value " << lfo3->gui_route1->value << endl;
	outputFile << "		] , [ " << endl;

	outputFile << "		lv2:symbol " << char(34) << "lfo3_route_one_dest" << char(34) << " ;" << endl;
	outputFile << "		pset:value " << lfo3->gui_route1->route_number  << endl;
	outputFile << "		] , [ " << endl;

	outputFile << "		lv2:symbol " << char(34) << "lfo3_route_two" << char(34) << " ;" << endl;
	outputFile << "		pset:value " << lfo3->gui_route2->value << endl;
	outputFile << "		] , [ " << endl;

	outputFile << "		lv2:symbol " << char(34) << "lfo3_route_two_dest" << char(34) << " ;" << endl;
	outputFile << "		pset:value " << lfo3->gui_route2->route_number  << endl;
	outputFile << "		] , [ " << endl;

	// FX ECHO

	outputFile << "		lv2:symbol " << char(34) << "fx_echo_active" << char(34) << " ;" << endl;
	outputFile << "		pset:value " << echo->gui_active->val  << endl;
	outputFile << "		] , [ " << endl;

	outputFile << "		lv2:symbol " << char(34) << "fx_echo_speed" << char(34) << " ;" << endl;
	outputFile << "		pset:value " << echo->gui_speed->value  << endl;
	outputFile << "		] , [ " << endl;

	outputFile << "		lv2:symbol " << char(34) << "fx_echo_decay" << char(34) << " ;" << endl;
	outputFile << "		pset:value " << echo->gui_decay->value  << endl;
	outputFile << "		] , [ " << endl;

	outputFile << "		lv2:symbol " << char(34) << "fx_echo_eq_low" << char(34) << " ;" << endl;
	outputFile << "		pset:value " << echo->gui_eq_low->value  << endl;
	outputFile << "		] , [ " << endl;

	outputFile << "		lv2:symbol " << char(34) << "fx_echo_eq_mid" << char(34) << " ;" << endl;
	outputFile << "		pset:value " << echo->gui_eq_mid->value  << endl;
	outputFile << "		] , [ " << endl;

	outputFile << "		lv2:symbol " << char(34) << "fx_echo_eq_high" << char(34) << " ;" << endl;
	outputFile << "		pset:value " << echo->gui_eq_high->value  << endl;
	outputFile << "		] , [ " << endl;

	// FX REVERB

	outputFile << "		lv2:symbol " << char(34) << "fx_reverb_active" << char(34) << " ;" << endl;
	outputFile << "		pset:value " << reverb->gui_active->val  << endl;
	outputFile << "		] , [ " << endl;

	outputFile << "		lv2:symbol " << char(34) << "fx_reverb_decay" << char(34) << " ;" << endl;
	outputFile << "		pset:value " << reverb->gui_decay->value  << endl;
	outputFile << "		] , [ " << endl;

	outputFile << "		lv2:symbol " << char(34) << "fx_reverb_mix" << char(34) << " ;" << endl;
	outputFile << "		pset:value " << reverb->gui_mix->value  << endl;
	outputFile << "		] , [ " << endl;

	outputFile << "		lv2:symbol " << char(34) << "fx_reverb_mix" << char(34) << " ;" << endl;
	outputFile << "		pset:value " << reverb->gui_mix->value  << endl;
	outputFile << "		] , [ " << endl;

	// FX MODIFIERS

	outputFile << "		lv2:symbol " << char(34) << "modifier_ring" << char(34) << " ;" << endl;
	outputFile << "		pset:value " << modifier->gui_modifier_ring->val  << endl;
	outputFile << "		] , [ " << endl;

	outputFile << "		lv2:symbol " << char(34) << "stereo_mode" << char(34) << " ;" << endl;
	outputFile << "		pset:value " << modifier->gui_modifier_stereo_mode->val  << endl;
	outputFile << "		] , [ " << endl;

	outputFile << "		lv2:symbol " << char(34) << "modifier_dirt" << char(34) << " ;" << endl;
	outputFile << "		pset:value " << modifier->gui_modifier_dirt->value  << endl;

	outputFile << 	"] . " << endl;

	outputFile.close();

	// WRITE MANIFEST

	outputFile.open( new_preset_manifest.str().substr(9) );
	outputFile << "@prefix atom: <http://lv2plug.in/ns/ext/atom#> ." << endl;
	outputFile << "@prefix lv2: <http://lv2plug.in/ns/lv2core#> ." << endl;
	outputFile << "@prefix pset: <http://lv2plug.in/ns/ext/presets#> ." << endl;
	outputFile << "@prefix rdf: <http://www.w3.org/1999/02/22-rdf-syntax-ns#> ." << endl;
	outputFile << "@prefix rdfs: <http://www.w3.org/2000/01/rdf-schema#> ." << endl;
	outputFile << "@prefix state: <http://lv2plug.in/ns/ext/state#> ." << endl;
	outputFile << "@prefix xsd: <http://www.w3.org/2001/XMLSchema#> ." << endl << endl;
	outputFile << "	<" << temp_str << ".ttl>" << endl;
	outputFile << "	a pset:Preset ;" << endl;
	outputFile << "	rdfs:seeAlso <" << temp_str << ".ttl> ;" << endl;
	outputFile << "	lv2:appliesTo <http://nickbailey.co.nr/triceratops> ." << endl;
	outputFile.close();

	// load default presets
	preset_file_names.erase (preset_file_names.begin(),preset_file_names.begin()+preset_file_names.size());
	get_preset_list("/usr/lib/lv2/triceratops-presets.lv2/");
	get_preset_list("/usr/local/lib/lv2/triceratops-presets.lv2/");
	// load local presets
	stringstream home_lv2;
	home_lv2.str("");
	home_lv2 << getenv("HOME") << "/.lv2/";
	get_preset_list_local(home_lv2.str());
	std::sort(preset_file_names.begin(), preset_file_names.end());

	for (int x=0; x < preset_file_names.size(); ++x)
	{
		if (preset_file_names[x] == "." + temp_str + ".lv2") 
		{
			current_preset_offset = x;
			current_preset = x;
			queue_draw();	
		}
	}
}



bool preset_select::on_expose_event(GdkEventExpose* event)
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
	        back_grad->add_color_stop_rgba(0,top_colour.get_red_p()/2,top_colour.get_green_p()/2,top_colour.get_blue_p()/2,1);
	        back_grad->add_color_stop_rgba(1,bottom_colour.get_red_p()/2,bottom_colour.get_green_p()/2,bottom_colour.get_blue_p()/2,1);
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
	
		cr->set_font_size(width/10);

		for (int x=0; x<(height/20)+2; x++)
		{
			if (x != current_preset - current_preset_offset) cr->set_source_rgba(0.9,0.9,0.9,0.6);
				else cr->set_source_rgba(0.9,0.9,0.9,1.0);
			cr->move_to(5,(x*20)+width/2);
			if (x+current_preset_offset < preset_file_names.size())
			{
				string preset_str = preset_file_names[x+current_preset_offset];
       				cr->show_text(preset_str);
			}
		}

    		Cairo::FontOptions font_options;
    		font_options.set_hint_style(Cairo::HINT_STYLE_NONE);
    		font_options.set_hint_metrics(Cairo::HINT_METRICS_OFF);
    		font_options.set_antialias(Cairo::ANTIALIAS_GRAY);

		back_grad = Cairo::LinearGradient::create( 0,allocation.get_height() - 30,0,allocation.get_height() );
	        back_grad->add_color_stop_rgba(0,top_colour.get_red_p(),top_colour.get_green_p(),top_colour.get_blue_p(),1);
	        back_grad->add_color_stop_rgba(1,bottom_colour.get_red_p(),bottom_colour.get_green_p(),bottom_colour.get_blue_p(),1);
		cr->set_source(back_grad);

    		cr->rectangle(event->area.x, allocation.get_height() - 45,event->area.width, 50);
		cr->fill();

		back_grad = Cairo::LinearGradient::create( 0,0,0,30 );
	        back_grad->add_color_stop_rgba(0,top_colour.get_red_p(),top_colour.get_green_p(),top_colour.get_blue_p(),1);
	        back_grad->add_color_stop_rgba(1,bottom_colour.get_red_p(),bottom_colour.get_green_p(),bottom_colour.get_blue_p(),1);
		cr->set_source(back_grad);

    		cr->rectangle(event->area.x, 0,event->area.width, 30);
		cr->fill();

		cr->select_font_face("Bitstream Vera Sans", Cairo::FONT_SLANT_NORMAL,
		Cairo::FONT_WEIGHT_NORMAL);
		cr->set_font_size(width/8);

		cr->move_to(5,width/6);
		cr->set_source_rgba(0.9,0.9,0.9,1.0);
       		cr->show_text("<  Presets  >");

    		cr->set_font_options(font_options);
    		cr->move_to(5,allocation.get_height() - 30);
		cr->set_font_size(width/8);
		cr->set_source_rgba(0.9,0.9,0.9,0.8);
    		cr->show_text("New preset");
	
    		cr->set_font_options(font_options);
    		cr->move_to(5,allocation.get_height() - 10);
		cr->set_font_size(width/11);
		cr->set_source_rgba(0.9,0.9,0.9,0.8);
    		cr->show_text(text + "_");

	}

  return true;
}


