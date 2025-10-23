#pragma once

#include "pico/unique_id.h"
#include <cstdio>
#include <cstring>

namespace device_id {

/**
 * @brief Get 8-bit device ID derived from unique board ID
 * 
 * Uses XOR of all 8 bytes of the board ID for better distribution.
 * This provides reasonable uniqueness for small networks (<50 devices).
 * 
 * @return uint8_t Device ID (0-255)
 * 
 * Note: With 8-bit IDs, collision probability:
 *   - 23 devices: 50% chance of collision
 *   - 50 devices: 97% chance of collision
 *   - For larger networks, consider using full board ID in protocol
 */
inline uint8_t get_device_id() {
    pico_unique_board_id_t board_id;
    pico_get_unique_board_id(&board_id);
    
    // XOR all 8 bytes together for better distribution
    uint8_t id = 0;
    for (int i = 0; i < PICO_UNIQUE_BOARD_ID_SIZE_BYTES; i++) {
        id ^= board_id.id[i];
    }
    
    // Ensure ID is never 0 (reserved for broadcast/invalid)
    if (id == 0) {
        id = 1;
    }
    
    return id;
}

/**
 * @brief Get unique board ID as hex string
 * 
 * Returns the full 64-bit board ID as a hex string for display/logging.
 * Format: "AABBCCDDEEFF0011" (16 hex characters)
 * 
 * @param buffer Output buffer (must be at least 17 bytes)
 * @param buffer_size Size of output buffer
 */
inline void get_board_id_string(char* buffer, size_t buffer_size) {
    if (buffer_size < (PICO_UNIQUE_BOARD_ID_SIZE_BYTES * 2 + 1)) {
        buffer[0] = '\0';
        return;
    }
    
    pico_unique_board_id_t board_id;
    pico_get_unique_board_id(&board_id);
    
    for (int i = 0; i < PICO_UNIQUE_BOARD_ID_SIZE_BYTES; i++) {
        snprintf(buffer + (i * 2), 3, "%02X", board_id.id[i]);
    }
}

/**
 * @brief Get last 4 bytes of board ID as 32-bit value
 * 
 * Useful for more unique identification than 8-bit while remaining compact.
 * 
 * @return uint32_t Last 4 bytes of board ID
 */
inline uint32_t get_device_id_32() {
    pico_unique_board_id_t board_id;
    pico_get_unique_board_id(&board_id);
    
    uint32_t id = 0;
    id |= ((uint32_t)board_id.id[4] << 24);
    id |= ((uint32_t)board_id.id[5] << 16);
    id |= ((uint32_t)board_id.id[6] << 8);
    id |= ((uint32_t)board_id.id[7]);
    
    return id;
}

/**
 * @brief Print device identification information
 * 
 * Displays full board ID and derived device ID for debugging.
 */
inline void print_device_info() {
    char board_id_str[PICO_UNIQUE_BOARD_ID_SIZE_BYTES * 2 + 1];
    get_board_id_string(board_id_str, sizeof(board_id_str));
    
    uint8_t device_id_8 = get_device_id();
    uint32_t device_id_32 = get_device_id_32();
    
    printf("Device Identification:\n");
    printf("  Board ID (64-bit):  %s\n", board_id_str);
    printf("  Device ID (8-bit):  %u (0x%02X)\n", device_id_8, device_id_8);
    printf("  Device ID (32-bit): %u (0x%08X)\n", device_id_32, device_id_32);
}

/**
 * @brief Compare if this device matches a given board ID string
 * 
 * Useful for conditional behavior based on specific devices.
 * 
 * @param target_board_id Hex string of target board ID (case-insensitive)
 * @return true if this device matches the target
 */
inline bool is_device(const char* target_board_id) {
    char my_board_id[PICO_UNIQUE_BOARD_ID_SIZE_BYTES * 2 + 1];
    get_board_id_string(my_board_id, sizeof(my_board_id));
    
    for (size_t i = 0; i < strlen(target_board_id) && i < sizeof(my_board_id); i++) {
        char c1 = my_board_id[i];
        char c2 = target_board_id[i];
        
        if (c1 >= 'a' && c1 <= 'z') c1 -= 32;  // to uppercase
        if (c2 >= 'a' && c2 <= 'z') c2 -= 32;
        
        if (c1 != c2) return false;
    }
    
    return true;
}

} // namespace device_id