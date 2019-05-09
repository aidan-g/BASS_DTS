#include <stdio.h>

#include "dts_file.h"
#include "../libdcadec/common.h"
#include "../libdcadec/ta.h"
#include "../libdcadec/dca_frame.h"

#define BUFFER_ALIGN 4096

#define DTSHDHDR UINT64_C(0x4454534844484452)

static BOOL dts_file_core_sync_word(const uint32_t value) {
	//Is the value a valid core sync word.
	return
		//DTS Core
		value == SYNC_WORD_CORE ||
		value == SYNC_WORD_CORE_LE ||
		value == SYNC_WORD_CORE_LE14 ||
		value == SYNC_WORD_CORE_BE14;
}

static BOOL dts_file_ext_sync_word(const uint32_t value) {
	//Is the value a valid extension sync word.
	return
		//XCH 
		value == SYNC_WORD_XCH ||
		//XXCH 
		value == SYNC_WORD_XXCH ||
		//X96K 
		value == SYNC_WORD_X96 ||
		//EXSS 
		value == SYNC_WORD_EXSS ||
		value == SYNC_WORD_EXSS_LE;
}

static BOOL dts_file_sync_word(const uint32_t value) {
	//Is the value a valid core or extension sync word.
	return dts_file_core_sync_word(value) || dts_file_ext_sync_word(value);
}

static BOOL dts_file_read_byte(const DTS_FILE* const dts_file, BYTE* const data) {
	//Read a single byte from the file, return whether it was read successfully.
	return bassfunc->file.Read(dts_file->bass_file, data, 1);
}

static BOOL dts_file_read_required(const DTS_FILE* const dts_file, void* buffer, const DWORD length) {
	//Read data from the file, return whether the request could be fully served.
	return bassfunc->file.Read(dts_file->bass_file, buffer, length) == length;
}

BOOL dts_file_create(const BASSFILE bass_file, DTS_FILE** const dts_file) {
	*dts_file = ta_znew(NULL, DTS_FILE);
	if (!*dts_file) {
		//Allocation failed.
		return FALSE;
	}

	(*dts_file)->bass_file = bass_file;
	(*dts_file)->info.length = bassfunc->file.GetPos(bass_file, BASS_FILEPOS_END);

	if (!((*dts_file)->frame.buffer = ta_zalloc_size(*dts_file, BUFFER_ALIGN * 2))) {
		//Allocation failed.
		dts_file_free(*dts_file);
		return FALSE;
	}

	return TRUE;
}

static BYTE* dts_file_get_buffer(DTS_FILE* const dts_file, const size_t size) {
	//Ensure that the buffer associated with this file is large enough.

	const size_t old_size = ta_get_size(dts_file->frame.buffer);
	const size_t new_size = DCA_ALIGN(dts_file->frame.size + size, BUFFER_ALIGN);

	if (old_size < new_size) {
		//We need to expand.
		BYTE* buffer = ta_realloc_size(dts_file, dts_file->frame.buffer, new_size);
		if (buffer) {
			//Zero the newly allocated memory (for no reason).
			memset(buffer + old_size, 0, new_size - old_size);
			dts_file->frame.buffer = buffer;
		}
		else {
			//Allocation failed.
			return NULL;
		}
	}

	return dts_file->frame.buffer + dts_file->frame.size;
}

static BOOL dts_file_read_sync_word(DTS_FILE* const dts_file, UINT* const sync_word) {
	//Read the next sync word from the file.
	//This function attempts to "append" to the current sync word if one is available.
	//Nothing is done if the current sync word is valid.
	*sync_word = dts_file->frame.sync_word;
	while (!dts_file_sync_word(*sync_word)) {
		//Attempt to read the next byte and rotate it into the current sync word.
		BYTE data;
		if (!dts_file_read_byte(dts_file, &data)) {
			//No more data, probably the end of the file.
			return FALSE;
		}
		*sync_word = (*sync_word << 8) | data;
	}
	return TRUE;
}

