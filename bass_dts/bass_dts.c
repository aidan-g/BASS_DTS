#include "bass_dts.h"
#include "dts_file.h"
#include "dts_stream.h"
#include "pcm.h"
#include "buffer.h"

#define BASS_CTYPE_MUSIC_DTS 0x1f200

extern const ADDON_FUNCTIONS addon_functions;

//We just privode the minimum functions, the NULL slots are optional.
const ADDON_FUNCTIONS addon_functions = {
	0,
	&BASS_DTS_Free,
	&BASS_DTS_GetLength,
	NULL,
	NULL,
	&BASS_DTS_GetInfo,
	&BASS_DTS_CanSetPosition,
	&BASS_DTS_SetPosition,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL
};

static const BASS_PLUGINFORM plugin_form[] = { BASS_CTYPE_MUSIC_DTS, "DTS", "*.dts" };

static const BASS_PLUGININFO plugin_info = { BASSVERSION, 1, plugin_form };

BOOL BASSDTSDEF(DllMain)(HANDLE dll, DWORD reason, LPVOID reserved) {
	switch (reason) {
	case DLL_PROCESS_ATTACH:
		DisableThreadLibraryCalls((HMODULE)dll);
		if (HIWORD(BASS_GetVersion()) != BASSVERSION || !GetBassFunc()) {
			MessageBoxA(0, "Incorrect BASS.DLL version (" BASSVERSIONTEXT " is required)", "BASS", MB_ICONERROR | MB_OK);
			return FALSE;
		}
		break;
	}
	return TRUE;
}

VOID* BASSDTSDEF(BASSplugin)(DWORD face) {
	switch (face) {
	case BASSPLUGIN_INFO:
		return (void*)&plugin_info;
	case BASSPLUGIN_CREATE:
		return (void*)&BASS_DTS_StreamCreate;
	case BASSPLUGIN_CREATEURL:
		return (void*)&BASS_DTS_StreamCreateURL;
	}
	return NULL;
}

HSTREAM BASSDTSDEF(BASS_DTS_StreamCreate)(BASSFILE file, DWORD flags) {
	HSTREAM handle;
	DTS_STREAM* dts_stream;
	if (!dts_stream_create(file, 0, &dts_stream)) {
		return 0;
	}
	if (flags & BASS_SAMPLE_FLOAT) {
		dts_stream->output_format.bits_per_sample = sizeof(float) * 8;
		dts_stream->output_format.bytes_per_sample = sizeof(float);
	}
	else {
		dts_stream->output_format.bits_per_sample = sizeof(short) * 8;
		dts_stream->output_format.bytes_per_sample = sizeof(short);
	}

	if (!(dts_stream->write_sample = pcm_write_sample(dts_stream->input_format, dts_stream->output_format))) {
		//Sample conversion is not implemented :c
		dts_stream_free(dts_stream);
		errorn(BASS_ERROR_NOTAVAIL);
		return 0;
	}

	handle = bassfunc->CreateStream(
		dts_stream->sample_rate,
		dts_stream->channel_count,
		flags,
		&BASS_DTS_StreamProc,
		dts_stream,
		&addon_functions
	);
	if (handle == 0) {
		dts_stream_free(dts_stream);
		return 0;
	}

	return handle;
}

HSTREAM BASSDTSDEF(BASS_DTS_StreamCreateFile)(BOOL mem, const void* file, QWORD offset, QWORD length, DWORD flags) {
	HSTREAM handle;
	BASSFILE bass_file = bassfunc->file.Open(mem, file, offset, length, flags, FALSE);
	if (!bass_file) {
		return 0;
	}
	handle = BASS_DTS_StreamCreate(bass_file, flags);
	if (!handle) {
		bassfunc->file.Close(bass_file);
		return 0;

	}
	bassfunc->file.SetStream(bass_file, handle);
	return handle;
}

HSTREAM BASSDTSDEF(BASS_DTS_StreamCreateURL)(const char *url, DWORD offset, DWORD flags, DOWNLOADPROC *proc, void *user) {
	//Sorry we only support file backed streams.
	errorn(BASS_ERROR_NOTAVAIL);
	return 0;
}

