// Copyright Nezametdinov E. Ildus 2024.
// Distributed under the GNU General Public License, Version 3.
// (See accompanying file LICENSE_GPL_3_0.txt or copy at
// https://www.gnu.org/licenses/gpl-3.0.txt)
//
#include "memory.h"
#include "rendering_text.h"

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H

#include <stdbool.h>
#include <stddef.h>

////////////////////////////////////////////////////////////////////////////////
// Helper macros.
////////////////////////////////////////////////////////////////////////////////

#define min_(a, b) ((a) < (b) ? (a) : (b))
#define max_(a, b) ((a) > (b) ? (a) : (b))

////////////////////////////////////////////////////////////////////////////////
// Font definition.
////////////////////////////////////////////////////////////////////////////////

struct rose_font {
    struct rose_memory memory;
    FT_Face ft_face;
};

////////////////////////////////////////////////////////////////////////////////
// Font managing utility functions.
////////////////////////////////////////////////////////////////////////////////

static void
rose_font_destroy(struct rose_font* font) {
    if(font != NULL) {
        if(font->ft_face != NULL) {
            font->ft_face = (FT_Done_Face(font->ft_face), NULL);
        }

        rose_free(&(font->memory));
    }
}

static struct rose_font
rose_font_initialize(FT_Library ft, struct rose_memory memory) {
    // Initialize font object.
    struct rose_font font = {.memory = memory};
    if(font.memory.data == NULL) {
        return font;
    }

    // Create a new font face.
    if(FT_New_Memory_Face(
           ft, font.memory.data, font.memory.size, 0, &(font.ft_face)) !=
       FT_Err_Ok) {
        rose_font_destroy(&font);
    }

    // Require scalable font.
    if((font.ft_face == NULL) || !FT_IS_SCALABLE(font.ft_face)) {
        rose_font_destroy(&font);
    }

    return font;
}

////////////////////////////////////////////////////////////////////////////////
// Text rendering context definition.
////////////////////////////////////////////////////////////////////////////////

struct rose_text_rendering_context {
    FT_Library ft;

    size_t font_count;
    struct rose_font fonts[];
};

////////////////////////////////////////////////////////////////////////////////
// Bounding box manipulating utility functions.
////////////////////////////////////////////////////////////////////////////////

static FT_BBox
rose_compute_bounding_box(FT_GlyphSlot glyph) {
    if(glyph == NULL) {
        return (FT_BBox){};
    }

    return (FT_BBox){
        .xMin = glyph->bitmap_left,
        .yMin = glyph->bitmap_top - glyph->bitmap.rows,
        .xMax = glyph->bitmap_left + glyph->bitmap.width,
        .yMax = glyph->bitmap_top};
}

static FT_BBox
rose_stretch_bounding_box(FT_BBox a, FT_BBox b, FT_Pos offset_x) {
    return (FT_BBox){
        .xMin = min_(a.xMin, b.xMin + offset_x),
        .yMin = min_(a.yMin, b.yMin),
        .xMax = max_(a.xMax, b.xMax + offset_x),
        .yMax = max_(a.yMax, b.yMax)};
}

static struct rose_text_rendering_extent
rose_compute_bounding_box_extent(FT_BBox bounding_box) {
    return (struct rose_text_rendering_extent){
        .width = bounding_box.xMax - bounding_box.xMin,
        .height = bounding_box.yMax - bounding_box.yMin};
}

////////////////////////////////////////////////////////////////////////////////
// Glyph rendering utility function.
////////////////////////////////////////////////////////////////////////////////

static FT_GlyphSlot
rose_render_glyph(struct rose_text_rendering_context* context, char32_t c) {
    // Find a font face which contains the given character's code point.
    FT_Face ft_face = context->fonts[0].ft_face;

    for(size_t i = 0; i < context->font_count; ++i) {
        if(FT_Get_Char_Index(context->fonts[i].ft_face, c) != 0) {
            ft_face = context->fonts[i].ft_face;
            break;
        }
    }

    // Render a glyph for the given character.
    if(FT_Load_Char(ft_face, c, FT_LOAD_RENDER) != FT_Err_Ok) {
        return NULL;
    }

    // Validate rendered glyph's format.
    if(ft_face->glyph->format != FT_GLYPH_FORMAT_BITMAP) {
        return NULL;
    }

    // Return the glyph slot with rendered glyph.
    return ft_face->glyph;
}