static int dts_file_read_frame_header(DTS_FILE* const dts_file, size_t* const size) {
	//Read and parse the next frame header from the file.
	int result;

	//Load the current sync word into the frame header.
	dts_file->frame.header[0] = (dts_file->frame.sync_word >> 24) & 0xff;
	dts_file->frame.header[1] = (dts_file->frame.sync_word >> 16) & 0xff;
	dts_file->frame.header[2] = (dts_file->frame.sync_word >> 8) & 0xff;
	dts_file->frame.header[3] = (dts_file->frame.sync_word >> 0) & 0xff;

	//Load the rest of the frame header from the file.
	if (!dts_file_read_required(dts_file, dts_file->frame.header + 4, DCADEC_FRAME_HEADER_SIZE - 4)) {
		//No more data, probably the end of the file.
		return FALSE;
	}

	//Parse the frame header.
	if ((result = dcadec_frame_parse_header(dts_file->frame.header, size)) < 0) {
		return result;
	}

	if (!dts_file->info.initialized) {
		//If this was the first frame then note the start position.
		//This seems to always be zero.
		dts_file->info.start = dts_file_position(dts_file) - DCADEC_FRAME_HEADER_SIZE;
	}

	return TRUE;
}

static int dts_file_read_frame_data(DTS_FILE* const dts_file, size_t* const size) {
	//Read and convert the next frame data from the file.
	BYTE* buffer;
	int result;

	//Get the current buffer, it is expanded if required.
	if (!(buffer = dts_file_get_buffer(dts_file, *size))) {
		return FALSE;
	}

	//Load the current frame header into the buffer.
	memcpy(buffer, dts_file->frame.header, DCADEC_FRAME_HEADER_SIZE);
	//Read the rest of the frame from the file.
	if (!dts_file_read_required(dts_file, buffer + DCADEC_FRAME_HEADER_SIZE, *size - DCADEC_FRAME_HEADER_SIZE)) {
		//No more data, probably the end of the file.
		return FALSE;
	}

	//Convert the frame to a format that can be parsed.
	if ((result = dcadec_frame_convert_bitstream(buffer, size, buffer, *size)) < 0) {
		//Negative return code means something went wrong.
		return result;
	}

	return TRUE;
}

static int dts_file_read_frame(DTS_FILE* const dts_file, const BOOL ext) {
	//Attempt to read a new frame from the file and convert it to PCM.

	size_t size;
	int result;

	//Read the next sync word from the file.
	if (!dts_file_read_sync_word(dts_file, &dts_file->frame.sync_word)) {
		//No more data, probably the end of the file.
		return FALSE;
	}

	//If we expected extended frame data but the sync code does not indicate this then something went wrong.
	if (ext && !dts_file_ext_sync_word(dts_file->frame.sync_word)) {
		//I've never seen this happen, might indicate file corruption.
		//We should resume at the next readable frame.
		return -DCADEC_ENOSYNC;
	}

	//Read the next frame header from the file.
	if ((result = dts_file_read_frame_header(dts_file, &size)) <= 0) {
		return result;
	}

	//Read and convert the next frame data from the file.
	if ((result = dts_file_read_frame_data(dts_file, &size)) <= 0) {
		return result;
	}

	//Don't know why we have to align this.
	dts_file->frame.size += DCA_ALIGN(size, 4);

	return TRUE;
}

static size_t dts_file_align(const size_t value) {
	//Align a value to closest power of 2 (limited to 32 bits).
	size_t result = value;
	result--;
	result |= result >> 1;
	result |= result >> 2;
	result |= result >> 4;
	result |= result >> 8;
	result |= result >> 16;
	result++;
	return result;
}

