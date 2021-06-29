#ifndef BASSDTS_H
#define BASSDTS_H

#include "../bass/bass.h"
#include "../bass/bass_addon.h"

#include "../libdcadec/dca_context.h"
#include "../libdcadec/dca_frame.h"

#ifndef BASSDTSDEF
#define BASSDTSDEF(f) WINAPI f
#endif

typedef struct {
	BYTE header[DCADEC_FRAME_HEADER_SIZE];
	BYTE* buffer;
	size_t size;
	UINT sync_word;
} DTS_FRAME;

typedef struct {
	BOOL initialized;
	QWORD frame_count;
	QWORD length;
	QWORD start;
	QWORD end;
	BOOL has_extensions;
} DTS_INFO;

typedef struct {
	BASSFILE bass_file;
	DTS_FRAME frame;
	DTS_INFO info;
} DTS_FILE;

typedef struct {
	int bits_per_sample;
	int bytes_per_sample;
	int samples_per_frame;
	double max_value;
} AUDIO_FORMAT;

typedef void(*PCM_WRITE_SAMPLE)(void* const buffer, const int position, const int sample);

typedef struct {
	int channel_count;
	int sample_rate;
	DTS_FILE* dts_file;
	struct dcadec_context* dcadec_context;
	int** samples;
	int sample_count;
	int sample_position;
	PCM_WRITE_SAMPLE write_sample;
	AUDIO_FORMAT input_format;
	AUDIO_FORMAT output_format;
} DTS_STREAM;

BOOL BASSDTSDEF(DllMain)(HANDLE dll, DWORD reason, LPVOID reserved);

VOID* BASSDTSDEF(BASSplugin)(DWORD face);

HSTREAM BASSDTSDEF(BASS_DTS_StreamCreate)(BASSFILE file, DWORD flags);

HSTREAM BASSDTSDEF(BASS_DTS_StreamCreateFile)(BOOL mem, const void* file, QWORD offset, QWORD length, DWORD flags);

HSTREAM BASSDTSDEF(BASS_DTS_StreamCreateURL)(const char* url, DWORD offset, DWORD flags, DOWNLOADPROC* proc, void* user);

DWORD BASSDTSDEF(BASS_DTS_StreamProc)(HSTREAM handle, void* buffer, DWORD length, void* user);

BOOL BASSDTSDEF(BASS_DTS_StreamWrite)(HSTREAM handle, void* buffer, DWORD* position, DWORD* remaining, void* user);

QWORD BASSDTSDEF(BASS_DTS_GetLength)(void* inst, DWORD mode);

VOID BASSDTSDEF(BASS_DTS_GetInfo)(void* inst, BASS_CHANNELINFO* info);

BOOL BASSDTSDEF(BASS_DTS_CanSetPosition)(void* inst, QWORD position, DWORD mode);

QWORD BASSDTSDEF(BASS_DTS_SetPosition)(void* inst, QWORD position, DWORD mode);

VOID BASSDTSDEF(BASS_DTS_Free)(void* inst);

#endif