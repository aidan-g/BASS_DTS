#include "bass_dts.h"

BOOL dts_stream_create(const BASSFILE file, const int flags, DTS_STREAM** const stream);

BOOL dts_stream_update(DTS_STREAM* const stream);

BOOL dts_stream_update_info(DTS_STREAM* const stream);

DWORD dts_stream_read(DTS_STREAM* const stream, void* buffer, const DWORD length);

BOOL dts_stream_reset(DTS_STREAM* const stream);

BOOL dts_stream_free(DTS_STREAM* const stream);