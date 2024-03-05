// Copyright Nezametdinov E. Ildus 2024.
// Distributed under the GNU General Public License, Version 3.
// (See accompanying file LICENSE_GPL_3_0.txt or copy at
// https://www.gnu.org/licenses/gpl-3.0.txt)
//
#include "unicode.h"

#include <fribidi/fribidi.h>
#include <stdbool.h>
#include <string.h>

////////////////////////////////////////////////////////////////////////////////
// Unicode-related utility functions and types.
////////////////////////////////////////////////////////////////////////////////

enum {
    rose_utf8_decoding_incomplete = 0xFFFFFFFE,
    rose_utf8_decoding_error = 0xFFFFFFFF
};

struct rose_utf8_decoding_result {
    char32_t x;
    size_t n;
};

static struct rose_utf8_decoding_result
rose_utf8_decode(unsigned char const* string, size_t n) {
    // Empty string is always incomplete.
    if((string == NULL) || (n == 0)) {
        return (struct rose_utf8_decoding_result){
            .x = rose_utf8_decoding_incomplete};
    }

    // Check if the string starts with an ASCII character.
    if(string[0] <= 0x7F) {
        return (struct rose_utf8_decoding_result){.x = string[0], .n = 1};
    }

#define is_in_range_(x, a, b) (((x) >= (a)) && ((x) <= (b)))

    // Check for disallowed first byte values (see the Table 3-7 in the Unicode
    // Standard for details).
    if(!is_in_range_(string[0], 0xC2, 0xF4)) {
        return (struct rose_utf8_decoding_result){
            .x = rose_utf8_decoding_error, .n = 1};
    }

    static const struct entry {
        char32_t high;
        unsigned char ranges[3][2], n;
    } utf8_table[51] = {
        {0x00000080, {{0x80, 0xBF}, {0x80, 0xBF}, {0x80, 0xBF}}, 1},
        {0x000000C0, {{0x80, 0xBF}, {0x80, 0xBF}, {0x80, 0xBF}}, 1},
        {0x00000100, {{0x80, 0xBF}, {0x80, 0xBF}, {0x80, 0xBF}}, 1},
        {0x00000140, {{0x80, 0xBF}, {0x80, 0xBF}, {0x80, 0xBF}}, 1},
        {0x00000180, {{0x80, 0xBF}, {0x80, 0xBF}, {0x80, 0xBF}}, 1},
        {0x000001C0, {{0x80, 0xBF}, {0x80, 0xBF}, {0x80, 0xBF}}, 1},
        {0x00000200, {{0x80, 0xBF}, {0x80, 0xBF}, {0x80, 0xBF}}, 1},
        {0x00000240, {{0x80, 0xBF}, {0x80, 0xBF}, {0x80, 0xBF}}, 1},
        {0x00000280, {{0x80, 0xBF}, {0x80, 0xBF}, {0x80, 0xBF}}, 1},
        {0x000002C0, {{0x80, 0xBF}, {0x80, 0xBF}, {0x80, 0xBF}}, 1},
        {0x00000300, {{0x80, 0xBF}, {0x80, 0xBF}, {0x80, 0xBF}}, 1},
        {0x00000340, {{0x80, 0xBF}, {0x80, 0xBF}, {0x80, 0xBF}}, 1},
        {0x00000380, {{0x80, 0xBF}, {0x80, 0xBF}, {0x80, 0xBF}}, 1},
        {0x000003C0, {{0x80, 0xBF}, {0x80, 0xBF}, {0x80, 0xBF}}, 1},
        {0x00000400, {{0x80, 0xBF}, {0x80, 0xBF}, {0x80, 0xBF}}, 1},
        {0x00000440, {{0x80, 0xBF}, {0x80, 0xBF}, {0x80, 0xBF}}, 1},
        {0x00000480, {{0x80, 0xBF}, {0x80, 0xBF}, {0x80, 0xBF}}, 1},
        {0x000004C0, {{0x80, 0xBF}, {0x80, 0xBF}, {0x80, 0xBF}}, 1},
        {0x00000500, {{0x80, 0xBF}, {0x80, 0xBF}, {0x80, 0xBF}}, 1},
        {0x00000540, {{0x80, 0xBF}, {0x80, 0xBF}, {0x80, 0xBF}}, 1},
        {0x00000580, {{0x80, 0xBF}, {0x80, 0xBF}, {0x80, 0xBF}}, 1},
        {0x000005C0, {{0x80, 0xBF}, {0x80, 0xBF}, {0x80, 0xBF}}, 1},
        {0x00000600, {{0x80, 0xBF}, {0x80, 0xBF}, {0x80, 0xBF}}, 1},
        {0x00000640, {{0x80, 0xBF}, {0x80, 0xBF}, {0x80, 0xBF}}, 1},
        {0x00000680, {{0x80, 0xBF}, {0x80, 0xBF}, {0x80, 0xBF}}, 1},
        {0x000006C0, {{0x80, 0xBF}, {0x80, 0xBF}, {0x80, 0xBF}}, 1},
        {0x00000700, {{0x80, 0xBF}, {0x80, 0xBF}, {0x80, 0xBF}}, 1},
        {0x00000740, {{0x80, 0xBF}, {0x80, 0xBF}, {0x80, 0xBF}}, 1},
        {0x00000780, {{0x80, 0xBF}, {0x80, 0xBF}, {0x80, 0xBF}}, 1},
        {0x000007C0, {{0x80, 0xBF}, {0x80, 0xBF}, {0x80, 0xBF}}, 1},
        {0x00000000, {{0xA0, 0xBF}, {0x80, 0xBF}, {0x80, 0xBF}}, 2},
        {0x00001000, {{0x80, 0xBF}, {0x80, 0xBF}, {0x80, 0xBF}}, 2},
        {0x00002000, {{0x80, 0xBF}, {0x80, 0xBF}, {0x80, 0xBF}}, 2},
        {0x00003000, {{0x80, 0xBF}, {0x80, 0xBF}, {0x80, 0xBF}}, 2},
        {0x00004000, {{0x80, 0xBF}, {0x80, 0xBF}, {0x80, 0xBF}}, 2},
        {0x00005000, {{0x80, 0xBF}, {0x80, 0xBF}, {0x80, 0xBF}}, 2},
        {0x00006000, {{0x80, 0xBF}, {0x80, 0xBF}, {0x80, 0xBF}}, 2},
        {0x00007000, {{0x80, 0xBF}, {0x80, 0xBF}, {0x80, 0xBF}}, 2},
        {0x00008000, {{0x80, 0xBF}, {0x80, 0xBF}, {0x80, 0xBF}}, 2},
        {0x00009000, {{0x80, 0xBF}, {0x80, 0xBF}, {0x80, 0xBF}}, 2},
        {0x0000A000, {{0x80, 0xBF}, {0x80, 0xBF}, {0x80, 0xBF}}, 2},
        {0x0000B000, {{0x80, 0xBF}, {0x80, 0xBF}, {0x80, 0xBF}}, 2},
        {0x0000C000, {{0x80, 0xBF}, {0x80, 0xBF}, {0x80, 0xBF}}, 2},
        {0x0000D000, {{0x80, 0x9F}, {0x80, 0xBF}, {0x80, 0xBF}}, 2},
        {0x0000E000, {{0x80, 0xBF}, {0x80, 0xBF}, {0x80, 0xBF}}, 2},
        {0x0000F000, {{0x80, 0xBF}, {0x80, 0xBF}, {0x80, 0xBF}}, 2},
        {0x00000000, {{0x90, 0xBF}, {0x80, 0xBF}, {0x80, 0xBF}}, 3},
        {0x00040000, {{0x80, 0xBF}, {0x80, 0xBF}, {0x80, 0xBF}}, 3},
        {0x00080000, {{0x80, 0xBF}, {0x80, 0xBF}, {0x80, 0xBF}}, 3},
        {0x000C0000, {{0x80, 0xBF}, {0x80, 0xBF}, {0x80, 0xBF}}, 3},
        {0x00100000, {{0x80, 0x8F}, {0x80, 0xBF}, {0x80, 0xBF}}, 3}};

    // Decode the rest of the UTF-8 sequence.
    if(true) {
        struct entry const r = utf8_table[(*(string++)) - 0xC2];
        char32_t x = r.high, shift = r.n * 6;

#define min_(a, b) ((a) < (b) ? (a) : (b))

        for(size_t i = 0, limit = min_(n - 1, r.n); i < limit; ++i) {
            if(!is_in_range_(string[i], r.ranges[i][0], r.ranges[i][1])) {
                return (struct rose_utf8_decoding_result){
                    .x = rose_utf8_decoding_error, .n = (i + 1)};
            }

#undef min_
#undef is_in_range_

            shift -= 6;
            x |= ((char32_t)(string[i] & 0x3F)) << shift;
        }

        if(shift == 0) {
            return (struct rose_utf8_decoding_result){.x = x, .n = (r.n + 1)};
        }
    }

    return (struct rose_utf8_decoding_result){
        .x = rose_utf8_decoding_incomplete};
}

