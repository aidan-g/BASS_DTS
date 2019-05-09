#include "bass_dts.h"

typedef enum {
	DTS_FILE_SEEK_BEGIN,
	DTS_FILE_SEEK_POSITION,
	DTS_FILE_SEEK_END
} DTS_FILE_SEEK;

BOOL dts_file_create(const BASSFILE bass_file, DTS_FILE** const dts_file);

BOOL dts_file_read(DTS_FILE* const dts_file);

BOOL dts_file_seek(DTS_FILE* const dts_file, const QWORD position, const DTS_FILE_SEEK mode);

QWORD dts_file_position(const DTS_FILE* const dts_file);

QWORD dts_file_length(const DTS_FILE* const dts_file);

BOOL dts_file_free(DTS_FILE* const dts_file);