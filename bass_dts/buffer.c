#include "buffer.h"

void* offset_buffer(const void* const buffer, const DWORD position) {
	//Just offsets a pointer by the specificed number of bytes.
	return (BYTE*)buffer + position;
}