#include "pcm.h"

void pcm_write_sample_16_short(void* buffer, const int position, const int sample) {
	//(2 ^ 16) / (2 ^ 16) = 1
	((short*)buffer)[position] = (short)sample;
}

void pcm_write_sample_16_float(void* buffer, const int position, const int sample) {
	//(2 ^ 16) = 65536
	((float*)buffer)[position] = (float)((sample + .5) / (65536 + .5));
}

void pcm_write_sample_24_short(void* buffer, const int position, const int sample) {
	//(2 ^ 16) / (2 ^ 24) = 0.00390625
	((short*)buffer)[position] = (short)(sample * 0.00390625);
}

void pcm_write_sample_24_float(void* buffer, const int position, const int sample) {
	//(2 ^ 24) = 16777216
	((float*)buffer)[position] = (float)((sample + .5) / (16777216 + .5));
}

void pcm_write_sample_32_short(void* buffer, const int position, const int sample) {
	//(2 ^ 16) / (2 ^ 32) = 0.00001525878
	((short*)buffer)[position] = (short)(sample * 0.00001525878);
}

void pcm_write_sample_32_float(void* buffer, const int position, const int sample) {
	//(2 ^ 32) = 4294967296
	((float*)buffer)[position] = (float)((sample + .5) / (4294967296 + .5));
}

PCM_WRITE_SAMPLE pcm_write_sample(const AUDIO_FORMAT input_format, const AUDIO_FORMAT output_format) {
	//Return a function which writes a sample in the input format to a buffer in the output format.
	switch (input_format.bits_per_sample) {
	case 16:
		switch (output_format.bits_per_sample) {
		case 16:
			return &pcm_write_sample_16_short;
		case 32:
			return &pcm_write_sample_16_float;
		}
	case 24:
		switch (output_format.bits_per_sample) {
		case 16:
			return &pcm_write_sample_24_short;
		case 32:
			return &pcm_write_sample_24_float;
		}
	case 32:
		switch (output_format.bits_per_sample) {
		case 16:
			return &pcm_write_sample_32_short;
		case 32:
			return &pcm_write_sample_32_float;
		}
	}
	//Not implemented.
	return NULL;
}