#pragma once

#include <stddef.h>
#include <stdint.h>
__attribute__((optimize("Ofast")))
static inline char crc7(uint8_t const *data, int const length) {
extern const char m_Crc7Table[];    
	char crc = 0;
	for (int i = 0; i < length; i++) {
		crc = m_Crc7Table[(crc << 1) ^ data[i]];
	}

	return crc;
}

uint16_t crc16(uint8_t const *data, int const length);
