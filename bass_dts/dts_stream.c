#include "dts_stream.h"
#include "dts_file.h"
#include "pcm.h"

BOOL dts_stream_create(const BASSFILE file, const int flags, DTS_STREAM** const stream) {
	*stream = calloc(sizeof(DTS_STREAM), 1);
	if (!*stream) {
		//Allocation failed.
		return FALSE;
	}

	if (!dts_file_create(file, &(*stream)->dts_file))
	{
		//Stream creation failed.
		dts_stream_free(*stream);
		return FALSE;
	}

	if (!((*stream)->dcadec_context = dcadec_context_create(flags))) {
		//Context creation failed.
		dts_stream_free(*stream);
		return FALSE;
	}

	if (!dts_stream_update(*stream)) {
		//Could not determine enough information to create a stream.
		dts_stream_free(*stream);
		return FALSE;
	}

	if (!dts_stream_update_info(*stream)) {
		//Could not determine enough information to create a stream.
		dts_stream_free(*stream);
		return FALSE;
	}

	return TRUE;
}

BOOL dts_stream_update(DTS_STREAM* const stream) {
	//Attempt to read a new frame (including extended info) from the file and convert it to PCM.
	int channel_mask;
	int sample_rate;
	int bits_per_sample;
	int profile;
	int result;

	//Attempt to read a new frame (including extended info) from the file.
	if (!dts_file_read(stream->dts_file)) {
		return FALSE;
	}

	//Attempt to parse the frame.
	if ((result = dcadec_context_parse(stream->dcadec_context, stream->dts_file->frame.buffer, stream->dts_file->frame.size)) < 0) {
		return FALSE;
	}

	//Attempt to convert the frame to PCM.
	if ((result = dcadec_context_filter(stream->dcadec_context, &stream->samples, &stream->sample_count, &channel_mask, &sample_rate, &bits_per_sample, &profile)) < 0) {
		return FALSE;
	}

	//Update some information.
	stream->input_format.bits_per_sample = bits_per_sample;
	stream->input_format.bytes_per_sample = bits_per_sample / 8;
	stream->input_format.samples_per_frame = stream->sample_count;

	return TRUE;
}

BOOL dts_stream_update_info(DTS_STREAM* const stream) {
	//Attempt to fetch the core and extended frame information.
	struct dcadec_core_info* dcadec_core_info = dcadec_context_get_core_info(stream->dcadec_context);
	struct dcadec_exss_info* dcadec_exss_info = dcadec_context_get_exss_info(stream->dcadec_context);

	//If there's no core info something is wrong.
	if (!dcadec_core_info) {
		return FALSE;
	}

	stream->channel_count = dcadec_core_info->nchannels;
	stream->sample_rate = dcadec_core_info->sample_rate;

	if (dcadec_exss_info) {
		//If we have extended information we usually have higher channel count or sample rate.
		stream->channel_count = dcadec_exss_info->nchannels;
		stream->sample_rate = dcadec_exss_info->sample_rate;
		//Make a note of this.
		stream->dts_file->info.has_extensions = TRUE;
		dcadec_context_free_exss_info(dcadec_exss_info);
	}
	else {
		stream->dts_file->info.has_extensions = FALSE;
	}

	dcadec_context_free_core_info(dcadec_core_info);
	return TRUE;
}

DWORD dts_stream_read(DTS_STREAM* const stream, void* buffer, const DWORD length) {
	//Read previously parsed PCM data into the buffer.

	DWORD position = 0;
	DWORD remaining = length;
	DWORD size = stream->output_format.bytes_per_sample * stream->channel_count;
	int channel;

	while (TRUE) {
		if (stream->sample_position == stream->sample_count) {
			//All data has been read, reset stream state.
			dts_stream_reset(stream);
			break;
		}
		if (size > remaining) {
			//Not enough space to write any more samples.
			break;
		}
		for (channel = 0; channel < stream->channel_count; channel++) {
			stream->write_sample(buffer, position++, stream->samples[channel][stream->sample_position]);
		}
		stream->sample_position++;
		remaining -= size;
	}

	return length - remaining;
}

BOOL dts_stream_reset(DTS_STREAM* const stream) {
	stream->samples = NULL;
	stream->sample_count = 0;
	stream->sample_position = 0;
	return TRUE;
}

BOOL dts_stream_free(DTS_STREAM* const stream) {
	dts_file_free(stream->dts_file);
	if (stream->dcadec_context) {
		dcadec_context_destroy(stream->dcadec_context);
	}
	free(stream);
	return TRUE;
}