

#include "synth.h"

// CONSTRUCTOR

//-------------------------------------------------------------
/*
Public source code by alex@smartelectronix.com
Simple example of implementation of formant filter
Vowelnum can be 0,1,2,3,4 <=> A,E,I,O,U
Good for spectral rich input like saw or square 
*/
//-------------------------------------------------------------VOWEL COEFFICIENTS

const double coeff[5][11] = {
	{ 3.11044e-06,
	8.943665402,    -36.83889529,    92.01697887,    -154.337906,    181.6233289,
	-151.8651235,   89.09614114,    -35.10298511,    8.388101016,    -0.923313471  ///A
	},
	{4.36215e-06,
	8.90438318,    -36.55179099,    91.05750846,    -152.422234,    179.1170248,  ///E
	-149.6496211,87.78352223,    -34.60687431,    8.282228154,    -0.914150747
	},
	{ 3.33819e-06,
	8.893102966,    -36.49532826,    90.96543286,    -152.4545478,    179.4835618,
	-150.315433,    88.43409371,    -34.98612086,    8.407803364,    -0.932568035  ///I
	},
	{1.13572e-06,
	8.994734087,    -37.2084849,    93.22900521,    -156.6929844,    184.596544,   ///O
	-154.3755513,    90.49663749,    -35.58964535,    8.478996281,    -0.929252233
	},
	{4.09431e-07,
	8.997322763,    -37.20218544,    93.11385476,    -156.2530937,    183.7080141,  ///U
	-153.2631681,    89.59539726,    -35.12454591,    8.338655623,    -0.910251753
	}
}; 


synth::synth( double _rate,string bundle_path)
{
	for (int x=0; x<10; ++x)
	{
		memory[x] = 0;
	}

	stereo_mode = true;

	synth_params = (params*)malloc(sizeof(params));

	update_counter = 0;
	update_speed = 64;
	out_fm = 0;
	rate = _rate;
	dco_count_one = 0;
	env_amp_level_db = 0;
	midi_key = -1;
	
	for (int dco=0; dco<6; ++dco)
	{
		lpO[dco] = osc_Create(rate);
		lpO[dco]->type = OT_SAW;
		lpO[dco]->bSync = false;

		sinewave_osc[dco] = new LFO(rate);
		sinewave_osc[dco]->setWaveform(LFO::sinus);
	}

	nixnoise = new noise();

	dco_frequency = 440;
	dc1 = 0;
	dc2 = 0;
	dc3 = 0;

	stringstream ss;
	ss.str("");
	ss << bundle_path << "minblep.mat";

	// load table
	FILE *fp=fopen(ss.str().c_str(),"rb");
	unsigned int iSize;

	fseek(fp,0x134,SEEK_SET);

	int err = fread(&iSize,sizeof(uint),1,fp);
	gMinBLEP.c=iSize/sizeof(double);

	gMinBLEP.lpTable=(double*)malloc(iSize);

	err = fread(gMinBLEP.lpTable,iSize,1,fp);

	fclose(fp);

	buf0_left=0; buf1_left=0;

	f_left=0; pc_left=0; q_left=0;             //filter coefficients
  	bf0_left=0; bf1_left=0; bf2_left=0; bf3_left=0; bf4_left=0;  //filter buffers (beware denormals!)
  	t1_left=0; t2_left=0;              //temporary buffers

	selectivity_left=90, gain1_left=0, gain2_left=0.25, ratio_left=7, cap_left=0; 

	env_amp_state = env_state_dormant;
	env_filter_state = env_state_dormant;
	env_lfo_state = env_state_dormant;

	env_amp_level = 0;
	env_filter_level = 0;
	env_lfo_level = 0;

}

// DECONSTRUCTOR

synth::~synth()
{

	for (int dco=0; dco<6; ++dco)
	{
		free( lpO[dco] );
		free( sinewave_osc[dco] );
	}

	delete(synth_params);
	delete(nixnoise);

}

// RUN

