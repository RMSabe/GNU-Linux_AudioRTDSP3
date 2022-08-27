//Differential LR DSP. Output signal = L - R

#include <cerrno>
#include <cstring>
#include <fstream>
#include <iostream>
#include <thread>
#include <alsa/asoundlib.h>

//Audio File Directory
#define AUDIO_FILE_DIR "/home/username/Music/audio_file.raw"

//System ID for Audio Device. Set to "default" to use the default system output.
#define AUDIO_DEV "plughw:0,0"

#define BUFFER_SIZE 65536

#define BUFFER_SIZE_BYTES (2*BUFFER_SIZE)
#define BUFFER_SIZE_PER_CHANNEL (BUFFER_SIZE/2)

std::thread loadthread;
std::thread playthread;

std::fstream audio_file;
unsigned int file_size = 0;
unsigned int file_pos = 0;

snd_pcm_t *audio_dev = NULL;
snd_pcm_uframes_t n_frames;
unsigned int audio_buffer_size = 0;
unsigned int buffer_n_div = 1;
short **pp_startpoint = NULL;

short *buffer_input = NULL;
short *buffer_output_0 = NULL;
short *buffer_output_1 = NULL;
bool curr_buf_cycle = false;

short *load_out = NULL;
short *play_out = NULL;

int n_sample = 0;

bool stop = false;

void update_buf_cycle(void)
{
	curr_buf_cycle = !curr_buf_cycle;
	return;
}

void buffer_remap(void)
{
	if(curr_buf_cycle)
	{
		load_out = buffer_output_1;
		play_out = buffer_output_0;
	}
	else
	{
		load_out = buffer_output_0;
		play_out = buffer_output_1;
	}

	return;
}

void buffer_load(void)
{
	if(file_pos >= file_size)
	{
		stop = true;
		return;
	}
  
	audio_file.seekg(file_pos);
	audio_file.read((char*) buffer_input, BUFFER_SIZE_BYTES);
	file_pos += BUFFER_SIZE_BYTES;

	return;
}

void buffer_play(void)
{
	unsigned int n_div = 0;
	int n_return = 0;
	while(n_div < buffer_n_div)
	{
		n_return = snd_pcm_writei(audio_dev, pp_startpoint[n_div], n_frames);
		n_div++;
	}

	return;
}

void run_dsp(void)
{
	n_sample = 0;
	while(n_sample < BUFFER_SIZE_PER_CHANNEL)
	{
		load_out[2*n_sample] = buffer_input[2*n_sample] - buffer_input[2*n_sample + 1];
		load_out[2*n_sample + 1] = load_out[2*n_sample];
		n_sample++;
	}

	return;
}

void load_startpoints(void)
{
	pp_startpoint[0] = play_out;
	unsigned int n_div = 1;
	while(n_div < buffer_n_div)
	{
		pp_startpoint[n_div] = &play_out[n_div*audio_buffer_size];
		n_div++;
	}

	return;
}

void loadthread_proc(void)
{
	buffer_load();
	run_dsp();
	update_buf_cycle();
	return;
}

void playthread_proc(void)
{
	load_startpoints();
	buffer_play();
	return;
}

void buffer_preload(void)
{
	curr_buf_cycle = false;
	buffer_remap();

	buffer_load();
	run_dsp();
	update_buf_cycle();
	buffer_remap();

	return;
}

void playback(void)
{
	buffer_preload();
	while(!stop)
	{
		playthread = std::thread(playthread_proc);
		loadthread = std::thread(loadthread_proc);
		loadthread.join();
		playthread.join();
		buffer_remap();
	}
  
	return;
}

