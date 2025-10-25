#pragma once

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cctype>
#include <atomic>
#include <array>

#include "pico/stdlib.h"
#include "pico/stdio.h"
#include "pico/time.h"

constexpr size_t TERMINAL_BUFFER_SIZE = 256;

enum GLOBAL_VAR : uint8_t {
    ACTIVE = 0,
    HX711_DISABLE_PRINTING=1,
    HX711_PRINT_CURRENT_WEIGHT=2,
    HX711_PRINT_OFFSET_VALUES=3,
    _COUNT
};

// System state structure
namespace SystemState {
    static inline std::array<std::atomic<uint32_t>, GLOBAL_VAR::_COUNT> globals_{};
}

namespace CommandLineInterface {

    namespace internal {
        bool read_terminal_line(char* buffer, size_t buffer_size) {
            static size_t buffer_pos = 0;
            static char internal_buffer[TERMINAL_BUFFER_SIZE];
            
            int c = getchar_timeout_us(0);
            
            while (c != PICO_ERROR_TIMEOUT) {
                if (c == '\r' || c == '\n') {
                    internal_buffer[buffer_pos] = '\0';
                    
                    size_t copy_len = buffer_pos < buffer_size - 1 ? 
                                      buffer_pos : buffer_size - 1;
                    
                    memcpy(buffer, internal_buffer, copy_len);
                    buffer[copy_len] = '\0';
                    
                    buffer_pos = 0;
                    
                    c = getchar_timeout_us(1000);
                    if (c != '\n' && c != PICO_ERROR_TIMEOUT) {
                        internal_buffer[buffer_pos++] = c;
                    }
                    
                    return true;
                }
                
                if (buffer_pos < TERMINAL_BUFFER_SIZE - 1) {
                    internal_buffer[buffer_pos++] = c;
                    putchar(c);
                }
                
                c = getchar_timeout_us(0);
            }
            
            if(buffer_pos > 0) {
                putchar('\n');
            }

            return false;
        }

        inline bool word_matches(const char* str, size_t word_index, const char* expected) {
            if (!str || !expected) return false;
            
            const char* current = str;
            size_t current_index = 0;
            while (*current == ' ') current++;
            
            while (*current) {
                const char* word_start = current;
                
                while (*current && *current != ' ') current++;
                if (current_index == word_index) {
                    size_t word_len = current - word_start;
                    size_t expected_len = strlen(expected);
                    if (word_len == expected_len && 
                        strncmp(word_start, expected, word_len) == 0) {
                        return true;
                    }
                    return false;
                }
                
                while (*current == ' ') current++;
                current_index++;
            }
            
            return false;
        }
    }

    void handle_hx711_printing(const char* buffer) {
        if(internal::word_matches(buffer, 2, "on") || internal::word_matches(buffer, 2, "1") || internal::word_matches(buffer, 2, "enable")) {
            printf("HX711: Output printing enabled.\n");
            SystemState::globals_[GLOBAL_VAR::HX711_DISABLE_PRINTING] = 0;
        } else if(internal::word_matches(buffer, 2, "off") || internal::word_matches(buffer, 2, "0") || internal::word_matches(buffer, 2, "disable")) {
            printf("HX711: Output printing disabled.\n");
            SystemState::globals_[GLOBAL_VAR::HX711_DISABLE_PRINTING] = 1;
        } else if(internal::word_matches(buffer, 2, "toggle") || internal::word_matches(buffer, 2, "x")) {
            uint8_t v = SystemState::globals_[GLOBAL_VAR::HX711_DISABLE_PRINTING].load();
            uint8_t n = 1 - v;
            printf("HX711: Output printing was %s now %s.\n", v ? "on" : "off", n ? "on" : "off");
            SystemState::globals_[GLOBAL_VAR::HX711_DISABLE_PRINTING] = n;
        } else {
                printf("Invalid hx711 print command format.\n\t To enable: on, 1, enable\n\t To disable: off, 0, disable\n\t To toggle: toggle, x\n");
        }
    }

    void handle_hx711_display(const char* buffer) {
        if(internal::word_matches(buffer, 2, "weight") || internal::word_matches(buffer, 2, "w")) {
            SystemState::globals_[GLOBAL_VAR::HX711_PRINT_CURRENT_WEIGHT] = 1;
        } else if(internal::word_matches(buffer, 2, "offset") || internal::word_matches(buffer, 2, "o") || internal::word_matches(buffer, 2, "off")) {
            SystemState::globals_[GLOBAL_VAR::HX711_PRINT_OFFSET_VALUES] = 1;
        }
    }

    void handle_hx711_command(const char* buffer) {   
        if(buffer == nullptr) {
            printf("Invalid hx711 command format. Use: hx711 [print]\n");
            return;
        } else if(internal::word_matches(buffer, 1, "print")) {
            handle_hx711_printing(buffer);
        } else if(internal::word_matches(buffer, 1, "show")) {
            handle_hx711_display(buffer);
        } else if(internal::word_matches(buffer, 1, "calib")) {
            handle_hx711_display(buffer);
        } else {
            printf("Unknown command.\n");
        }
    }

    void process_cli_commands(){
        static char buffer[TERMINAL_BUFFER_SIZE];
        size_t buffer_pos = 0;
        if(internal::read_terminal_line(buffer, TERMINAL_BUFFER_SIZE)){
            if (internal::word_matches(buffer, 0, "status")) {
                printf("System Status: Running for %lu\n", to_ms_since_boot(get_absolute_time()));
            } else if (internal::word_matches(buffer, 0, "hx711")) {
                handle_hx711_command(buffer);
            } else {
                printf("Invalid set command format.\n");
            }
        }
    }
}