void synth::run(float* out_left, float* out_right, uint32_t n_samples)
{

	if (synth_params->TRICERATOPS_ACTIVE_ONE[0]!=1 &&
		synth_params->TRICERATOPS_ACTIVE_TWO[0]!=1 &&
		synth_params->TRICERATOPS_ACTIVE_THREE[0]!=1) return;

	//------------------------------ envelope stuff

	float amp_attack = fast_pow(1-synth_params->TRICERATOPS_ATTACK_ONE[0],6);
	float amp_decay = fast_pow(1-synth_params->TRICERATOPS_DECAY_ONE[0],7);
	float amp_sustain = synth_params->TRICERATOPS_SUSTAIN_ONE[0];
	float amp_release = fast_pow(1-synth_params->TRICERATOPS_RELEASE_ONE[0],10);

	float filter_attack = fast_pow(1-synth_params->TRICERATOPS_ATTACK_TWO[0],6);
	float filter_decay = fast_pow(1-synth_params->TRICERATOPS_DECAY_TWO[0],7);
	float filter_sustain = synth_params->TRICERATOPS_SUSTAIN_TWO[0];
	float filter_release = fast_pow(1-synth_params->TRICERATOPS_RELEASE_TWO[0],10);

	float lfo_attack = fast_pow(1-synth_params->TRICERATOPS_ATTACK_THREE[0],6);
	float lfo_decay = fast_pow(1-synth_params->TRICERATOPS_DECAY_THREE[0],7);
	float lfo_sustain = synth_params->TRICERATOPS_SUSTAIN_THREE[0];
	float lfo_release = fast_pow(1-synth_params->TRICERATOPS_RELEASE_THREE[0],10);
	
	for (uint32_t pos = 0; pos < n_samples; pos++)
	{

	//------------------------------ oscillator stuff

	update_counter = ++update_counter % update_speed;

	if (update_counter == 0)
	{
		//--------- ADSR AMP
		// ATTACK

		switch (env_amp_state)
		{

		case env_state_begin:
			if (env_amp_level > 0) 
			{
				env_amp_level -= 0.001;
				if (env_amp_level < 0)
				{
					env_amp_level = 0;
				}
			}
			if (env_amp_level==0)
			{
				env_amp_state = env_state_attack;
			}
			break;

		case env_state_attack:
			if (env_amp_level < 1) 
			{
				env_amp_level += amp_attack;
				if (env_amp_level >= 1)
				{
					env_amp_level = 1;
					env_amp_state = env_state_decay;
				}
			}
			break;

		// DECAY / SUSTAIN

		case env_state_decay:
		{	
			if (env_amp_level > amp_sustain) 
			{
				env_amp_level -= amp_decay;
					if (env_amp_level <= amp_sustain)
					{
						env_amp_level = amp_sustain;
						env_amp_state = env_state_wait;
					}
			}

			if (env_amp_level <amp_sustain) 
			{
				env_amp_level += amp_decay;
					if (env_amp_level >=amp_sustain)
					{
						env_amp_level = amp_sustain;
						env_amp_state = env_state_wait;
					}
			}
			break;

		// RELEASE

		case env_state_release:
			if (env_amp_level > 0) 
			{
				env_amp_level -=amp_release;
				if (env_amp_level <= 0)
				{
					env_amp_level = 0;
					env_amp_state = env_state_dormant;
				}
			}
			break;
		}
	}

	env_amp_level_db = fast_pow(env_amp_level,3); 


		//--------- ADSR FILTER
		// ATTACK

		switch (env_filter_state)
		{

		case env_state_attack:
			if (env_filter_level < 1) 
			{
				env_filter_level += filter_attack;
				if (env_filter_level >= 1)
				{
					env_filter_level = 1;
					env_filter_state = env_state_decay;
				}
			}
			break;

		// DECAY / SUSTAIN

		case env_state_decay:
		{	
			if (env_filter_level > filter_sustain) 
			{
				env_filter_level -= filter_decay;
					if (env_filter_level <= filter_sustain)
					{
						env_filter_level = filter_sustain;
						env_filter_state = env_state_wait;
					}
			}

			if (env_filter_level <filter_sustain) 
			{
				env_filter_level += filter_decay;
					if (env_filter_level >=filter_sustain)
					{
						env_filter_level = filter_sustain;
						env_filter_state = env_state_wait;
					}
			}
			break;

		// RELEASE

		case env_state_release:
			if (env_filter_level > 0) 
			{
				env_filter_level -=filter_release;
				if (env_filter_level <= 0)
				{
					env_filter_level = 0;
					env_filter_state = env_state_dormant;
				}
			}
			break;
		}
	}
		//--------- ADSR LFO
		// ATTACK

		if (env_lfo_state == env_state_attack)
		{
			if (env_lfo_level < 1) 
			{
				env_lfo_level += lfo_attack;
				if (env_lfo_level >= 1)
				{
					env_lfo_level = 1;
					env_lfo_state = env_state_decay;
				}
			}
		}	
		
		// DECAY / SUSTAIN

		if (env_lfo_state == env_state_decay)
		{	
			if (env_lfo_level > lfo_sustain) 
			{
				env_lfo_level -= lfo_decay;
					if (env_lfo_level <= lfo_sustain)
					{
						env_lfo_level = lfo_sustain;
						env_lfo_state = env_state_wait;
					}
			}

			if (env_lfo_level <lfo_sustain) 
			{
				env_lfo_level += lfo_decay;
					if (env_lfo_level >=lfo_sustain)
					{
						env_lfo_level = lfo_sustain;
						env_lfo_state = env_state_wait;
					}
			}
		}

		// RELEASE

		if (env_lfo_state == env_state_release)
		{
			if (env_lfo_level > 0) 
			{
				env_lfo_level -=lfo_release;
				if (env_lfo_level <= 0)
				{
					env_lfo_level = 0;
					env_lfo_state = env_state_dormant;
				}
			}
		}	
	}

	float lfo1 = 0;
	float lfo2 = 0;
	float lfo3 = 0;

	if (synth_params->TRICERATOPS_ADSR3_LFO1_AMOUNT[0] == 0)
	{		
		lfo1 = lfo1_out[0][pos];
	}
	else 
	{
		lfo1 = lfo1_out[0][pos] * env_lfo_level;
	}

	if (synth_params->TRICERATOPS_ADSR3_LFO2_AMOUNT[0] == 0)
	{		
		lfo2 = lfo2_out[0][pos];
	}
	else 
	{
		lfo2 = lfo2_out[0][pos] * env_lfo_level;
	}


	if (synth_params->TRICERATOPS_ADSR3_LFO3_AMOUNT[0] == 0)
	{		
		lfo3 = lfo3_out[0][pos];
	}
		else 
	{
		lfo3 = lfo3_out[0][pos] * env_lfo_level;
	}


	if (update_counter == 0 || synth_params->TRICERATOPS_FM[0] == 1)
	{	

		//------------- oscillators

		float routes = 0;
		
		routes += (nixnoise->tick()*synth_params->TRICERATOPS_MODIFIER_DIRT[0]);

		routes += (synth_params->TRICERATOPS_ADSR1_ROUTE_ONE_DEST[0]==3) * synth_params->TRICERATOPS_ADSR1_ROUTE_ONE[0] * env_amp_level * 12;
		routes += (synth_params->TRICERATOPS_ADSR2_ROUTE_ONE_DEST[0]==3) * synth_params->TRICERATOPS_ADSR2_ROUTE_ONE[0] * env_filter_level * 12;
		routes += (synth_params->TRICERATOPS_ADSR1_ROUTE_TWO_DEST[0]==3) * synth_params->TRICERATOPS_ADSR1_ROUTE_TWO[0] * env_amp_level * 12;
		routes += (synth_params->TRICERATOPS_ADSR2_ROUTE_TWO_DEST[0]==3) * synth_params->TRICERATOPS_ADSR2_ROUTE_TWO[0] * env_filter_level * 12;
		routes += (synth_params->TRICERATOPS_LFO1_ROUTE_ONE_DEST[0]==3) * synth_params->TRICERATOPS_LFO1_ROUTE_ONE[0] * lfo1 * 12;
		routes += (synth_params->TRICERATOPS_LFO2_ROUTE_ONE_DEST[0]==3) * synth_params->TRICERATOPS_LFO2_ROUTE_ONE[0] * lfo2 * 12;
		routes += (synth_params->TRICERATOPS_LFO3_ROUTE_ONE_DEST[0]==3) * synth_params->TRICERATOPS_LFO3_ROUTE_ONE[0] * lfo3 * 12;
		routes += (synth_params->TRICERATOPS_LFO1_ROUTE_TWO_DEST[0]==3) * synth_params->TRICERATOPS_LFO1_ROUTE_TWO[0] * lfo1 * 12;
		routes += (synth_params->TRICERATOPS_LFO2_ROUTE_TWO_DEST[0]==3) * synth_params->TRICERATOPS_LFO2_ROUTE_TWO[0] * lfo2 * 12;
		routes += (synth_params->TRICERATOPS_LFO3_ROUTE_TWO_DEST[0]==3) * synth_params->TRICERATOPS_LFO3_ROUTE_TWO[0] * lfo3 * 12;

		routes += synth_params->TRICERATOPS_LFO1_DCO1_PITCH[0] * lfo1 * 12;
		routes += synth_params->TRICERATOPS_LFO2_DCO1_PITCH[0] * lfo2 * 12;
		routes += synth_params->TRICERATOPS_LFO3_DCO1_PITCH[0] * lfo3 * 12;

		if (synth_params->TRICERATOPS_FM[0] == 1) { routes+= out_fm; } 

		routes += synth_params->TRICERATOPS_MASTER_TUNE[0] + 0.22;
		routes += synth_params->TRICERATOPS_DETUNE_ONE[0];
		routes += (12 * synth_params->TRICERATOPS_OCTAVE_ONE[0]);

		float new_frequency = dco_frequency * powf(1.05946, routes + (pitch_bend[0] * 7) );
		if (new_frequency > 18000) new_frequency = 18000;

		if (synth_params->TRICERATOPS_INERTIA_ONE[0] > 0)
		{
			inertia_one.set_speed( (new_frequency * (1.1-synth_params->TRICERATOPS_INERTIA_ONE[0])) * 0.005 );
		}
		else { inertia_one.set_speed(1000); }

		if (synth_params->TRICERATOPS_WAVE_ONE[0] < 2)
		{
			lpO[0]->type = (oscwave_t)synth_params->TRICERATOPS_WAVE_ONE[0];
			lpO[0]->f = inertia_one.slide(new_frequency) - (synth_params->TRICERATOPS_UNISON_ONE[0] * synth_params->TRICERATOPS_UNISON_ACTIVATE[0]);

			if (synth_params->TRICERATOPS_UNISON_ACTIVATE[0] == 1)
			{
				lpO[3]->type = lpO[0]->type;
				lpO[3]->f = lpO[0]->f + synth_params->TRICERATOPS_UNISON_ONE[0];
			}

			routes = 0;
	
			routes += (synth_params->TRICERATOPS_ADSR1_ROUTE_ONE_DEST[0]==6) * synth_params->TRICERATOPS_ADSR1_ROUTE_ONE[0] * env_amp_level;
			routes += (synth_params->TRICERATOPS_ADSR1_ROUTE_ONE_DEST[0]==6) * synth_params->TRICERATOPS_ADSR1_ROUTE_ONE[0] * env_filter_level;
			routes += (synth_params->TRICERATOPS_ADSR2_ROUTE_TWO_DEST[0]==6) * synth_params->TRICERATOPS_ADSR2_ROUTE_TWO[0] * env_amp_level;
			routes += (synth_params->TRICERATOPS_ADSR2_ROUTE_TWO_DEST[0]==6) * synth_params->TRICERATOPS_ADSR2_ROUTE_TWO[0] * env_filter_level;
			routes += (synth_params->TRICERATOPS_LFO1_ROUTE_ONE_DEST[0]==6) * synth_params->TRICERATOPS_LFO1_ROUTE_ONE[0]* lfo1;
			routes += (synth_params->TRICERATOPS_LFO2_ROUTE_ONE_DEST[0]==6) * synth_params->TRICERATOPS_LFO2_ROUTE_ONE[0]* lfo2;
			routes += (synth_params->TRICERATOPS_LFO3_ROUTE_ONE_DEST[0]==6) * synth_params->TRICERATOPS_LFO3_ROUTE_ONE[0]* lfo3;
			routes += (synth_params->TRICERATOPS_LFO1_ROUTE_TWO_DEST[0]==6) * synth_params->TRICERATOPS_LFO1_ROUTE_TWO[0]* lfo1;
			routes += (synth_params->TRICERATOPS_LFO2_ROUTE_TWO_DEST[0]==6) * synth_params->TRICERATOPS_LFO2_ROUTE_TWO[0]* lfo2;
			routes += (synth_params->TRICERATOPS_LFO3_ROUTE_TWO_DEST[0]==6) * synth_params->TRICERATOPS_LFO3_ROUTE_TWO[0]* lfo3;

			routes += synth_params->TRICERATOPS_PULSEWIDTH_ONE[0]+0.5;

			if (routes < 0) routes = 0;
			if (routes > 1) routes = 1;
	
			lpO[0]->fPWM = routes;
			lpO[3]->fPWM = routes;

			if ((oscwave_t)synth_params->TRICERATOPS_WAVE_ONE[0] == OT_SAW)
			{
				dc1 = 0.5 + (lpO[0]->f * 0.000055);
			}
			else {
				dc1 = 0.5;// + (lpO[0]->f * 0.000003);
			}

		}
		else if (synth_params->TRICERATOPS_WAVE_ONE[0] == 2)
		{
			sinewave_osc[0]->setRate(inertia_one.slide(new_frequency) - (synth_params->TRICERATOPS_UNISON_ONE[0] * synth_params->TRICERATOPS_UNISON_ACTIVATE[0]));

			if (synth_params->TRICERATOPS_UNISON_ACTIVATE[0] == 1)
			{
				sinewave_osc[0]->setRate(inertia_one.slide(new_frequency) + synth_params->TRICERATOPS_UNISON_ONE[0]);
			}

		}

		routes = 0;

		routes += (nixnoise->tick()*synth_params->TRICERATOPS_MODIFIER_DIRT[0]);


		routes += (synth_params->TRICERATOPS_ADSR1_ROUTE_ONE_DEST[0]==4) * synth_params->TRICERATOPS_ADSR1_ROUTE_ONE[0] * env_amp_level * 12;
		routes += (synth_params->TRICERATOPS_ADSR2_ROUTE_ONE_DEST[0]==4) * synth_params->TRICERATOPS_ADSR2_ROUTE_ONE[0] * env_filter_level * 12;
		routes += (synth_params->TRICERATOPS_ADSR1_ROUTE_TWO_DEST[0]==4) * synth_params->TRICERATOPS_ADSR1_ROUTE_TWO[0] * env_amp_level * 12;
		routes += (synth_params->TRICERATOPS_ADSR2_ROUTE_TWO_DEST[0]==4) * synth_params->TRICERATOPS_ADSR2_ROUTE_TWO[0] * env_filter_level * 12;
		routes += (synth_params->TRICERATOPS_LFO1_ROUTE_ONE_DEST[0]==4) * synth_params->TRICERATOPS_LFO1_ROUTE_ONE[0]* lfo1 * 12;
		routes += (synth_params->TRICERATOPS_LFO2_ROUTE_ONE_DEST[0]==4) * synth_params->TRICERATOPS_LFO2_ROUTE_ONE[0]* lfo2 * 12;
		routes += (synth_params->TRICERATOPS_LFO3_ROUTE_ONE_DEST[0]==4) * synth_params->TRICERATOPS_LFO3_ROUTE_ONE[0]* lfo3 * 12;
		routes += (synth_params->TRICERATOPS_LFO1_ROUTE_TWO_DEST[0]==4) * synth_params->TRICERATOPS_LFO1_ROUTE_TWO[0]* lfo1 * 12;
		routes += (synth_params->TRICERATOPS_LFO2_ROUTE_TWO_DEST[0]==4) * synth_params->TRICERATOPS_LFO2_ROUTE_TWO[0]* lfo2 * 12;
		routes += (synth_params->TRICERATOPS_LFO3_ROUTE_TWO_DEST[0]==4) * synth_params->TRICERATOPS_LFO3_ROUTE_TWO[0]* lfo3 * 12;

		routes += synth_params->TRICERATOPS_LFO1_DCO2_PITCH[0] * lfo1 * 12;
		routes += synth_params->TRICERATOPS_LFO2_DCO2_PITCH[0] * lfo2 * 12;
		routes += synth_params->TRICERATOPS_LFO3_DCO2_PITCH[0] * lfo3 * 12;

		if (synth_params->TRICERATOPS_FM[0] == 1) { routes+= out_fm; } 

		routes += synth_params->TRICERATOPS_MASTER_TUNE[0] + 0.22;
		routes += synth_params->TRICERATOPS_DETUNE_TWO[0];
		routes += (12 * synth_params->TRICERATOPS_OCTAVE_TWO[0]);

		new_frequency = dco_frequency * powf(1.05946, routes + (pitch_bend[0] * 7) );

		if (new_frequency > 18000) new_frequency = 18000;

		if (synth_params->TRICERATOPS_INERTIA_TWO[0] > 0)
		{
			inertia_two.set_speed( (new_frequency * (1.1-synth_params->TRICERATOPS_INERTIA_TWO[0])) * 0.005 );
		}
		else { inertia_two.set_speed(1000); }

		if (synth_params->TRICERATOPS_WAVE_TWO[0] < 2)
		{
			lpO[1]->type = (oscwave_t)synth_params->TRICERATOPS_WAVE_TWO[0];
			lpO[1]->f = inertia_two.slide(new_frequency) - (synth_params->TRICERATOPS_UNISON_TWO[0] * synth_params->TRICERATOPS_UNISON_ACTIVATE[0] == 1);

			if (synth_params->TRICERATOPS_UNISON_ACTIVATE[0] == 1)
			{
				lpO[4]->type = lpO[1]->type;
				lpO[4]->f = lpO[1]->f + synth_params->TRICERATOPS_UNISON_TWO[0];
			}

			routes = 0;
			routes += (synth_params->TRICERATOPS_ADSR1_ROUTE_ONE_DEST[0]==7) * synth_params->TRICERATOPS_ADSR1_ROUTE_ONE[0] * env_amp_level;
			routes += (synth_params->TRICERATOPS_ADSR2_ROUTE_ONE_DEST[0]==7) * synth_params->TRICERATOPS_ADSR2_ROUTE_ONE[0] * env_filter_level;
			routes += (synth_params->TRICERATOPS_ADSR1_ROUTE_TWO_DEST[0]==7) * synth_params->TRICERATOPS_ADSR1_ROUTE_TWO[0] * env_amp_level;
			routes += (synth_params->TRICERATOPS_ADSR2_ROUTE_TWO_DEST[0]==7) * synth_params->TRICERATOPS_ADSR2_ROUTE_TWO[0] * env_filter_level;
			routes += (synth_params->TRICERATOPS_LFO1_ROUTE_ONE_DEST[0]==7) * synth_params->TRICERATOPS_LFO1_ROUTE_ONE[0]* lfo1;
			routes += (synth_params->TRICERATOPS_LFO2_ROUTE_ONE_DEST[0]==7) * synth_params->TRICERATOPS_LFO2_ROUTE_ONE[0]* lfo2;
			routes += (synth_params->TRICERATOPS_LFO3_ROUTE_ONE_DEST[0]==7) * synth_params->TRICERATOPS_LFO3_ROUTE_ONE[0]* lfo3;
			routes += (synth_params->TRICERATOPS_LFO1_ROUTE_TWO_DEST[0]==7) * synth_params->TRICERATOPS_LFO1_ROUTE_TWO[0]* lfo1;
			routes += (synth_params->TRICERATOPS_LFO2_ROUTE_TWO_DEST[0]==7) * synth_params->TRICERATOPS_LFO2_ROUTE_TWO[0]* lfo2;
			routes += (synth_params->TRICERATOPS_LFO3_ROUTE_TWO_DEST[0]==7) * synth_params->TRICERATOPS_LFO3_ROUTE_TWO[0]* lfo3;

			routes += synth_params->TRICERATOPS_PULSEWIDTH_TWO[0]+0.5;

			if (routes < 0) routes = 0;
			if (routes > 1) routes = 1;
	
			lpO[1]->fPWM = routes;
			lpO[4]->fPWM = routes;

			dc2 = 0.5 + (lpO[1]->f * 0.00005);
		}
		else if (synth_params->TRICERATOPS_WAVE_TWO[0] == 2)
		{
			sinewave_osc[1]->setRate(inertia_two.slide(new_frequency) - (synth_params->TRICERATOPS_UNISON_TWO[0] * synth_params->TRICERATOPS_UNISON_ACTIVATE[0]));

			if (synth_params->TRICERATOPS_UNISON_ACTIVATE[0] == 1)
			{
				sinewave_osc[1]->setRate(inertia_one.slide(new_frequency) + synth_params->TRICERATOPS_UNISON_TWO[0]);
			}

		}

		routes = 0;

		routes += (nixnoise->tick()*synth_params->TRICERATOPS_MODIFIER_DIRT[0]);

		routes += (synth_params->TRICERATOPS_ADSR1_ROUTE_ONE_DEST[0]==5) * synth_params->TRICERATOPS_ADSR1_ROUTE_ONE[0] * env_amp_level * 12;
		routes += (synth_params->TRICERATOPS_ADSR2_ROUTE_ONE_DEST[0]==5) * synth_params->TRICERATOPS_ADSR2_ROUTE_ONE[0] * env_filter_level * 12;
		routes += (synth_params->TRICERATOPS_ADSR1_ROUTE_TWO_DEST[0]==5) * synth_params->TRICERATOPS_ADSR1_ROUTE_TWO[0] * env_amp_level * 12;
		routes += (synth_params->TRICERATOPS_ADSR2_ROUTE_TWO_DEST[0]==5) * synth_params->TRICERATOPS_ADSR2_ROUTE_TWO[0] * env_filter_level * 12;
		routes += (synth_params->TRICERATOPS_LFO1_ROUTE_ONE_DEST[0]==5) * synth_params->TRICERATOPS_LFO1_ROUTE_ONE[0]* lfo1 * 12;
		routes += (synth_params->TRICERATOPS_LFO2_ROUTE_ONE_DEST[0]==5) * synth_params->TRICERATOPS_LFO2_ROUTE_ONE[0]* lfo2 * 12;
		routes += (synth_params->TRICERATOPS_LFO3_ROUTE_ONE_DEST[0]==5) * synth_params->TRICERATOPS_LFO3_ROUTE_ONE[0]* lfo3 * 12;
		routes += (synth_params->TRICERATOPS_LFO1_ROUTE_TWO_DEST[0]==5) * synth_params->TRICERATOPS_LFO1_ROUTE_TWO[0]* lfo1 * 12;
		routes += (synth_params->TRICERATOPS_LFO2_ROUTE_TWO_DEST[0]==5) * synth_params->TRICERATOPS_LFO2_ROUTE_TWO[0]* lfo2 * 12;
		routes += (synth_params->TRICERATOPS_LFO3_ROUTE_TWO_DEST[0]==5) * synth_params->TRICERATOPS_LFO3_ROUTE_TWO[0]* lfo3 * 12;

		routes += synth_params->TRICERATOPS_LFO1_DCO3_PITCH[0] * lfo1 * 12;
		routes += synth_params->TRICERATOPS_LFO2_DCO3_PITCH[0] * lfo2 * 12;
		routes += synth_params->TRICERATOPS_LFO3_DCO3_PITCH[0] * lfo3 * 12;

		routes += synth_params->TRICERATOPS_MASTER_TUNE[0] + 0.22;
		routes += synth_params->TRICERATOPS_DETUNE_THREE[0];
		routes += (12 * synth_params->TRICERATOPS_OCTAVE_THREE[0]);

		new_frequency = dco_frequency * powf(1.05946, routes + (pitch_bend[0] * 7) );
		if (new_frequency > 18000) new_frequency = 18000;

		if (synth_params->TRICERATOPS_INERTIA_THREE[0] > 0)
		{
			inertia_three.set_speed( (new_frequency * (1.1-synth_params->TRICERATOPS_INERTIA_THREE[0])) * 0.005 );
		}
		else { inertia_three.set_speed(1000); }

		if (synth_params->TRICERATOPS_WAVE_THREE[0] < 2)
		{
			lpO[2]->type = (oscwave_t)synth_params->TRICERATOPS_WAVE_THREE[0];
			lpO[2]->f = inertia_three.slide(new_frequency) - (synth_params->TRICERATOPS_UNISON_THREE[0] * synth_params->TRICERATOPS_UNISON_ACTIVATE[0]);

			if (synth_params->TRICERATOPS_UNISON_ACTIVATE[0] == 1)
			{
				lpO[5]->type = lpO[2]->type;
				lpO[5]->f = lpO[2]->f + synth_params->TRICERATOPS_UNISON_THREE[0];
			}

			routes = 0;
			routes += (synth_params->TRICERATOPS_ADSR1_ROUTE_ONE_DEST[0]==8) * synth_params->TRICERATOPS_ADSR1_ROUTE_ONE[0] * env_amp_level;
			routes += (synth_params->TRICERATOPS_ADSR2_ROUTE_ONE_DEST[0]==8) * synth_params->TRICERATOPS_ADSR2_ROUTE_ONE[0] * env_filter_level;
			routes += (synth_params->TRICERATOPS_ADSR1_ROUTE_TWO_DEST[0]==8) * synth_params->TRICERATOPS_ADSR1_ROUTE_TWO[0] * env_amp_level;
			routes += (synth_params->TRICERATOPS_ADSR2_ROUTE_TWO_DEST[0]==8) * synth_params->TRICERATOPS_ADSR2_ROUTE_TWO[0] * env_filter_level;
			routes += (synth_params->TRICERATOPS_LFO1_ROUTE_ONE_DEST[0]==8) * synth_params->TRICERATOPS_LFO1_ROUTE_ONE[0]* lfo1;
			routes += (synth_params->TRICERATOPS_LFO2_ROUTE_ONE_DEST[0]==8) * synth_params->TRICERATOPS_LFO2_ROUTE_ONE[0]* lfo2;
			routes += (synth_params->TRICERATOPS_LFO3_ROUTE_ONE_DEST[0]==8) * synth_params->TRICERATOPS_LFO3_ROUTE_ONE[0]* lfo3;
			routes += (synth_params->TRICERATOPS_LFO1_ROUTE_TWO_DEST[0]==8) * synth_params->TRICERATOPS_LFO1_ROUTE_TWO[0]* lfo1;
			routes += (synth_params->TRICERATOPS_LFO2_ROUTE_TWO_DEST[0]==8) * synth_params->TRICERATOPS_LFO2_ROUTE_TWO[0]* lfo2;
			routes += (synth_params->TRICERATOPS_LFO3_ROUTE_TWO_DEST[0]==8) * synth_params->TRICERATOPS_LFO3_ROUTE_TWO[0]* lfo3;

			routes += synth_params->TRICERATOPS_PULSEWIDTH_THREE [0]+0.5;

			if (routes < 0) routes = 0;
			if (routes > 1) routes = 1;
	
			lpO[2]->fPWM = routes;
			lpO[5]->fPWM = routes;

			dc3 = 0.5 + (lpO[2]->f * 0.00005);
		}
		else if (synth_params->TRICERATOPS_WAVE_THREE[0] == 2)
		{
			sinewave_osc[2]->setRate(inertia_three.slide(new_frequency) - (synth_params->TRICERATOPS_UNISON_THREE[0] * synth_params->TRICERATOPS_UNISON_ACTIVATE[0]));

			if (synth_params->TRICERATOPS_UNISON_ACTIVATE[0] == 1)
			{
				sinewave_osc[2]->setRate(inertia_one.slide(new_frequency) + synth_params->TRICERATOPS_UNISON_THREE[0]);
			}
		}
	}

	float out = 0;
	float outleft = 0;
	float outright = 0;


	if (synth_params->TRICERATOPS_ACTIVE_ONE[0]==1) // play oscillator one
	{

		float dco1_pan = synth_params->TRICERATOPS_DCO1_PAN[0];

		float routes = 0;

		routes += (synth_params->TRICERATOPS_LFO1_ROUTE_ONE_DEST[0]==9) * synth_params->TRICERATOPS_LFO1_ROUTE_ONE[0]* lfo1;
		routes += (synth_params->TRICERATOPS_LFO2_ROUTE_ONE_DEST[0]==9) * synth_params->TRICERATOPS_LFO2_ROUTE_ONE[0]* lfo2;
		routes += (synth_params->TRICERATOPS_LFO3_ROUTE_ONE_DEST[0]==9) * synth_params->TRICERATOPS_LFO3_ROUTE_ONE[0]* lfo3;
		routes += (synth_params->TRICERATOPS_LFO1_ROUTE_TWO_DEST[0]==9) * synth_params->TRICERATOPS_LFO1_ROUTE_TWO[0]* lfo1;
		routes += (synth_params->TRICERATOPS_LFO2_ROUTE_TWO_DEST[0]==9) * synth_params->TRICERATOPS_LFO2_ROUTE_TWO[0]* lfo2;
		routes += (synth_params->TRICERATOPS_LFO3_ROUTE_TWO_DEST[0]==9) * synth_params->TRICERATOPS_LFO3_ROUTE_TWO[0]* lfo3;	

		dco1_pan += routes;

		if (synth_params->TRICERATOPS_UNISON_ACTIVATE[0] == 0) 
		{
			float dco_out = 0;
			if (synth_params->TRICERATOPS_WAVE_ONE[0]<2) 
			{
				float v;
				float	 fs=lpO[0]->f / lpO[0]->s_rate;

				// create waveform
				lpO[0]->p=lpO[0]->p+fs;

				// add BLEP at end of waveform
				if (lpO[0]->p>=1)
				{
					lpO[0]->p=lpO[0]->p-1.0;
					lpO[0]->v=0.0f;
					osc_AddBLEP(lpO[0], lpO[0]->p/fs,1.0f);
				}

				// add BLEP in middle of wavefor for squarewave
				if (!lpO[0]->v && lpO[0]->p>lpO[0]->fPWM && lpO[0]->type==OT_SQUARE)
				{
					lpO[0]->v=1.0f;
					osc_AddBLEP(lpO[0], (lpO[0]->p-lpO[0]->fPWM)/fs,-1.0f);
				}

				// sample value
				if (lpO[0]->type==OT_SAW)
					v=lpO[0]->p;
				else
					v=lpO[0]->v;

				// add BLEP buffer contents
				if (lpO[0]->nInit)
				{
					v+=lpO[0]->buffer[lpO[0]->iBuffer];
					lpO[0]->nInit--;
				if (++lpO[0]->iBuffer>=lpO[0]->cBuffer) lpO[0]->iBuffer=0;
				}
	
				dco_out = v - dc1;
		}

		if (synth_params->TRICERATOPS_WAVE_ONE[0]==2) dco_out = sinewave_osc[0]->tick();
		if (synth_params->TRICERATOPS_WAVE_ONE[0]==3) dco_out = nixnoise->tick();


		if (synth_params->TRICERATOPS_MODIFIER_STEREO_MODE[0]==1)
		{
			outleft+= (dco_out *synth_params->TRICERATOPS_VOLUME_ONE[0]) * (1 - dco1_pan);
			outright+= (dco_out *synth_params->TRICERATOPS_VOLUME_ONE[0]) * dco1_pan;
		}
		else 
		{
			out+= (dco_out * synth_params->TRICERATOPS_VOLUME_ONE[0]) * 0.75;
		}

		}
		if (synth_params->TRICERATOPS_UNISON_ACTIVATE[0] == 1)  // UNISON ENABLED SO DO BOTH OSCILLATOR 0 and 3
		{
			float dco_out = 0;
			if (synth_params->TRICERATOPS_WAVE_ONE[0]<2) dco_out = osc_Play( lpO[0] ) - dc1;
			{
				float v;
				float fs=lpO[0]->f / lpO[0]->s_rate;

				// create waveform
				lpO[0]->p=lpO[0]->p+fs;

				// add BLEP at end of waveform
				if (lpO[0]->p>=1)
				{
					lpO[0]->p=lpO[0]->p-1.0;
					lpO[0]->v=0.0f;
					osc_AddBLEP(lpO[0], lpO[0]->p/fs,1.0f);
				}

				// add BLEP in middle of wavefor for squarewave
				if (!lpO[0]->v && lpO[0]->p>lpO[0]->fPWM && lpO[0]->type==OT_SQUARE)
				{
					lpO[0]->v=1.0f;
					osc_AddBLEP(lpO[0], (lpO[0]->p-lpO[0]->fPWM)/fs,-1.0f);
				}

				// sample value
				if (lpO[0]->type==OT_SAW)
					v=lpO[0]->p;
				else
					v=lpO[0]->v;

				// add BLEP buffer contents
				if (lpO[0]->nInit)
				{
					v+=lpO[0]->buffer[lpO[0]->iBuffer];
					lpO[0]->nInit--;
				if (++lpO[0]->iBuffer>=lpO[0]->cBuffer) lpO[0]->iBuffer=0;
				}
	
				dco_out = v - dc1;
			}

			if (synth_params->TRICERATOPS_WAVE_ONE[0]==2) dco_out = sinewave_osc[0]->tick();
			if (synth_params->TRICERATOPS_WAVE_ONE[0]==3) dco_out = nixnoise->tick();

			if (synth_params->TRICERATOPS_MODIFIER_STEREO_MODE[0]==1)
			{
				outleft+= (dco_out *synth_params->TRICERATOPS_VOLUME_ONE[0]) * (1 - dco1_pan);
				outright+= (dco_out *synth_params->TRICERATOPS_VOLUME_ONE[0]) * dco1_pan;
			}
			else 
			{
				out+= (dco_out * synth_params->TRICERATOPS_VOLUME_ONE[0]) * 0.75;
			}

			dco_out = 0;
			if (synth_params->TRICERATOPS_WAVE_ONE[0]<2) 

			{
				float v;
				float fs=lpO[3]->f / lpO[3]->s_rate;

				// create waveform
				lpO[3]->p=lpO[3]->p+fs;

				// add BLEP at end of waveform
				if (lpO[3]->p>=1)
				{
					lpO[3]->p=lpO[3]->p-1.0;
					lpO[3]->v=0.0f;
					osc_AddBLEP(lpO[3], lpO[3]->p/fs,1.0f);
				}

				// add BLEP in middle of wavefor for squarewave
				if (!lpO[3]->v && lpO[3]->p>lpO[3]->fPWM && lpO[3]->type==OT_SQUARE)
				{
					lpO[3]->v=1.0f;
					osc_AddBLEP(lpO[3], (lpO[3]->p-lpO[3]->fPWM)/fs,-1.0f);
				}

				// sample value
				if (lpO[3]->type==OT_SAW)
					v=lpO[3]->p;
				else
					v=lpO[3]->v;

				// add BLEP buffer contents
				if (lpO[3]->nInit)
				{
					v+=lpO[3]->buffer[lpO[3]->iBuffer];
					lpO[3]->nInit--;
				if (++lpO[3]->iBuffer>=lpO[3]->cBuffer) lpO[3]->iBuffer=0;
				}
	
				dco_out = v - dc1;
		}

			if (synth_params->TRICERATOPS_WAVE_ONE[0]==2) dco_out = sinewave_osc[1]->tick();
			if (synth_params->TRICERATOPS_WAVE_ONE[0]==3) dco_out = nixnoise->tick();

			if (synth_params->TRICERATOPS_MODIFIER_STEREO_MODE[0]==1)
			{
				outleft+= (dco_out *synth_params->TRICERATOPS_VOLUME_ONE[0]) * (1 - dco1_pan);
				outright+= (dco_out *synth_params->TRICERATOPS_VOLUME_ONE[0]) * dco1_pan;
			}
			else 
			{
				out+= (dco_out * synth_params->TRICERATOPS_VOLUME_ONE[0]) * 0.75;
			}
		}
	}

	if (synth_params->TRICERATOPS_ACTIVE_TWO[0]==1) // play oscillator two
	{

		float dco2_pan = synth_params->TRICERATOPS_DCO2_PAN[0];

		float routes = 0;

		routes += (synth_params->TRICERATOPS_LFO1_ROUTE_ONE_DEST[0]==10) * synth_params->TRICERATOPS_LFO1_ROUTE_ONE[0] * lfo1;
		routes += (synth_params->TRICERATOPS_LFO2_ROUTE_ONE_DEST[0]==10) * synth_params->TRICERATOPS_LFO2_ROUTE_ONE[0] * lfo2;
		routes += (synth_params->TRICERATOPS_LFO3_ROUTE_ONE_DEST[0]==10) * synth_params->TRICERATOPS_LFO3_ROUTE_ONE[0] * lfo3;
		routes += (synth_params->TRICERATOPS_LFO1_ROUTE_TWO_DEST[0]==10) * synth_params->TRICERATOPS_LFO1_ROUTE_TWO[0] * lfo1;
		routes += (synth_params->TRICERATOPS_LFO2_ROUTE_TWO_DEST[0]==10) * synth_params->TRICERATOPS_LFO2_ROUTE_TWO[0] * lfo2;
		routes += (synth_params->TRICERATOPS_LFO3_ROUTE_TWO_DEST[0]==10) * synth_params->TRICERATOPS_LFO3_ROUTE_TWO[0] * lfo3;	

		dco2_pan += routes;

		if (synth_params->TRICERATOPS_UNISON_ACTIVATE[0] == 0)
		{
			float dco_out = 0;
			if (synth_params->TRICERATOPS_WAVE_TWO[0]<2)
			{
				float v;
				float fs=lpO[1]->f / lpO[1]->s_rate;

				// create waveform
				lpO[1]->p=lpO[1]->p+fs;

				// add BLEP at end of waveform
				if (lpO[1]->p>=1)
				{
					lpO[1]->p=lpO[1]->p-1.0;
					lpO[1]->v=0.0f;
					osc_AddBLEP(lpO[1], lpO[1]->p/fs,1.0f);
				}

				// add BLEP in middle of wavefor for squarewave
				if (!lpO[1]->v && lpO[1]->p>lpO[1]->fPWM && lpO[1]->type==OT_SQUARE)
				{
					lpO[1]->v=1.0f;
					osc_AddBLEP(lpO[1], (lpO[1]->p-lpO[1]->fPWM)/fs,-1.0f);
				}

				// sample value
				if (lpO[1]->type==OT_SAW)
					v=lpO[1]->p;
				else
					v=lpO[1]->v;

				// add BLEP buffer contents
				if (lpO[1]->nInit)
				{
					v+=lpO[1]->buffer[lpO[1]->iBuffer];
					lpO[1]->nInit--;
				if (++lpO[1]->iBuffer>=lpO[1]->cBuffer) lpO[1]->iBuffer=0;
				}
	
				dco_out = v - dc1;
			}

			if (synth_params->TRICERATOPS_WAVE_TWO[0]==2) dco_out = sinewave_osc[1]->tick();
			if (synth_params->TRICERATOPS_WAVE_TWO[0]==3) dco_out = nixnoise->tick();

			if (synth_params->TRICERATOPS_MODIFIER_STEREO_MODE[0]==1)
			{
				if (synth_params->TRICERATOPS_MODIFIER_RING[0]==1)
				{
					outleft*= 4*(dco_out *synth_params->TRICERATOPS_VOLUME_TWO[0]) * (1 - dco2_pan);
					outright*= 4*(dco_out *synth_params->TRICERATOPS_VOLUME_TWO[0]) * dco2_pan;
				}
				else
				{
					outleft+= (dco_out *synth_params->TRICERATOPS_VOLUME_TWO[0]) * (1 - dco2_pan);
					outright+= (dco_out *synth_params->TRICERATOPS_VOLUME_TWO[0]) * dco2_pan;
				}
			}
			else 
			{
				if (synth_params->TRICERATOPS_MODIFIER_RING[0]==1)
				{
					out*=4*(dco_out * synth_params->TRICERATOPS_VOLUME_TWO[0]) * 0.75;
				}
				else
				{
					out+= (dco_out * synth_params->TRICERATOPS_VOLUME_TWO[0]) * 0.75;
				}
			}

		}

		if (synth_params->TRICERATOPS_UNISON_ACTIVATE[0] == 1)
		{
			float dco_out = 0;
			if (synth_params->TRICERATOPS_WAVE_TWO[0]<2)
			{
				float v;
				float fs=lpO[1]->f / lpO[1]->s_rate;

				// create waveform
				lpO[1]->p=lpO[1]->p+fs;

				// add BLEP at end of waveform
				if (lpO[1]->p>=1)
				{
					lpO[1]->p=lpO[1]->p-1.0;
					lpO[1]->v=0.0f;
					osc_AddBLEP(lpO[1], lpO[1]->p/fs,1.0f);
				}

				// add BLEP in middle of wavefor for squarewave
				if (!lpO[1]->v && lpO[1]->p>lpO[1]->fPWM && lpO[1]->type==OT_SQUARE)
				{
					lpO[1]->v=1.0f;
					osc_AddBLEP(lpO[1], (lpO[1]->p-lpO[1]->fPWM)/fs,-1.0f);
				}

				// sample value
				if (lpO[1]->type==OT_SAW)
					v=lpO[1]->p;
				else
					v=lpO[1]->v;

				// add BLEP buffer contents
				if (lpO[1]->nInit)
				{
					v+=lpO[1]->buffer[lpO[1]->iBuffer];
					lpO[1]->nInit--;
				if (++lpO[1]->iBuffer>=lpO[1]->cBuffer) lpO[1]->iBuffer=0;
				}
	
				dco_out = v - dc1;
			}

			if (synth_params->TRICERATOPS_WAVE_TWO[0]==2) dco_out = sinewave_osc[1]->tick();
			if (synth_params->TRICERATOPS_WAVE_TWO[0]==3) dco_out = nixnoise->tick();

			if (synth_params->TRICERATOPS_MODIFIER_STEREO_MODE[0]==1)
			{
				outleft+= (dco_out *synth_params->TRICERATOPS_VOLUME_TWO[0]) * (1 - dco2_pan);
				outright+= (dco_out *synth_params->TRICERATOPS_VOLUME_TWO[0]) * dco2_pan;
			}
			else 
			{
				out+= (dco_out * synth_params->TRICERATOPS_VOLUME_TWO[0]) * 0.75;
			}

			dco_out = 0;
			if (synth_params->TRICERATOPS_WAVE_TWO[0]<2)
			{
				float v;
				float fs=lpO[4]->f / lpO[4]->s_rate;

				// create waveform
				lpO[4]->p=lpO[4]->p+fs;

				// add BLEP at end of waveform
				if (lpO[4]->p>=1)
				{
					lpO[4]->p=lpO[4]->p-1.0;
					lpO[4]->v=0.0f;
					osc_AddBLEP(lpO[4], lpO[4]->p/fs,1.0f);
				}

				// add BLEP in middle of wavefor for squarewave
				if (!lpO[4]->v && lpO[4]->p>lpO[4]->fPWM && lpO[4]->type==OT_SQUARE)
				{
					lpO[4]->v=1.0f;
					osc_AddBLEP(lpO[4], (lpO[4]->p-lpO[4]->fPWM)/fs,-1.0f);
				}

				// sample value
				if (lpO[4]->type==OT_SAW)
					v=lpO[4]->p;
				else
					v=lpO[4]->v;

				// add BLEP buffer contents
				if (lpO[4]->nInit)
				{
					v+=lpO[4]->buffer[lpO[4]->iBuffer];
					lpO[4]->nInit--;
				if (++lpO[4]->iBuffer>=lpO[4]->cBuffer) lpO[4]->iBuffer=0;
				}
	
				dco_out = v - dc1;
			}

			if (synth_params->TRICERATOPS_WAVE_TWO[0]==2) dco_out = sinewave_osc[4]->tick();
			if (synth_params->TRICERATOPS_WAVE_TWO[0]==3) dco_out = nixnoise->tick();
	
			if (synth_params->TRICERATOPS_MODIFIER_STEREO_MODE[0]==1)
			{
				outleft+= (dco_out *synth_params->TRICERATOPS_VOLUME_TWO[0]) * (1 - dco2_pan);
				outright+= (dco_out *synth_params->TRICERATOPS_VOLUME_TWO[0]) * dco2_pan;
			}
			else 
			{
				out+= (dco_out * synth_params->TRICERATOPS_VOLUME_TWO[0]) * 0.75;
			}
		}
	}


	if (synth_params->TRICERATOPS_ACTIVE_THREE[0]==1) // play oscillator three
	{
		float dco_out = 0;
		float dco3_pan = synth_params->TRICERATOPS_DCO3_PAN[0];

		float routes = 0;

		routes += (synth_params->TRICERATOPS_LFO1_ROUTE_ONE_DEST[0]==11) * synth_params->TRICERATOPS_LFO1_ROUTE_ONE[0]* lfo1;
		routes += (synth_params->TRICERATOPS_LFO2_ROUTE_ONE_DEST[0]==11) * synth_params->TRICERATOPS_LFO2_ROUTE_ONE[0]* lfo2;
		routes += (synth_params->TRICERATOPS_LFO3_ROUTE_ONE_DEST[0]==11) * synth_params->TRICERATOPS_LFO3_ROUTE_ONE[0]* lfo3;
		routes += (synth_params->TRICERATOPS_LFO1_ROUTE_TWO_DEST[0]==11) * synth_params->TRICERATOPS_LFO1_ROUTE_TWO[0]* lfo1;
		routes += (synth_params->TRICERATOPS_LFO2_ROUTE_TWO_DEST[0]==11) * synth_params->TRICERATOPS_LFO2_ROUTE_TWO[0]* lfo2;
		routes += (synth_params->TRICERATOPS_LFO3_ROUTE_TWO_DEST[0]==11) * synth_params->TRICERATOPS_LFO3_ROUTE_TWO[0]* lfo3;	

		dco3_pan += routes;

		if (synth_params->TRICERATOPS_UNISON_ACTIVATE[0] == 0)
		{
			dco_out = 0;
			if (synth_params->TRICERATOPS_WAVE_THREE[0]<2)
			{
				float v;
				float fs=lpO[2]->f / lpO[2]->s_rate;

				// create waveform
				lpO[2]->p=lpO[2]->p+fs;

				// add BLEP at end of waveform
				if (lpO[2]->p>=1)
				{
					lpO[2]->p=lpO[2]->p-1.0;
					lpO[2]->v=0.0f;
					osc_AddBLEP(lpO[2], lpO[2]->p/fs,1.0f);
				}

				// add BLEP in middle of wavefor for squarewave
				if (!lpO[2]->v && lpO[2]->p>lpO[2]->fPWM && lpO[2]->type==OT_SQUARE)
				{
					lpO[2]->v=1.0f;
					osc_AddBLEP(lpO[2], (lpO[2]->p-lpO[2]->fPWM)/fs,-1.0f);
				}

				// sample value
				if (lpO[2]->type==OT_SAW)
					v=lpO[2]->p;
				else
					v=lpO[2]->v;

				// add BLEP buffer contents
				if (lpO[2]->nInit)
				{
					v+=lpO[2]->buffer[lpO[2]->iBuffer];
					lpO[2]->nInit--;
				if (++lpO[2]->iBuffer>=lpO[2]->cBuffer) lpO[2]->iBuffer=0;
				}
	
				dco_out = v - dc1;
			}

			if (synth_params->TRICERATOPS_WAVE_THREE[0]==2) dco_out = sinewave_osc[2]->tick();
			if (synth_params->TRICERATOPS_WAVE_THREE[0]==3) dco_out = nixnoise->tick();

			if (synth_params->TRICERATOPS_MODIFIER_STEREO_MODE[0]==1)
			{
				if (synth_params->TRICERATOPS_FM[0] == 1)
				{
					out_fm = (dco_out * synth_params->TRICERATOPS_VOLUME_THREE[0] * 16) * env_filter_level;
				}
				if (synth_params->TRICERATOPS_FM[0] == 0)
				{
					outleft+= (dco_out *synth_params->TRICERATOPS_VOLUME_THREE[0]) * (1 - dco3_pan);
					outright+= (dco_out *synth_params->TRICERATOPS_VOLUME_THREE[0]) * dco3_pan;;
				}
			}
			else 
			{
				if (synth_params->TRICERATOPS_FM[0] == 1)
				{
					out_fm = (dco_out * synth_params->TRICERATOPS_VOLUME_THREE[0] * 16) * env_filter_level;
				} 
				if (synth_params->TRICERATOPS_FM[0] == 0)
				{
					out +=  (dco_out * synth_params->TRICERATOPS_VOLUME_THREE[0]) * 0.75;
				}
			}




		}

		if (synth_params->TRICERATOPS_UNISON_ACTIVATE[0] == 1)
		{
			dco_out = 0;
			if (synth_params->TRICERATOPS_WAVE_THREE[0]<2)
			{
				float v;
				float fs=lpO[2]->f / lpO[2]->s_rate;

				// create waveform
				lpO[2]->p=lpO[2]->p+fs;

				// add BLEP at end of waveform
				if (lpO[2]->p>=1)
				{
					lpO[2]->p=lpO[2]->p-1.0;
					lpO[2]->v=0.0f;
					osc_AddBLEP(lpO[2], lpO[2]->p/fs,1.0f);
				}

				// add BLEP in middle of wavefor for squarewave
				if (!lpO[2]->v && lpO[2]->p>lpO[2]->fPWM && lpO[2]->type==OT_SQUARE)
				{
					lpO[2]->v=1.0f;
					osc_AddBLEP(lpO[2], (lpO[2]->p-lpO[2]->fPWM)/fs,-1.0f);
				}

				// sample value
				if (lpO[2]->type==OT_SAW)
					v=lpO[2]->p;
				else
					v=lpO[2]->v;

				// add BLEP buffer contents
				if (lpO[2]->nInit)
				{
					v+=lpO[2]->buffer[lpO[2]->iBuffer];
					lpO[2]->nInit--;
				if (++lpO[2]->iBuffer>=lpO[2]->cBuffer) lpO[2]->iBuffer=0;
				}
	
				dco_out = v - dc1;
			}

			if (synth_params->TRICERATOPS_WAVE_THREE[0]==2) dco_out = sinewave_osc[2]->tick();
			if (synth_params->TRICERATOPS_WAVE_THREE[0]==3) dco_out = nixnoise->tick();

			if (synth_params->TRICERATOPS_WAVE_THREE[0]<2)
			{
				float v;
				float fs=lpO[5]->f / lpO[5]->s_rate;

				// create waveform
				lpO[5]->p=lpO[5]->p+fs;

				// add BLEP at end of waveform
				if (lpO[5]->p>=1)
				{
					lpO[5]->p=lpO[5]->p-1.0;
					lpO[5]->v=0.0f;
					osc_AddBLEP(lpO[5], lpO[5]->p/fs,1.0f);
				}

				// add BLEP in middle of wavefor for squarewave
				if (!lpO[5]->v && lpO[5]->p>lpO[5]->fPWM && lpO[5]->type==OT_SQUARE)
				{
					lpO[5]->v=1.0f;
					osc_AddBLEP(lpO[5], (lpO[5]->p-lpO[5]->fPWM)/fs,-1.0f);
				}

				// sample value
				if (lpO[5]->type==OT_SAW)
					v=lpO[5]->p;
				else
					v=lpO[5]->v;

				// add BLEP buffer contents
				if (lpO[5]->nInit)
				{
					v+=lpO[5]->buffer[lpO[5]->iBuffer];
					lpO[5]->nInit--;
				if (++lpO[5]->iBuffer>=lpO[5]->cBuffer) lpO[5]->iBuffer=0;
				}
	
				dco_out += v - dc1;
			}

			if (synth_params->TRICERATOPS_WAVE_THREE[0]==2) dco_out += sinewave_osc[5]->tick();
			if (synth_params->TRICERATOPS_WAVE_THREE[0]==3) dco_out += nixnoise->tick();

			if (synth_params->TRICERATOPS_MODIFIER_STEREO_MODE[0]==1)
			{
				if (synth_params->TRICERATOPS_FM[0] == 1)
				{
					out_fm = (dco_out * synth_params->TRICERATOPS_VOLUME_THREE[0] * 16) * env_filter_level;
				}
				if (synth_params->TRICERATOPS_FM[0] == 0)
				{
					outleft+= (dco_out *synth_params->TRICERATOPS_VOLUME_THREE[0]) * (1 - dco3_pan);
					outright+= (dco_out *synth_params->TRICERATOPS_VOLUME_THREE[0]) * dco3_pan;;
				}
			}
			else 
			{
				if (synth_params->TRICERATOPS_FM[0] == 1)
				{
					out_fm = (dco_out * synth_params->TRICERATOPS_VOLUME_THREE[0] * 16) * env_filter_level;
				} 
				if (synth_params->TRICERATOPS_FM[0] == 0)
				{
					out +=  (dco_out * synth_params->TRICERATOPS_VOLUME_THREE[0]) * 0.75;
				}
			}
		}

	}

	if (synth_params->TRICERATOPS_SYNC[0] == 1) 
	{
		if (lpO[0]->p<0.1 ) { lpO[1]->p = 0; sinewave_osc[1]->phase = 0; }
		if (lpO[3]->p<0.1 ) { lpO[4]->p = 0; sinewave_osc[4]->phase = 0; }
	}

	
	//------------ filter

		if ((uint)synth_params->TRICERATOPS_FILTER_MODE != 0)
		{
			float routes = 0;

			routes += (synth_params->TRICERATOPS_LFO1_ROUTE_ONE_DEST[0]==1) * synth_params->TRICERATOPS_LFO1_ROUTE_ONE[0]* lfo1;
			routes += (synth_params->TRICERATOPS_LFO2_ROUTE_ONE_DEST[0]==1) * synth_params->TRICERATOPS_LFO2_ROUTE_ONE[0]* lfo2;
			routes += (synth_params->TRICERATOPS_LFO3_ROUTE_ONE_DEST[0]==1) * synth_params->TRICERATOPS_LFO3_ROUTE_ONE[0]* lfo3;
			routes += (synth_params->TRICERATOPS_LFO1_ROUTE_TWO_DEST[0]==1) * synth_params->TRICERATOPS_LFO1_ROUTE_TWO[0]* lfo1;
			routes += (synth_params->TRICERATOPS_LFO2_ROUTE_TWO_DEST[0]==1) * synth_params->TRICERATOPS_LFO2_ROUTE_TWO[0]* lfo2;
			routes += (synth_params->TRICERATOPS_LFO3_ROUTE_TWO_DEST[0]==1) * synth_params->TRICERATOPS_LFO3_ROUTE_TWO[0]* lfo3;	

			routes += synth_params->TRICERATOPS_LFO1_FILTER[0] * lfo1 * 12;
			routes += synth_params->TRICERATOPS_LFO2_FILTER[0] * lfo2 * 12;
			routes += synth_params->TRICERATOPS_LFO3_FILTER[0] * lfo3 * 12;

			routes *= 0.25;

			routes += (synth_params->TRICERATOPS_FILTER_KEY_FOLLOW[0] * (dco_frequency/1024));

			routes += (synth_params->TRICERATOPS_ADSR1_ROUTE_ONE_DEST[0]==1) * synth_params->TRICERATOPS_ADSR1_ROUTE_ONE[0] * env_amp_level * (velocity * 0.01);
			routes += (synth_params->TRICERATOPS_ADSR2_ROUTE_ONE_DEST[0]==1) * synth_params->TRICERATOPS_ADSR2_ROUTE_ONE[0] * env_filter_level * (velocity * 0.01);
			routes += (synth_params->TRICERATOPS_ADSR1_ROUTE_TWO_DEST[0]==1) * synth_params->TRICERATOPS_ADSR1_ROUTE_TWO[0] * env_amp_level * (velocity * 0.01);
			routes += (synth_params->TRICERATOPS_ADSR2_ROUTE_TWO_DEST[0]==1) * synth_params->TRICERATOPS_ADSR2_ROUTE_TWO[0] * env_filter_level * (velocity * 0.01);

			routes += synth_params->TRICERATOPS_CUTOFF[0];
			routes += (nixnoise->tick()*synth_params->TRICERATOPS_MODIFIER_DIRT[0] * 0.001);

			float frequency = fast_pow(routes,4);
			cutoff_modded = frequency;

			routes = 0;

			routes += (synth_params->TRICERATOPS_LFO1_ROUTE_ONE_DEST[0]==2) * synth_params->TRICERATOPS_LFO1_ROUTE_ONE[0]* lfo1;
			routes += (synth_params->TRICERATOPS_LFO2_ROUTE_ONE_DEST[0]==2) * synth_params->TRICERATOPS_LFO2_ROUTE_ONE[0]* lfo2;
			routes += (synth_params->TRICERATOPS_LFO3_ROUTE_ONE_DEST[0]==2) * synth_params->TRICERATOPS_LFO3_ROUTE_ONE[0]* lfo3;
			routes += (synth_params->TRICERATOPS_LFO1_ROUTE_TWO_DEST[0]==2) * synth_params->TRICERATOPS_LFO1_ROUTE_TWO[0]* lfo1;
			routes += (synth_params->TRICERATOPS_LFO2_ROUTE_TWO_DEST[0]==2) * synth_params->TRICERATOPS_LFO2_ROUTE_TWO[0]* lfo2;
			routes += (synth_params->TRICERATOPS_LFO3_ROUTE_TWO_DEST[0]==2) * synth_params->TRICERATOPS_LFO3_ROUTE_TWO[0]* lfo3;

			routes *= 0.25;

			routes += (synth_params->TRICERATOPS_ADSR1_ROUTE_ONE_DEST[0]==2) * synth_params->TRICERATOPS_ADSR1_ROUTE_ONE[0] * env_amp_level;
			routes += (synth_params->TRICERATOPS_ADSR2_ROUTE_ONE_DEST[0]==2) * synth_params->TRICERATOPS_ADSR2_ROUTE_ONE[0] * env_filter_level;
			routes += (synth_params->TRICERATOPS_ADSR1_ROUTE_TWO_DEST[0]==2) * synth_params->TRICERATOPS_ADSR1_ROUTE_TWO[0] * env_amp_level;
			routes += (synth_params->TRICERATOPS_ADSR2_ROUTE_TWO_DEST[0]==2) * synth_params->TRICERATOPS_ADSR2_ROUTE_TWO[0] * env_filter_level;
	
			routes += synth_params->TRICERATOPS_RESONANCE[0];
	
			float resonance = routes;
			float in = 0;

			if (synth_params->TRICERATOPS_MODIFIER_STEREO_MODE[0] == 0)
			{	
				in = (out * 0.8);
			}
			else 
			{
				in = (outleft * 0.8);
			}

			if (frequency > 0.6) { frequency = 0.6; }

			float in_left = in;

			// IN MONO MODE WE ONLY NEED ONE FILTER SO THIS SERVES AS BOTH MONO AND LEFT CHANNEL FOR STEREO MODE

			// Moog 24 dB/oct resonant lowpass VCF
			// References: CSound source code, Stilson/Smith CCRMA paper.
			// Modified by paul.kellett@maxim.abel.co.uk July 2000
	
			// Set coefficients given frequency & resonance [0.0...1.0]
	
			q_left = 1.0f - frequency;
			pc_left = frequency + 0.8f * frequency * q_left;
			f_left= pc_left + pc_left - 1.0f;
			q_left = resonance * (1.0f + 0.5f * q_left * (1.0f - q_left + 5.6f * q_left * q_left));
	

			// Filter (in [-1.0...+1.0])

			in -= q_left * bf4_left;                          //feedback
			t1_left = bf1_left;  bf1_left = (in + bf0_left) * pc_left - bf1_left * f_left;
			t2_left = bf2_left;  bf2_left = (bf1_left + t1_left) * pc_left - bf2_left * f_left;
			t1_left = bf3_left;  bf3_left = (bf2_left + t2_left) * pc_left - bf3_left * f_left;
		        bf4_left = (bf3_left + t1_left) * pc_left - bf4_left * f_left;
			bf4_left = bf4_left - bf4_left * bf4_left * bf4_left * 0.166667f;    //clipping
			bf0_left = in;

		
			// Lowpass  output:  bf4_left
			// Highpass output:  in - bf4_left;
			// Bandpass output:  3.0f * (bf3_left - bf4_left);

			if (  isnan(bf4_left) ) {bf1_left = 0; bf2_left = 0; bf3_left = 0; bf4_left = 0; in = 0; out = 0;}

			// STEREO MODE ENABLED - NEED TWO FILTERS, HERES HE RIGHT CHANNEL ONE

			float in_right = 0;

			if (synth_params->TRICERATOPS_MODIFIER_STEREO_MODE[0] == 1)
			{

				in = (outright * 0.8);
				in_right = in;

				// Moog 24 dB/oct resonant lowpass VCF
				// References: CSound source code, Stilson/Smith CCRMA paper.
				// Modified by paul.kellett@maxim.abel.co.uk July 2000
	
				// Set coefficients given frequency & resonance [0.0...1.0]
	
				q_right = 1.0f - frequency;
				pc_right = frequency + 0.8f * frequency * q_right;
				f_right= pc_right + pc_right - 1.0f;
				q_right = resonance * (1.0f + 0.5f * q_right * (1.0f - q_right + 5.6f * q_right * q_right));
	

				// Filter (in [-1.0...+1.0])

				in -= q_right * bf4_right;                          //feedback
				t1_right = bf1_right;  bf1_right = (in + bf0_right) * pc_right - bf1_right * f_right;
				t2_right = bf2_right;  bf2_right = (bf1_right + t1_right) * pc_right - bf2_right * f_right;
				t1_right = bf3_right;  bf3_right = (bf2_right + t2_right) * pc_right - bf3_right * f_right;
			        bf4_right = (bf3_right + t1_right) * pc_right - bf4_right * f_right;
				bf4_right = bf4_right - bf4_right * bf4_right * bf4_right * 0.166667f;    //clipping
				bf0_right = in;

		
				// Lowpass  output:  bf4_right
				// Highpass output:  in - bf4_right;
				// Bandpass output:  3.0f * (bf3_right - bf4_right);

				if (  isnan(bf4_right) ) {bf1_right = 0; bf2_right = 0; bf3_right = 0; bf4_right = 0; in = 0; out = 0;}
		}
		

		

	//------------ return audio

			if ((uint)synth_params->TRICERATOPS_FILTER_MODE[0] == 1)
			{
				out_left[pos] += (bf4_left * env_amp_level_db) * synth_params->TRICERATOPS_MASTER_VOLUME[0];
				if (synth_params->TRICERATOPS_MODIFIER_STEREO_MODE[0] == 0) out_right[pos] = out_left[pos];
					else out_right[pos] += (bf4_right * env_amp_level_db) * synth_params->TRICERATOPS_MASTER_VOLUME[0];
			}

			if ((uint)synth_params->TRICERATOPS_FILTER_MODE[0] == 2)
			{
				out_left[pos] += ((in_left-bf4_left) * env_amp_level_db) * synth_params->TRICERATOPS_MASTER_VOLUME[0];
				if (synth_params->TRICERATOPS_MODIFIER_STEREO_MODE[0] == 0)  out_right[pos] = out_left[pos];
					else out_right[pos] +=  ((in_right-bf4_right) * env_amp_level_db) * synth_params->TRICERATOPS_MASTER_VOLUME[0];
			}

			if ((uint)synth_params->TRICERATOPS_FILTER_MODE[0] == 3)
			{
				out_left[pos] += ((3.0F * (bf3_left-bf4_left)) * env_amp_level_db) * synth_params->TRICERATOPS_MASTER_VOLUME[0];
				if (synth_params->TRICERATOPS_MODIFIER_STEREO_MODE[0] == 0)  out_right[pos] = out_left[pos];
					else out_right[pos] +=  ((3.0F * (bf3_right-bf4_right)) * env_amp_level_db) * synth_params->TRICERATOPS_MASTER_VOLUME[0];
			}

			if ((uint)synth_params->TRICERATOPS_FILTER_MODE[0] == 4)
			{

				float crossfade = (cutoff_modded * 6.5);

				if (crossfade < 0) crossfade=0;
				if (crossfade > 4) crossfade=4;

				float fade = 0;

				int vowel1 = 0;
				int vowel2 = 0;
	
				if (crossfade >= 0 &&
				crossfade < 1)
				{
					fade = crossfade;
					vowel1 = 1;
					vowel2 = 0;	
				}	

				if (crossfade >= 1 &&
				crossfade < 2)
				{
					fade = 1 - (crossfade - 1);
					vowel1 = 1;
					vowel2 = 2;	
				}	

				if (crossfade >= 2 &&
					crossfade < 3)
				{
					fade = crossfade - 2;
					vowel1 = 3;
					vowel2 = 2;	
				}

				if (crossfade >= 3 &&
				crossfade <= 4)
				{
					fade = 1 - (crossfade - 3);
					vowel1 = 3;
					vowel2 = 4;	
				}


				morph_coeff[0] = (float)( (coeff[vowel1][0]
					-coeff[vowel2][0])*fade)
					+coeff[vowel2][0];

				morph_coeff[1] = (float)( (coeff[vowel1][1]
					-coeff[vowel2][1])*fade)
					+coeff[vowel2][1];

				morph_coeff[2] = (float)( (coeff[vowel1][2]
					-coeff[vowel2][2])*fade)
					+coeff[vowel2][2];

				morph_coeff[3] = (float)( (coeff[vowel1][3]
					-coeff[vowel2][3])*fade)
					+coeff[vowel2][3];

				morph_coeff[4] = (float)( (coeff[vowel1][4]
					-coeff[vowel2][4])*fade)
					+coeff[vowel2][4];

				morph_coeff[5] = (float)( (coeff[vowel1][5]
					-coeff[vowel2][5])*fade)
					+coeff[vowel2][5];

				morph_coeff[6] = (float)( (coeff[vowel1][6]
					-coeff[vowel2][6])*fade)
					+coeff[vowel2][6];

				morph_coeff[7] = (float)( (coeff[vowel1][7]
					-coeff[vowel2][7])*fade)
					+coeff[vowel2][7];

				morph_coeff[8] = (float)( (coeff[vowel1][8]
					-coeff[vowel2][8])*fade)
					+coeff[vowel2][8];
	
				morph_coeff[9] = (float)( (coeff[vowel1][9]
					-coeff[vowel2][9])*fade)
					+coeff[vowel2][9];

				morph_coeff[10] = (float)( (coeff[vowel1][10]
					-coeff[vowel2][10])*fade)
					+coeff[vowel2][10];

				if (synth_params->TRICERATOPS_MODIFIER_STEREO_MODE[0] == 1)  out = (outleft + outright);

				float res = (float) ( morph_coeff[0]  * out +
				morph_coeff[1]  *memory[0] +  
				morph_coeff[2]  *memory[1] +
				morph_coeff[3]  *memory[2] +
				morph_coeff[4]  *memory[3] +
				morph_coeff[5]  *memory[4] +
				morph_coeff[6]  *memory[5] +
				morph_coeff[7]  *memory[6] +
				morph_coeff[8]  *memory[7] +
				morph_coeff[9]  *memory[8] +
				morph_coeff[10] *memory[9] );

				if (std::isnan(in))
				{
					res = 0;
					for (int x=0; x<11; ++x) memory[x] = 0;
				}
	
				memory[9] = memory[8];
				memory[8] = memory[7];
				memory[7] = memory[6];
				memory[6] = memory[5];
				memory[5] = memory[4];
				memory[4] = memory[3];
				memory[3] = memory[2];
				memory[2] = memory[1];                     
				memory[1] = memory[0];
				memory[0] = (float) res;

				out_left[pos] += res * env_amp_level_db
					* synth_params->TRICERATOPS_MASTER_VOLUME[0];
				out_right[pos] = out_left[pos];
			}

		}

		if (synth_params->TRICERATOPS_FILTER_MODE[0] == 0) // BYPASS THE FILTER
		{
			if (synth_params->TRICERATOPS_MODIFIER_STEREO_MODE[0] == 0)  
			{
				out_left[pos] += ((out * env_amp_level_db) * synth_params->TRICERATOPS_MASTER_VOLUME[0])/3;
				out_right[pos] = out_left[pos];
			}
			else
			{
				out_left[pos] = ((outleft * env_amp_level_db) * synth_params->TRICERATOPS_MASTER_VOLUME[0])/3;
				out_right[pos] = ((outright * env_amp_level_db) * synth_params->TRICERATOPS_MASTER_VOLUME[0])/3;
			}
		}

	}	


	return;

}