////////////////////////////////////////////////////////////////////////////////
// String conversion interface implementation.
////////////////////////////////////////////////////////////////////////////////

struct rose_utf8_string
rose_convert_ntbs_to_utf8(char* string) {
    return (struct rose_utf8_string){
        .data = string, .size = ((string == NULL) ? 0 : strlen(string))};
}

struct rose_utf32_string
rose_convert_utf8_to_utf32(struct rose_utf8_string string) {
    struct rose_utf32_string r = {};

    // Decode the given string.
    while((r.size < rose_utf32_string_size_max) && (string.size > 0)) {
        // Decode the next character.
        struct rose_utf8_decoding_result d =
            rose_utf8_decode((unsigned char*)(string.data), string.size);

        string.data += d.n;
        string.size -= d.n;

        // If the code sequence is incomplete, then break out of the cycle.
        if(d.x == rose_utf8_decoding_incomplete) {
            break;
        }

        // Skip the error, if any.
        if(d.x == rose_utf8_decoding_error) {
            continue;
        }

        // Save successfully decoded character.
        r.data[r.size++] = d.x;
    }

    // Add ellipsis to the end of the decoded string, if needed.
    if((string.size != 0) && (r.size == rose_utf32_string_size_max)) {
        r.data[rose_utf32_string_size_max - 1] = 0x2026;
    }

    // Apply Unicode Bidirectional Algorithm.
    if(true) {
        FriBidiChar buffer_input[rose_utf32_string_size_max] = {};
        FriBidiChar buffer_output[rose_utf32_string_size_max] = {};

        FriBidiStrIndex string_size = (FriBidiStrIndex)(r.size);
        FriBidiParType base_dir = (FriBidiParType)(FRIBIDI_TYPE_ON);

        // Copy the UTF-32 string into the input buffer.
        for(size_t i = 0; i < r.size; ++i) {
            buffer_input[i] = (FriBidiChar)(r.data[i]);
        }

        // Run the algorithm.
        fribidi_log2vis(buffer_input, string_size, &base_dir, buffer_output,
                        NULL, NULL, NULL);

        // Copy algorithm's output back to the resulting UTF-32 string.
        for(size_t i = 0; i < r.size; ++i) {
            r.data[i] = (char32_t)(buffer_output[i]);
        }
    }

    // Return decoded string.
    return r;
}
