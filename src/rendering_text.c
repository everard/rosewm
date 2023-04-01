// Copyright Nezametdinov E. Ildus 2023.
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
// Font managing utility functions and types.
////////////////////////////////////////////////////////////////////////////////

struct rose_font {
    struct rose_memory memory;
    FT_Face ft_face;
};

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
    if(FT_New_Memory_Face( //
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

    size_t n_fonts;
    struct rose_font fonts[];
};

////////////////////////////////////////////////////////////////////////////////
// Bounding box computation utility functions.
////////////////////////////////////////////////////////////////////////////////

static FT_BBox
rose_compute_bbox(FT_GlyphSlot glyph) {
    if(glyph == NULL) {
        return (FT_BBox){};
    }

    return (FT_BBox){.xMin = glyph->bitmap_left,
                     .yMin = glyph->bitmap_top - glyph->bitmap.rows,
                     .xMax = glyph->bitmap_left + glyph->bitmap.width,
                     .yMax = glyph->bitmap_top};
}

static FT_BBox
rose_stretch_bbox(FT_BBox a, FT_BBox b, FT_Pos offset_x) {
    return (FT_BBox){.xMin = min_(a.xMin, b.xMin + offset_x),
                     .yMin = min_(a.yMin, b.yMin),
                     .xMax = max_(a.xMax, b.xMax + offset_x),
                     .yMax = max_(a.yMax, b.yMax)};
}

////////////////////////////////////////////////////////////////////////////////
// Glyph rendering utility function.
////////////////////////////////////////////////////////////////////////////////

static FT_GlyphSlot
rose_render_glyph(struct rose_text_rendering_context* context, char32_t c) {
    // Find a font face which contains the given character's code point.
    FT_Face ft_face = context->fonts[0].ft_face;

    for(size_t i = 0; i < context->n_fonts; ++i) {
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
    FT_Glyph glyphs[rose_utf32_string_size_max];
    size_t n_glyphs;
};

struct rose_string_metrics {
    FT_BBox bbox;
    FT_Pos y_ref_min, y_ref_max;
};

static struct rose_string_metrics
rose_render_string_glyphs( //
    struct rose_text_rendering_context* context,
    struct rose_text_rendering_parameters params,
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
    for(size_t i = 0; i < context->n_fonts; ++i) {
        FT_Set_Char_Size( //
            context->fonts[i].ft_face, 0, params.font_size * 64, params.dpi,
            params.dpi);
    }

    // Compute reference space for the string.
    if(true) {
        FT_GlyphSlot glyph = rose_render_glyph(context, 0x4D);
        if(glyph != NULL) {
            result.y_ref_min = glyph->bitmap_top - glyph->bitmap.rows;
            result.y_ref_max = glyph->bitmap_top;
        }
    }

    // Initialize bounding box for the string.
    result.bbox =
        (FT_BBox){.xMin = 65535, .yMin = 65535, .xMax = -65535, .yMax = -65535};

    // Initialize rendering history (used for backtracking).
    struct {
        FT_Pos pen_x;
        FT_BBox bbox;
    } history[rose_utf32_string_size_max] = {};

    // Compute horizontal bound.
    FT_Pos w_max = ((params.w_max <= 0) ? INT_MAX : params.w_max);

    // Initialize ellipsis character data.
    struct {
        FT_Glyph glyph;
        FT_BBox bbox;
        FT_Pos advance_x;
    } ellipsis = {};

    if(glyph_buffer != NULL) {
        FT_GlyphSlot glyph = rose_render_glyph(context, 0x2026);
        if(glyph != NULL) {
            if(FT_Get_Glyph(glyph, &(ellipsis.glyph)) == FT_Err_Ok) {
                ellipsis.bbox = rose_compute_bbox(glyph);
                ellipsis.advance_x = (glyph->advance.x / 64);
            }
        }
    }

    // Initialize pen position.
    FT_Pos pen_x = 0;

    // Render string characters.
    for(size_t i = 0, j = 0; i < string.size; ++i) {
        // Render current character.
        FT_GlyphSlot glyph = rose_render_glyph(context, string.data[i]);
        if(glyph == NULL) {
            continue;
        }

        // Stretch string's bounding box.
        result.bbox =
            rose_stretch_bbox(result.bbox, rose_compute_bbox(glyph), pen_x);

        // If there is no glyph buffer specified, then advance pen position and
        // move to the next character.
        if(glyph_buffer == NULL) {
            pen_x += (glyph->advance.x / 64);
            continue;
        }

        // Check string's width.
        if((result.bbox.xMax - result.bbox.xMin) > w_max) {
            // If it exceeds horizontal bound, then start backtracking and find
            // a position which can fit the rendered ellipsis character.
            for(j = 0; glyph_buffer->n_glyphs > 0;) {
                // Destroy the last glyph.
                FT_Done_Glyph(
                    glyph_buffer->glyphs[j = --(glyph_buffer->n_glyphs)]);

                // Restore pen position.
                pen_x = history[j].pen_x;

                // Compute string's bounding box.
                result.bbox =
                    rose_stretch_bbox(history[j].bbox, ellipsis.bbox, pen_x);

                // If the string fits, then break out of the cycle.
                if((result.bbox.xMax - result.bbox.xMin) <= w_max) {
                    break;
                }
            }

            // If such position is at the beginning, then restore pen's initial
            // position and set string's bounding box to ellipsis character's
            // bounding box.
            if(j == 0) {
                pen_x = history[0].pen_x;
                result.bbox = ellipsis.bbox;
            }

            // Add rendered ellipsis glyph to the glyph buffer.
            if(ellipsis.glyph != NULL) {
                if(FT_Glyph_Copy(ellipsis.glyph, &(glyph_buffer->glyphs[j])) ==
                   FT_Err_Ok) {
                    // Compute glyph's offset.
                    ((FT_BitmapGlyph)(glyph_buffer->glyphs[j]))->left += pen_x;

                    // Increment the number of glyphs in the glyph buffer.
                    glyph_buffer->n_glyphs = ++j;
                }
            }

            // And break out of the cycle.
            break;
        }

        // Add rendered glyph to the glyph buffer.
        if(FT_Get_Glyph(glyph, &(glyph_buffer->glyphs[j])) == FT_Err_Ok) {
            // Compute glyph's offset.
            ((FT_BitmapGlyph)(glyph_buffer->glyphs[j]))->left += pen_x;

            // Save current pen position and string's bounding box.
            history[j].pen_x = pen_x;
            history[j].bbox = result.bbox;

            // Increment the number of glyphs in the glyph buffer.
            glyph_buffer->n_glyphs = ++j;
        }

        // Advance pen position.
        pen_x += (glyph->advance.x / 64);
    }

    // Free memory.
    if(ellipsis.glyph != NULL) {
        FT_Done_Glyph(ellipsis.glyph);
    }

    return result;
}

////////////////////////////////////////////////////////////////////////////////
// Text rendering context initialization/destruction interface implementation.
////////////////////////////////////////////////////////////////////////////////

struct rose_text_rendering_context*
rose_text_rendering_context_initialize(
    struct rose_text_rendering_context_parameters params) {
#define free_font_data_                           \
    for(size_t i = 0; i != params.n_fonts; ++i) { \
        rose_free(&(params.fonts[i]));            \
    }

    // Check the parameters.
    if((params.n_fonts == 0) || (params.n_fonts > 8)) {
        free_font_data_;
        return NULL;
    }

    // Allocate and initialize a new text rendering context.
    struct rose_text_rendering_context* context =
        malloc(sizeof(struct rose_text_rendering_context) +
               sizeof(struct rose_font) * params.n_fonts);

    if(context != NULL) {
        *context =
            (struct rose_text_rendering_context){.n_fonts = params.n_fonts};

        for(size_t i = 0; i != params.n_fonts; ++i) {
            context->fonts[i] = (struct rose_font){.memory = params.fonts[i]};
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
    for(size_t i = 0; i != context->n_fonts; ++i) {
        context->fonts[i] = rose_font_initialize(context->ft, params.fonts[i]);
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
        for(size_t i = 0; i != context->n_fonts; ++i) {
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

struct rose_text_rendering_extents
rose_compute_string_extents( //
    struct rose_text_rendering_context* context,
    struct rose_text_rendering_parameters params,
    struct rose_utf32_string string) {
    struct rose_text_rendering_extents extents = {};

    // Render glyphs from the string and compute string's bounding box.
    FT_BBox string_bbox =
        rose_render_string_glyphs(context, params, string, NULL).bbox;

    // Validate string's bounding box.
    if((string_bbox.xMax < string_bbox.xMin) ||
       (string_bbox.yMax < string_bbox.yMin)) {
        return extents;
    }

    // Compute string's extents.
    extents.w = string_bbox.xMax - string_bbox.xMin;
    extents.h = string_bbox.yMax - string_bbox.yMin;

    return extents;
}

struct rose_text_rendering_extents
rose_render_string( //
    struct rose_text_rendering_context* context,
    struct rose_text_rendering_parameters params,
    struct rose_utf32_string string, //
    struct rose_pixel_buffer pixel_buffer) {
    struct rose_text_rendering_extents extents = {};

    // Obtain color.
    unsigned color[] = {
        params.color.rgba8[2], params.color.rgba8[1], params.color.rgba8[0]};

    // Compute horizontal bound.
    params.w_max = ((params.w_max > 0) ? min_(pixel_buffer.w, params.w_max)
                                       : pixel_buffer.w);

    // Compute pixel buffer's pitch, if needed.
    if(pixel_buffer.pitch <= 0) {
        pixel_buffer.pitch = pixel_buffer.w * 4;
    }

    // Initialize an empty glyph buffer.
    struct rose_glyph_buffer glyph_buffer = {};

    // Render glyphs and compute string's bounding box.
    struct rose_string_metrics string_metrics =
        rose_render_string_glyphs(context, params, string, &glyph_buffer);

    // Validate string's bounding box.
    if((string_metrics.bbox.xMax < string_metrics.bbox.xMin) ||
       (string_metrics.bbox.yMax < string_metrics.bbox.yMin)) {
        goto cleanup;
    }

    // Render string to the pixel buffer.
    if(true) {
        // Compute baseline's offset.
        FT_Pos baseline_dx = -string_metrics.bbox.xMin;
        FT_Pos baseline_dy = -string_metrics.y_ref_min;

        // Center the baseline.
        if(true) {
            // Compute string's reference height.
            FT_Pos h = (string_metrics.y_ref_max - string_metrics.y_ref_min);

            // Update the baseline.
            if(pixel_buffer.h > h) {
                if(string_metrics.y_ref_min < 0) {
                    h -= string_metrics.y_ref_min;
                }

                baseline_dy += (pixel_buffer.h - h) / 2;
            }
        }

        // Copy each rendered character to the pixel buffer.
        for(size_t i = 0; i < glyph_buffer.n_glyphs; ++i) {
            // Obtain current glyph.
            FT_BitmapGlyph glyph = (FT_BitmapGlyph)(glyph_buffer.glyphs[i]);

            // Compute offsets.
            FT_Pos dst_dx = glyph->left + baseline_dx;
            FT_Pos dst_dy = pixel_buffer.h - glyph->top - baseline_dy;

            FT_Pos src_dy = ((dst_dy < 0) ? -dst_dy : 0);
            dst_dy = ((dst_dy < 0) ? 0 : dst_dy);

            // Compute target width based on glyph bitmap's width and the amount
            // of space left in the pixel buffer.
            FT_Pos w = glyph->bitmap.width;

            if(true) {
                FT_Pos w_available = pixel_buffer.w - dst_dx;
                w = min_(w, w_available);
            }

            // Compute target height based on glyph bitmap's height and the
            // amount of space left in the pixel buffer.
            FT_Pos h = glyph->bitmap.rows - src_dy;

            if(true) {
                FT_Pos h_available = pixel_buffer.h - dst_dy;
                h = min_(h, h_available);
            }

            // Compute glyph bitmap's pitch.
            FT_Pos bitmap_pitch =
                ((glyph->bitmap.pitch < 0) ? -(glyph->bitmap.pitch)
                                           : +(glyph->bitmap.pitch));

            // Render glyph's bitmap to the pixel buffer.
            if((w > 0) && (h > 0)) {
                unsigned char* line_src =
                    glyph->bitmap.buffer + (ptrdiff_t)(bitmap_pitch * src_dy);

                for(FT_Pos j = dst_dy; j < (dst_dy + h);
                    ++j, line_src += bitmap_pitch) {
                    unsigned char* src = line_src;
                    unsigned char* dst =
                        pixel_buffer.data + pixel_buffer.pitch * j + 4 * dst_dx;

                    for(int k = 0; k != w; ++k) {
                        unsigned char c = *(src++);
                        *(dst++) = (unsigned char)((color[0] * c) / 255);
                        *(dst++) = (unsigned char)((color[1] * c) / 255);
                        *(dst++) = (unsigned char)((color[2] * c) / 255);
                        *(dst++) = c;
                    }
                }
            }
        }
    }

    // Compute string's extents.
    extents.w = string_metrics.bbox.xMax - string_metrics.bbox.xMin;
    extents.h = string_metrics.bbox.yMax - string_metrics.bbox.yMin;

cleanup:
    // Free memory and return the result.
    for(size_t i = 0; i < glyph_buffer.n_glyphs; ++i) {
        FT_Done_Glyph(glyph_buffer.glyphs[i]);
    }

    return extents;
}