//----------------------------------------------------------

osc_t* synth::osc_Create(double rate)
{
	osc_t *lpO=(osc_t*)malloc(sizeof(osc_t));

	// frequency
	lpO->f=440;
	lpO->p=0;
	lpO->v=0;

	// stuff
	lpO->bSync=false;
	lpO->fPWM=0.5;
	lpO->type=OT_SAW;
	lpO->s_rate = rate;

	// buffer
	lpO->cBuffer = 1024; //gMinBLEP.c/KTABLE;
	lpO->buffer=(double*)malloc(sizeof(double)*lpO->cBuffer);
	lpO->iBuffer=0;
	lpO->nInit=0;

	return lpO;
}

#define LERP(A,B,F) ((B-A)*F+A)

//----------------------------------------------------------

// add impulse into buffer
void synth::osc_AddBLEP(osc_t *lpO, double offset, double amp)
{
int i;
double *lpOut=lpO->buffer+lpO->iBuffer;
double *lpIn=gMinBLEP.lpTable+(uint)(KTABLE*offset);
double frac=fmod(KTABLE*offset,1.0);
int cBLEP=(gMinBLEP.c/KTABLE)-1;
double *lpBufferEnd=lpO->buffer+lpO->cBuffer;
double f;

	// add
	for (i=0; i<lpO->nInit; i++, lpIn+=KTABLE, lpOut++)
	{
		if (lpOut>=lpBufferEnd) lpOut=lpO->buffer;
		f=LERP(lpIn[0],lpIn[1],frac);
		*lpOut+=amp*(1-f);
	}

	// copy
	for (; i<cBLEP; i++, lpIn+=KTABLE, lpOut++)
	{
		if (lpOut>=lpBufferEnd) lpOut=lpO->buffer;
		f=LERP(lpIn[0],lpIn[1],frac);
		*lpOut=amp*(1-f);
	}

	lpO->nInit=cBLEP;
}

//----------------------------------------------------------

// play waveform
double synth::osc_Play(osc_t *lpO)
{

double v;
double fs=lpO->f / lpO->s_rate;

	// create waveform
	lpO->p=lpO->p+fs;

	// add BLEP at end of waveform
	if (lpO->p>=1)
	{
		lpO->p=lpO->p-1.0;
		lpO->v=0.0f;
		osc_AddBLEP(lpO, lpO->p/fs,1.0f);
	}

	// add BLEP in middle of wavefor for squarewave
	if (!lpO->v && lpO->p>lpO->fPWM && lpO->type==OT_SQUARE)
	{
		lpO->v=1.0f;
		osc_AddBLEP(lpO, (lpO->p-lpO->fPWM)/fs,-1.0f);
	}

	// sample value
	if (lpO->type==OT_SAW)
		v=lpO->p;
	else
		v=lpO->v;

	// add BLEP buffer contents
	if (lpO->nInit)
	{
		v+=lpO->buffer[lpO->iBuffer];
		lpO->nInit--;
		if (++lpO->iBuffer>=lpO->cBuffer) lpO->iBuffer=0;
	}
	
	return v;
}


//----------------------------------

void synth::minBLEP_Free()
{
	free(gMinBLEP.lpTable);
}