void buffer_malloc(void)
{
	audio_buffer_size = 2*n_frames;
	if(audio_buffer_size < BUFFER_SIZE) buffer_n_div = BUFFER_SIZE/audio_buffer_size;
	else buffer_n_div = 1;

	pp_startpoint = (short**) malloc(buffer_n_div*sizeof(short*));

	buffer_input = (short*) malloc(BUFFER_SIZE_BYTES);
	buffer_output_0 = (short*) malloc(BUFFER_SIZE_BYTES);
	buffer_output_1 = (short*) malloc(BUFFER_SIZE_BYTES);

	memset(buffer_input, 0, BUFFER_SIZE_BYTES);
	memset(buffer_output_0, 0, BUFFER_SIZE_BYTES);
	memset(buffer_output_1, 0, BUFFER_SIZE_BYTES);

	return;
}

void buffer_free(void)
{
	free(pp_startpoint);

	free(buffer_input);
	free(buffer_output_0);
	free(buffer_output_1);

	return;
}

bool open_audio_file(void)
{
	audio_file.open(AUDIO_FILE_DIR, std::ios_base::in);
	if(audio_file.is_open())
	{
		audio_file.seekg(0, audio_file.end);
		file_size = audio_file.tellg();
		file_pos = 0;
		audio_file.seekg(file_pos);
		return true;
	}
  
	return false;
}

bool audio_hw_init(void)
{
	int n_return;
	snd_pcm_hw_params_t *hw_params = NULL;
  
	n_return = snd_pcm_open(&audio_dev, AUDIO_DEV, SND_PCM_STREAM_PLAYBACK, 0);
	if(n_return < 0)
	{
		std::cout << "Error opening audio device\n";
		return false;
	}
  
	snd_pcm_hw_params_malloc(&hw_params);
	snd_pcm_hw_params_any(audio_dev, hw_params);
  
	n_return = snd_pcm_hw_params_set_access(audio_dev, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED);
	if(n_return < 0)
	{
		std::cout << "Error setting access to read/write interleaved\n";
		return false;
	}
  
	n_return = snd_pcm_hw_params_set_format(audio_dev, hw_params, SND_PCM_FORMAT_S16_LE);
	if(n_return < 0)
	{
		std::cout << "Error setting format to signed 16bit little-endian\n";
		return false;
	}
  
	n_return = snd_pcm_hw_params_set_channels(audio_dev, hw_params, 2);
	if(n_return < 0)
	{
		std::cout << "Error setting channels to stereo\n";
		return false;
	}
  
	unsigned int sample_rate = 44100;
	n_return = snd_pcm_hw_params_set_rate_near(audio_dev, hw_params, &sample_rate, 0);
	if(n_return < 0 || sample_rate < 44100)
	{
		std::cout << "Could not set sample rate to 44100 Hz\nAttempting to set sample rate to 48000 Hz\n";
		sample_rate = 48000;
		n_return = snd_pcm_hw_params_set_rate_near(audio_dev, hw_params, &sample_rate, 0);
		if(n_return < 0 || sample_rate < 48000)
		{
			std::cout << "Error setting sample rate\n";
			return false;
		}
	}
  
	n_return = snd_pcm_hw_params(audio_dev, hw_params);
	if(n_return < 0)
	{
		std::cout << "Error setting audio hardware parameters\n";
		return false;
	}
  
	snd_pcm_hw_params_get_period_size(hw_params, &n_frames, 0);
	snd_pcm_hw_params_free(hw_params);
	return true;
}

int main(int argc, char **argv)
{
	if(!audio_hw_init())
	{
		std::cout << "Error code: " << errno << "\nTerminated\n";
		return 0;
	}
	std::cout << "Audio hardware initialized\n";
  
	if(!open_audio_file())
	{
		std::cout << "Error opening audio file\nError code: " << errno << "\nTerminated\n";
		return 0;
	}
	std::cout << "Audio file is open\n";
  
	buffer_malloc();
  
	std::cout << "Playback started\n";
	playback();
	std::cout << "Playback finished\n";
  
	audio_file.close();
	snd_pcm_drain(audio_dev);
	snd_pcm_close(audio_dev);
	buffer_free();
	std::cout << "Terminated\n";
  
	return 0;
}