BOOL dts_file_read(DTS_FILE* const dts_file) {
	//Attempt to read a new frame (including extended info) from the file.

	int result;

	dts_file->frame.size = 0;

	//Read the next core frame from the file.
	while (TRUE) {
		result = dts_file_read_frame(dts_file, FALSE);
		if (result == TRUE) {
			break;
		}
		else {
			return FALSE;
		}
	}

	//If the sync word is valid then attempt to read an extended frame too.
	if (dts_file_core_sync_word(dts_file->frame.sync_word)) {
		dts_file->frame.sync_word = 0;
		result = dts_file_read_frame(dts_file, TRUE);
		if (result != TRUE && result != -DCADEC_ENOSYNC) {
			//If there was an error not relating to invalid (extended) sync word then something went wrong.
			return FALSE;
		}
	}
	else
	{
		//The sync word is not valid. Don't know how it got here.
		//This should never happen.
		dts_file->frame.sync_word = 0;
	}

	if (!dts_file->info.initialized) {
		//This is not 100% accurate.
		//There appears to be some additional data in the file after the first frame which isn't actual frame data.
		dts_file->info.frame_count =
			(dts_file->info.length - dts_file->info.start) / dts_file_align(dts_file->frame.size);
		dts_file->info.initialized = TRUE;
	}

	return TRUE;
}

static BOOL dts_file_synchronize(DTS_FILE* const dts_file) {
	//Attempt to resynchronize the stream after manual seeking.
retry:
	//Read the next sync word from the file.
	dts_file->frame.sync_word = 0;
	if (!dts_file_read_sync_word(dts_file, &dts_file->frame.sync_word)) {
		//No more data, probably the end of the file.
		return FALSE;
	}
	if (dts_file->info.has_extensions) {
		//If the sync word is extended then we ended up between a core and extended frame.
		//Discard if and fetch the next one.
		if (dts_file_ext_sync_word(dts_file->frame.sync_word)) {
			goto retry;
		}
	}
	//We back off the stream position by the size of a sync word as the stream state is about to be discarded.
	if (!bassfunc->file.Seek(dts_file->bass_file, dts_file_position(dts_file) - sizeof(UINT))) {
		//Failed to seek for some reason.
		return FALSE;
	}
	return TRUE;
}

BOOL dts_file_seek(DTS_FILE* const dts_file, const QWORD position, const DTS_FILE_SEEK mode) {
	QWORD offset = position;
	switch (mode) {
	case DTS_FILE_SEEK_BEGIN:
		//Seek to the beginning of the file.
		offset += bassfunc->file.GetPos(dts_file->bass_file, BASS_FILEPOS_START);
		if (!bassfunc->file.Seek(dts_file->bass_file, offset)) {
			//Failed to seek for some reason.
			return FALSE;
		}
		break;
	case DTS_FILE_SEEK_POSITION:
		//Seek to a specific position in the file.
		if (!bassfunc->file.Seek(dts_file->bass_file, position)) {
			//Failed to seek for some reason.
			return FALSE;
		}
		//After seeking we must synchronize the stream to the start of the next core sync word.
		if (!dts_file_synchronize(dts_file)) {
			//Failed to synchronize for some reason.
			return FALSE;
		}
		break;
	case DTS_FILE_SEEK_END:
		//Seek to the end of the file.
		offset += bassfunc->file.GetPos(dts_file->bass_file, BASS_FILEPOS_END);
		if (!bassfunc->file.Seek(dts_file->bass_file, offset)) {
			//Failed to seek for some reason.
			return FALSE;
		}
		break;
	}
	//Discard the current sync word.
	dts_file->frame.sync_word = 0;
	return TRUE;
}

QWORD dts_file_position(const DTS_FILE* const dts_file) {
	//Get the file position in bytes.
	return bassfunc->file.GetPos(dts_file->bass_file, BASS_FILEPOS_CURRENT);
}

QWORD dts_file_length(const DTS_FILE* const dts_file) {
	//Get the file length in bytes.
	return bassfunc->file.GetPos(dts_file->bass_file, BASS_FILEPOS_END);
}

BOOL dts_file_free(DTS_FILE* const dts_file) {
	ta_free(dts_file);
	return TRUE;
}