////////////////////////////////////////////////////////////////////////////////
// String rendering utility function and types.
////////////////////////////////////////////////////////////////////////////////

struct rose_glyph_buffer {
    FT_Glyph data[rose_utf32_string_size_max];
    size_t size;
};

struct rose_string_metrics {
    FT_BBox bounding_box;
    FT_Pos y_min, y_max;
};

static struct rose_string_metrics
rose_render_string_glyphs(
    struct rose_text_rendering_context* context,
    struct rose_text_rendering_parameters parameters,
    struct rose_utf32_string string, //
    struct rose_glyph_buffer* glyph_buffer) {
    // Initialize an empty result.
    struct rose_string_metrics result = {};

    // Do nothing if the given string is empty.
    if(string.size == 0) {
        return result;
    }

    // Limit string size.
    string.size = min_(string.size, rose_utf32_string_size_max);

    // Set font size.
    for(size_t i = 0; i < context->font_count; ++i) {
        FT_Set_Char_Size(
            context->fonts[i].ft_face, 0, parameters.font_size * 64,
            parameters.dpi, parameters.dpi);
    }

    // Compute reference space for the string.
    if(true) {
        FT_GlyphSlot glyph = rose_render_glyph(context, 0x4D);
        if(glyph != NULL) {
            result.y_min = glyph->bitmap_top - glyph->bitmap.rows;
            result.y_max = glyph->bitmap_top;
        }
    }

    // Initialize bounding box for the string.
    result.bounding_box =
        (FT_BBox){.xMin = 65535, .yMin = 65535, .xMax = -65535, .yMax = -65535};

    // Initialize rendering history (used for backtracking).
    struct {
        FT_Pos pen_position;
        FT_BBox bounding_box;
    } history[rose_utf32_string_size_max] = {};

    // Compute horizontal bound.
    FT_Pos max_width =
        ((parameters.max_width <= 0) ? INT_MAX : parameters.max_width);

    // Initialize ellipsis character data.
    struct {
        FT_Glyph glyph;

        FT_Pos advance_x;
        FT_BBox bounding_box;
    } ellipsis_character = {};

    if(glyph_buffer != NULL) {
        FT_GlyphSlot glyph = rose_render_glyph(context, 0x2026);
        if(glyph != NULL) {
            if(FT_Get_Glyph(glyph, &(ellipsis_character.glyph)) == FT_Err_Ok) {
                ellipsis_character.advance_x = (glyph->advance.x / 64);
                ellipsis_character.bounding_box =
                    rose_compute_bounding_box(glyph);
            }
        }
    }

    // Initialize pen position.
    FT_Pos pen_position = 0;

    // Render string characters.
    for(size_t i = 0, j = 0; i < string.size; ++i) {
        // Render current character.
        FT_GlyphSlot glyph = rose_render_glyph(context, string.data[i]);
        if(glyph == NULL) {
            continue;
        }

        // Stretch string's bounding box.
        result.bounding_box = rose_stretch_bounding_box(
            result.bounding_box, rose_compute_bounding_box(glyph),
            pen_position);

        // If there is no glyph buffer specified, then advance pen position and
        // move to the next character.
        if(glyph_buffer == NULL) {
            pen_position += (glyph->advance.x / 64);
            continue;
        }

        // Check string's width.
        if((result.bounding_box.xMax - result.bounding_box.xMin) > max_width) {
            // If it exceeds horizontal bound, then start backtracking and find
            // a position which can fit the rendered ellipsis character.
            for(j = 0; glyph_buffer->size > 0;) {
                // Destroy the last glyph.
                FT_Done_Glyph(glyph_buffer->data[j = --(glyph_buffer->size)]);

                // Restore pen position.
                pen_position = history[j].pen_position;

                // Compute string's bounding box.
                result.bounding_box = rose_stretch_bounding_box(
                    history[j].bounding_box, ellipsis_character.bounding_box,
                    pen_position);

                // If the string fits, then break out of the cycle.
                if((result.bounding_box.xMax - result.bounding_box.xMin) <=
                   max_width) {
                    break;
                }
            }

            // If such position is at the beginning, then restore pen's initial
            // position and set string's bounding box to ellipsis character's
            // bounding box.
            if(j == 0) {
                pen_position = history[0].pen_position;
                result.bounding_box = ellipsis_character.bounding_box;
            }

            // Add rendered ellipsis glyph to the glyph buffer.
            if(ellipsis_character.glyph != NULL) {
                if(FT_Glyph_Copy(
                       ellipsis_character.glyph, &(glyph_buffer->data[j])) ==
                   FT_Err_Ok) {
                    // Compute glyph's offset.
                    ((FT_BitmapGlyph)(glyph_buffer->data[j]))->left +=
                        pen_position;

                    // Increment the number of glyphs in the glyph buffer.
                    glyph_buffer->size = ++j;
                }
            }

            // And break out of the cycle.
            break;
        }

        // Add rendered glyph to the glyph buffer.
        if(FT_Get_Glyph(glyph, &(glyph_buffer->data[j])) == FT_Err_Ok) {
            // Compute glyph's offset.
            ((FT_BitmapGlyph)(glyph_buffer->data[j]))->left += pen_position;

            // Save current pen position and string's bounding box.
            history[j].pen_position = pen_position;
            history[j].bounding_box = result.bounding_box;

            // Increment the number of glyphs in the glyph buffer.
            glyph_buffer->size = ++j;
        }

        // Advance pen position.
        pen_position += (glyph->advance.x / 64);
    }

    // Free memory.
    if(ellipsis_character.glyph != NULL) {
        FT_Done_Glyph(ellipsis_character.glyph);
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////
// Text rendering context initialization/destruction interface implementation.
////////////////////////////////////////////////////////////////////////////////

struct rose_text_rendering_context*
rose_text_rendering_context_initialize(
    struct rose_text_rendering_context_parameters parameters) {
#define free_font_data_                                  \
    for(size_t i = 0; i != parameters.font_count; ++i) { \
        rose_free(&(parameters.fonts[i]));               \
    }

    // Check the parameters.
    if((parameters.font_count == 0) || (parameters.font_count > 8)) {
        free_font_data_;
        return NULL;
    }

    // Allocate and initialize a new text rendering context.
    struct rose_text_rendering_context* context = malloc(
        sizeof(struct rose_text_rendering_context) +
        sizeof(struct rose_font) * parameters.font_count);

    if(context != NULL) {
        *context = (struct rose_text_rendering_context){
            .font_count = parameters.font_count};

        for(size_t i = 0; i != parameters.font_count; ++i) {
            context->fonts[i] =
                (struct rose_font){.memory = parameters.fonts[i]};
        }
    } else {
        free_font_data_;
        return NULL;
    }

#undef free_font_data_

    // Initialize FreeType.
    if(FT_Init_FreeType(&(context->ft)) != FT_Err_Ok) {
        return (rose_text_rendering_context_destroy(context), NULL);
    }

    // Initialize fonts.
    for(size_t i = 0; i != context->font_count; ++i) {
        context->fonts[i] =
            rose_font_initialize(context->ft, parameters.fonts[i]);

        if(context->fonts[i].ft_face == NULL) {
            return (rose_text_rendering_context_destroy(context), NULL);
        }
    }

    return context;
}

void
rose_text_rendering_context_destroy(
    struct rose_text_rendering_context* context) {
    if(context != NULL) {
        // Destroy fonts.
        for(size_t i = 0; i != context->font_count; ++i) {
            rose_font_destroy(&(context->fonts[i]));
        }

        // Destroy FreeType context.
        if(context->ft != NULL) {
            FT_Done_FreeType(context->ft);
        }

        // Free memory.
        free(context);
    }
}

////////////////////////////////////////////////////////////////////////////////
// Text rendering interface implementation.
////////////////////////////////////////////////////////////////////////////////

struct rose_text_rendering_extent
rose_compute_string_extent(
    struct rose_text_rendering_context* context,
    struct rose_text_rendering_parameters parameters,
    struct rose_utf32_string string) {
    // Render glyphs from the string and obtain string's bounding box.
    FT_BBox bounding_box =
        rose_render_string_glyphs(context, parameters, string, NULL)
            .bounding_box;

    // Validate string's bounding box.
    if((bounding_box.xMax < bounding_box.xMin) ||
       (bounding_box.yMax < bounding_box.yMin)) {
        return (struct rose_text_rendering_extent){};
    }

    return rose_compute_bounding_box_extent(bounding_box);
}

struct rose_text_rendering_extent
rose_render_string(
    struct rose_text_rendering_context* context,
    struct rose_text_rendering_parameters parameters,
    struct rose_utf32_string string, //
    struct rose_pixel_buffer pixel_buffer) {
    struct rose_text_rendering_extent result = {};

    // Obtain color.
    unsigned color[] = {
        parameters.color.rgba8[2], parameters.color.rgba8[1],
        parameters.color.rgba8[0]};

    // Compute horizontal bound.
    parameters.max_width =
        ((parameters.max_width > 0)
             ? min_(pixel_buffer.width, parameters.max_width)
             : pixel_buffer.width);

    // Compute pixel buffer's pitch, if needed.
    if(pixel_buffer.pitch <= 0) {
        pixel_buffer.pitch = pixel_buffer.width * 4;
    }

    // Initialize an empty glyph buffer.
    struct rose_glyph_buffer glyph_buffer = {};

    // Render glyphs and compute string's bounding box.
    struct rose_string_metrics string_metrics =
        rose_render_string_glyphs(context, parameters, string, &glyph_buffer);

    // Validate string's bounding box.
    if((string_metrics.bounding_box.xMax < string_metrics.bounding_box.xMin) ||
       (string_metrics.bounding_box.yMax < string_metrics.bounding_box.yMin)) {
        goto cleanup;
    }

    // Render string to the pixel buffer.
    if(true) {
        // Compute baseline's offset.
        FT_Pos dx_baseline = -string_metrics.bounding_box.xMin;
        FT_Pos dy_baseline = -string_metrics.y_min;

        // Center the baseline.
        if(true) {
            // Compute string's reference height.
            FT_Pos height = (string_metrics.y_max - string_metrics.y_min);

            // Update the baseline.
            if(pixel_buffer.height > height) {
                if(string_metrics.y_min < 0) {
                    height -= string_metrics.y_min;
                }

                dy_baseline += (pixel_buffer.height - height) / 2;
            }
        }

        // Copy each rendered character to the pixel buffer.
        for(size_t i = 0; i < glyph_buffer.size; ++i) {
            // Obtain current glyph.
            FT_BitmapGlyph glyph = (FT_BitmapGlyph)(glyph_buffer.data[i]);

            // Compute offsets.
            FT_Pos dx_target = glyph->left + dx_baseline;
            FT_Pos dy_target = pixel_buffer.height - glyph->top - dy_baseline;

            FT_Pos dy_source = ((dy_target < 0) ? -dy_target : 0);
            dy_target = max_(0, dy_target);

            // Compute target width based on glyph bitmap's width and the amount
            // of space left in the pixel buffer.
            FT_Pos width = glyph->bitmap.width;
            if(true) {
                FT_Pos space = pixel_buffer.width - dx_target;
                width = min_(width, space);
            }

            // Compute target height based on glyph bitmap's height and the
            // amount of space left in the pixel buffer.
            FT_Pos height = glyph->bitmap.rows - dy_source;
            if(true) {
                FT_Pos space = pixel_buffer.height - dy_target;
                height = min_(height, space);
            }

            // Compute glyph bitmap's pitch.
            FT_Pos bitmap_pitch =
                ((glyph->bitmap.pitch < 0) ? -(glyph->bitmap.pitch)
                                           : +(glyph->bitmap.pitch));

            // Render glyph's bitmap to the pixel buffer.
            if((width > 0) && (height > 0)) {
                unsigned char* bitmap_buffer =
                    glyph->bitmap.buffer + bitmap_pitch * dy_source;

                for(FT_Pos j = dy_target; j < (dy_target + height);
                    ++j, bitmap_buffer += bitmap_pitch) {
                    unsigned char* source = bitmap_buffer;
                    unsigned char* target = pixel_buffer.data +
                                            pixel_buffer.pitch * j +
                                            4 * dx_target;

                    for(int k = 0; k != width; ++k) {
                        unsigned char c = *(source++);
                        *(target++) = (unsigned char)((color[0] * c) / 255);
                        *(target++) = (unsigned char)((color[1] * c) / 255);
                        *(target++) = (unsigned char)((color[2] * c) / 255);
                        *(target++) = c;
                    }
                }
            }
        }
    }

    // Compute string's extent.
    result = rose_compute_bounding_box_extent(string_metrics.bounding_box);

cleanup:
    // Free memory and return the result.
    for(size_t i = 0; i < glyph_buffer.size; ++i) {
        FT_Done_Glyph(glyph_buffer.data[i]);
    }

    return result;
}