DWORD BASSDTSDEF(BASS_DTS_StreamProc)(HSTREAM handle, void* buffer, DWORD length, void* user) {
	DTS_STREAM* dts_stream = user;
	DWORD position = 0;
	DWORD remaining = length;
	while (remaining > 0) {
		//Make sure samples are available.
		if (!dts_stream->samples || !dts_stream->sample_count) {
			if (!dts_stream_update(dts_stream)) {
				//Reached the end of the file (or some catastrophic failure to synchronize).
				return BASS_STREAMPROC_END;
			}
		}
		if (!BASS_DTS_StreamWrite(handle, buffer, &position, &remaining, user)) {
			break;
		}
	}
	//If remaining > 0 it's a buffer underrun, ASIO won't be happy. 
	//Nothing we can do.
	return length - remaining;
}

BOOL BASSDTSDEF(BASS_DTS_StreamWrite)(HSTREAM handle, void* buffer, DWORD* position, DWORD* remaining, void *user) {
	DTS_STREAM* dts_stream = user;
	//length = the amount of data from the current decodeded samples available.
	DWORD length =
		(dts_stream->sample_count - dts_stream->sample_position) *
		dts_stream->output_format.bytes_per_sample *
		dts_stream->channel_count;

	//we usually have more data than can be written.
	if (length > *remaining) {
		length = *remaining;
	}

	if (!(length = dts_stream_read(dts_stream, offset_buffer(buffer, *position), length))) {
		//Failed to read *any* data. Probably something wrong.
		return FALSE;
	}

	*position += length;
	*remaining -= length;
	return TRUE;
}

QWORD BASSDTSDEF(BASS_DTS_GetLength)(void *inst, DWORD mode) {
	DTS_STREAM* dts_stream = inst;
	QWORD position;
	if (mode == BASS_POS_BYTE) {
		//This is *almost* correct. The frame_count is not quite right which throws this off.
		position =
			dts_stream->dts_file->info.frame_count *
			dts_stream->input_format.samples_per_frame *
			dts_stream->output_format.bytes_per_sample *
			dts_stream->channel_count;
		return position;
	}
	else {
		errorn(BASS_ERROR_NOTAVAIL);
		return 0;
	}
}

VOID BASSDTSDEF(BASS_DTS_GetInfo)(void *inst, BASS_CHANNELINFO* info) {
	DTS_STREAM* dts_stream = inst;
	info->ctype = BASS_CTYPE_MUSIC_DTS;
	info->freq = dts_stream->sample_rate;
	info->chans = dts_stream->channel_count;
	info->origres = dts_stream->input_format.bits_per_sample;
}

BOOL BASSDTSDEF(BASS_DTS_CanSetPosition)(void *inst, QWORD position, DWORD mode) {
	if (mode == BASS_POS_BYTE) {
		//Because we're always file backed the position should always be valid.
		return TRUE;
	}
	else {
		errorn(BASS_ERROR_NOTAVAIL);
		return 0;
	}
}

QWORD BASSDTSDEF(BASS_DTS_SetPosition)(void *inst, QWORD position, DWORD mode) {
	DTS_STREAM* dts_stream = inst;
	if (mode == BASS_POS_BYTE) {
		//Not sure why we divide by the number of channels but nothing else.
		QWORD offset = position / dts_stream->channel_count;
		//I have no fucking clue why BASS sometimes sends a position that is twice the size it should be.
		//No fucking clue.
		if (offset > dts_stream->dts_file->info.length) {
			offset /= 2;
		}
		if (dts_file_seek(dts_stream->dts_file, offset, DTS_FILE_SEEK_POSITION)) {
			//If the seek succeeded then throw away any already decoded samples.
			if (dts_stream_reset(dts_stream)) {
				return position;
			}
		}
	}
	errorn(BASS_ERROR_NOTAVAIL);
	return 0;
}

VOID BASSDTSDEF(BASS_DTS_Free)(void *inst) {
	DTS_STREAM* dts_stream = inst;
	dts_stream_free(dts_stream);
}