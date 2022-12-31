// Copyright Nezametdinov E. Ildus 2022.
// Distributed under the GNU General Public License, Version 3.
// (See accompanying file LICENSE_GPL_3_0.txt or copy at
// https://www.gnu.org/licenses/gpl-3.0.txt)
//
#include "rendering_text.h"

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H

#include <sys/stat.h>
#include <sys/types.h>

#include <limits.h>
#include <unistd.h>
#include <string.h>

#include <stdbool.h>
#include <stdio.h>

////////////////////////////////////////////////////////////////////////////////
// Helper macros.
////////////////////////////////////////////////////////////////////////////////

#define min_(a, b) ((a) < (b) ? (a) : (b))
#define max_(a, b) ((a) > (b) ? (a) : (b))

////////////////////////////////////////////////////////////////////////////////
// Font-handling-related utility functions and types.
////////////////////////////////////////////////////////////////////////////////

struct rose_font {
    FT_Face ft_face;
    unsigned char* data;
    size_t size;
};

static struct rose_font*
rose_font_initialize(FT_Library ft, char const* file_name) {
    // Open the file.
    FILE* file = fopen(file_name, "rb");
    if(file == NULL) {
        return NULL;
    }

    // Determine file's size.
    struct stat file_stat = {};
    fstat(fileno(file), &file_stat);

    // Allocate and initialize a new font object.
    struct rose_font* font =
        malloc(sizeof(struct rose_font) + (size_t)(file_stat.st_size));

    if(font == NULL) {
        goto exit;
    } else {
        *font = (struct rose_font){
            .data = (unsigned char*)(font) + sizeof(struct rose_font),
            .size = (size_t)(file_stat.st_size)};
    }

    // Read file's contents to the buffer.
    fread(font->data, font->size, 1, file);

    // Create a new font face.
    if(FT_New_Memory_Face(ft, font->data, font->size, 0, &(font->ft_face)) !=
       FT_Err_Ok) {
        goto error;
    }

    // Require scalable font.
    if(!FT_IS_SCALABLE(font->ft_face)) {
        goto error;
    }

    goto exit;

error:
    // On error, destroy FreeType font face.
    if(font->ft_face != NULL) {
        FT_Done_Face(font->ft_face);
    }

    // And free memory.
    font = (free(font), NULL);

exit:
    // Close the file.
    fclose(file);

    // Return created font, if any.
    return font;
}

static void
rose_font_destroy(struct rose_font* font) {
    if(font == NULL) {
        return;
    }

    // Destroy FreeType font face.
    if(font->ft_face != NULL) {
        FT_Done_Face(font->ft_face);
    }

    // Free memory.
    free(font);
}

////////////////////////////////////////////////////////////////////////////////
// Text rendering context definition.
////////////////////////////////////////////////////////////////////////////////

struct rose_text_rendering_context {
    FT_Library ft;

    size_t n_fonts;
    struct rose_font* fonts[];
};

////////////////////////////////////////////////////////////////////////////////
// Glyph-rendering-related utility functions and types.
////////////////////////////////////////////////////////////////////////////////

struct rose_glyph_buffer {
    FT_Glyph glyphs[rose_utf32_string_size_max];
    size_t size;
};

struct rose_string_metrics {
    FT_BBox bbox;
    FT_Pos y_ref_min, y_ref_max;
};

static FT_GlyphSlot
rose_render_glyph(struct rose_text_rendering_context* context, char32_t c) {
    // Find a font face which contains the given character's code point.
    FT_Face ft_face = context->fonts[0]->ft_face;

    for(size_t i = 0; i < context->n_fonts; ++i) {
        if(FT_Get_Char_Index(context->fonts[i]->ft_face, c) != 0) {
            ft_face = context->fonts[i]->ft_face;
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

static struct rose_string_metrics
rose_render_glyphs( //
    struct rose_text_rendering_context* context,
    struct rose_text_rendering_parameters params,
    struct rose_utf32_string string, FT_Pos pen_x,
    struct rose_glyph_buffer* glyph_buffer) {
    // Do nothing if the given string is empty.
    if(string.size == 0) {
        return (struct rose_string_metrics){};
    }

    // Limit string's size.
    string.size = min_(string.size, rose_utf32_string_size_max);

    // Set font's size.
    for(size_t i = 0; i < context->n_fonts; ++i) {
        FT_Set_Char_Size(context->fonts[i]->ft_face, 0, params.font_size * 64,
                         params.dpi, params.dpi);
    }

    // Compute string's reference space.
    FT_Pos y_ref_min = 0, y_ref_max = 0;
    if(true) {
        // Render a glyph for 'M' character.
        FT_GlyphSlot glyph = rose_render_glyph(context, 0x4D);

        // Compute the space.
        if(glyph != NULL) {
            y_ref_min = glyph->bitmap_top - (FT_Pos)(glyph->bitmap.rows);
            y_ref_max = glyph->bitmap_top;
        }
    }

    // Initialize string's bounding box.
    FT_BBox string_bbox = {
        .xMin = 65535, .yMin = 65535, .xMax = -65535, .yMax = -65535};

    // Define buffers for backtracking.
    FT_Pos saved_pen_x[rose_utf32_string_size_max];
    FT_BBox saved_string_bbox[rose_utf32_string_size_max];

    // Compute horizontal bound.
    FT_Pos w_max = ((params.w_max == 0) ? INT_MAX : params.w_max);

    // Render a glyph for the ellipsis character, if needed.
    struct {
        FT_Glyph glyph;
        FT_BBox bbox;

        FT_Pos advance_x;
        FT_Pos w;
    } ellipsis = {};

    if(glyph_buffer != NULL) {
        // Render the glyph.
        FT_GlyphSlot glyph = rose_render_glyph(context, 0x2026);

        // Save the rendered glyph, if any.
        if(glyph != NULL) {
            if(FT_Get_Glyph(glyph, &(ellipsis.glyph)) == FT_Err_Ok) {
                ellipsis.bbox = (FT_BBox){
                    .xMin = glyph->bitmap_left,
                    .yMin = glyph->bitmap_top - (FT_Pos)(glyph->bitmap.rows),
                    .xMax = glyph->bitmap_left + (FT_Pos)(glyph->bitmap.width),
                    .yMax = glyph->bitmap_top};

                ellipsis.advance_x = (glyph->advance.x / 64);
                ellipsis.w = ellipsis.bbox.xMax - ellipsis.bbox.xMin;
            }
        }
    }

    // Render string's characters.
    for(size_t i = 0, j = 0; i < string.size; ++i) {
        // Render current character.
        FT_GlyphSlot glyph = rose_render_glyph(context, string.data[i]);

        if(glyph == NULL) {
            continue;
        }

        // Compute rendered glyph's bounding box.
        FT_BBox glyph_bbox = {
            .xMin = glyph->bitmap_left,
            .yMin = glyph->bitmap_top - (FT_Pos)(glyph->bitmap.rows),
            .xMax = glyph->bitmap_left + (FT_Pos)(glyph->bitmap.width),
            .yMax = glyph->bitmap_top};

        // Stretch string's bounding box: bottom-left corner.
        string_bbox.xMin = min_(string_bbox.xMin, glyph_bbox.xMin + pen_x);
        string_bbox.yMin = min_(string_bbox.yMin, glyph_bbox.yMin);

        // Stretch string's bounding box: top-right corner.
        string_bbox.xMax = max_(string_bbox.xMax, glyph_bbox.xMax + pen_x);
        string_bbox.yMax = max_(string_bbox.yMax, glyph_bbox.yMax);

        // If there is a glyph buffer specified, then perform additional
        // actions.
        if(glyph_buffer != NULL) {
            // If there is a rendered ellipsis glyph, and the string exceeds
            // specified horizontal bound, then use the rendered ellipsis to
            // signal that the string isn't fully visible.
            if(ellipsis.glyph != NULL) {
                // Compute string's width.
                FT_Pos string_w = string_bbox.xMax - string_bbox.xMin;

                // Obtain bounding box of the rendered ellipsis.
                glyph_bbox = ellipsis.bbox;

                // If string's width exceeds horizontal bound, then start
                // backtracking and insert ellipsis at the end.
                if(string_w > w_max) {
                    // Find a position which can fit the rendered ellipsis.
                    for(j = 0; glyph_buffer->size > 0;) {
                        FT_Done_Glyph(
                            glyph_buffer->glyphs[j = --glyph_buffer->size]);

                        // Restore pen's position and string's bounding box.
                        pen_x = saved_pen_x[j];
                        string_bbox = saved_string_bbox[j];

                        // Stretch string's bounding box: bottom-left corner.
                        string_bbox.xMin =
                            min_(string_bbox.xMin, glyph_bbox.xMin + pen_x);
                        string_bbox.yMin =
                            min_(string_bbox.yMin, glyph_bbox.yMin);

                        // Stretch string's bounding box: top-right corner.
                        string_bbox.xMax =
                            max_(string_bbox.xMax, glyph_bbox.xMax + pen_x);
                        string_bbox.yMax =
                            max_(string_bbox.yMax, glyph_bbox.yMax);

                        // If the string fits, then break out of the cycle.
                        if((string_bbox.xMax - string_bbox.xMin) <= w_max) {
                            break;
                        }
                    }

                    // If such position is at the beginning of the glyph buffer,
                    // then restore pen's initial position and set string's
                    // bounding box to ellipsis's bounding box.
                    if(j == 0) {
                        pen_x = saved_pen_x[0];
                        string_bbox = glyph_bbox;
                    }

                    // Add rendered ellipsis to the glyph buffer.
                    if(FT_Glyph_Copy(ellipsis.glyph,
                                     &(glyph_buffer->glyphs[j])) == FT_Err_Ok) {
                        // Compute glyph's offset.
                        ((FT_BitmapGlyph)(glyph_buffer->glyphs[j]))->left +=
                            (int)(pen_x);

                        // Increment the number of glyphs in the glyph buffer.
                        glyph_buffer->size = ++j;
                    }

                    // Break out of the cycle.
                    break;
                }
            }

            // Otherwise, add rendered glyph to the glyph buffer.
            if(FT_Get_Glyph(glyph, &(glyph_buffer->glyphs[j])) == FT_Err_Ok) {
                // Compute glyph's offset.
                ((FT_BitmapGlyph)(glyph_buffer->glyphs[j]))->left +=
                    (int)(pen_x);

                // Save pen's position and string's bounding box.
                saved_pen_x[j] = pen_x;
                saved_string_bbox[j] = string_bbox;

                // Increment the number of glyphs in the glyph buffer.
                glyph_buffer->size = ++j;
            }
        }

        // Advance pen's position.
        pen_x += (glyph->advance.x / 64);
    }

    // Free memory.
    if(ellipsis.glyph != NULL) {
        FT_Done_Glyph(ellipsis.glyph);
    }

    // Return string's metrics.
    return (struct rose_string_metrics){
        .bbox = string_bbox, .y_ref_min = y_ref_min, .y_ref_max = y_ref_max};
}

////////////////////////////////////////////////////////////////////////////////
// Text rendering context initialization/destruction interface implementation.
////////////////////////////////////////////////////////////////////////////////

struct rose_text_rendering_context*
rose_text_rendering_context_initialize(
    struct rose_text_rendering_context_parameters params) {
    // Limit the number of fonts.
    params.n_fonts = ((params.n_fonts > 8) ? 8 : params.n_fonts);

    // Number of fonts must not equal to zero.
    if(params.n_fonts == 0) {
        return NULL;
    }

    // Allocate and initialize a new text rendering context.
    struct rose_text_rendering_context* context =
        malloc(sizeof(struct rose_text_rendering_context) +
               sizeof(struct rose_font*) * params.n_fonts);

    if(context != NULL) {
        *context =
            (struct rose_text_rendering_context){.n_fonts = params.n_fonts};

        for(size_t i = 0; i < context->n_fonts; ++i) {
            context->fonts[i] = NULL;
        }
    } else {
        goto error;
    }

    // Initialize FreeType.
    if(FT_Init_FreeType(&(context->ft)) != FT_Err_Ok) {
        goto error;
    }

    // Load fonts.
    for(size_t i = 0; i < context->n_fonts; ++i) {
        context->fonts[i] =
            rose_font_initialize(context->ft, params.font_names[i]);

        if(context->fonts[i] == NULL) {
            goto error;
        }
    }

    // Initialization succeeded.
    return context;

error:
    // On error, destroy the context.
    return rose_text_rendering_context_destroy(context), NULL;
}

void
rose_text_rendering_context_destroy(
    struct rose_text_rendering_context* context) {
    if(context == NULL) {
        return;
    }

    // Destroy fonts.
    for(size_t i = 0; i < context->n_fonts; ++i) {
        rose_font_destroy(context->fonts[i]);
    }

    // Destroy FreeType context.
    if(context->ft != NULL) {
        FT_Done_FreeType(context->ft);
    }

    // Free memory.
    free(context);
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

    // Render glyphs and compute string's bounding box.
    FT_BBox string_bbox =
        rose_render_glyphs(context, params, string, 0, NULL).bbox;

    // Validate string's bounding box.
    if((string_bbox.xMax < string_bbox.xMin) ||
       (string_bbox.yMax < string_bbox.yMin)) {
        return extents;
    }

    // Compute string's extents.
    extents.w = string_bbox.xMax - string_bbox.xMin;
    extents.h = string_bbox.yMax - string_bbox.yMin;

    // Return string's extents.
    return extents;
}

struct rose_text_rendering_extents
rose_render_string( //
    struct rose_text_rendering_context* context,
    struct rose_text_rendering_parameters params,
    struct rose_utf32_string string, //
    struct rose_pixel_buffer pixel_buffer) {
    struct rose_text_rendering_extents extents = {};

    // Convert color value.
    unsigned color_value[] = //
        {(unsigned)(params.color.v[2] * 255.0 + 0.5),
         (unsigned)(params.color.v[1] * 255.0 + 0.5),
         (unsigned)(params.color.v[0] * 255.0 + 0.5)};

    // Compute horizontal bound.
    params.w_max = ((params.w_max != 0) ? min_(pixel_buffer.w, params.w_max)
                                        : pixel_buffer.w);

    // Render glyphs and compute string's bounding box.
    struct rose_glyph_buffer glyph_buffer = {};

    struct rose_string_metrics string_metrics =
        rose_render_glyphs(context, params, string, 0, &glyph_buffer);

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
            FT_Pos string_h =
                (string_metrics.y_ref_max - string_metrics.y_ref_min);

            // Update the baseline.
            if(pixel_buffer.h > string_h) {
                if(string_metrics.y_ref_min < 0) {
                    string_h -= string_metrics.y_ref_min;
                }

                baseline_dy += (pixel_buffer.h - string_h) / 2;
            }
        }

        // Copy each rendered character to the pixel buffer.
        for(size_t i = 0; i < glyph_buffer.size; ++i) {
            // Obtain current glyph.
            FT_BitmapGlyph glyph = (FT_BitmapGlyph)(glyph_buffer.glyphs[i]);

            // Compute offsets.
            FT_Pos dst_dx = glyph->left + baseline_dx;
            FT_Pos dst_dy = pixel_buffer.h - glyph->top - baseline_dy;

            FT_Pos src_dy = ((dst_dy < 0) ? -dst_dy : 0);
            dst_dy = ((dst_dy < 0) ? 0 : dst_dy);

            // Compute target width based on glyph bitmap's width and the amount
            // of space left in pixel buffer.
            FT_Pos w = (FT_Pos)(glyph->bitmap.width);

            if(true) {
                FT_Pos w_available = pixel_buffer.w - dst_dx;
                w = min_(w, w_available);
            }

            // Compute target height based on glyph bitmap's height and the
            // amount of space left in pixel buffer.
            FT_Pos h = (FT_Pos)(glyph->bitmap.rows) - src_dy;

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
                        pixel_buffer.data + 4 * (pixel_buffer.w * j + dst_dx);

                    for(int k = 0; k != w; ++k) {
                        unsigned char c = *(src++);
                        *(dst++) = (unsigned char)((color_value[0] * c) / 255);
                        *(dst++) = (unsigned char)((color_value[1] * c) / 255);
                        *(dst++) = (unsigned char)((color_value[2] * c) / 255);
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
    // Free memory.
    for(size_t i = 0; i < glyph_buffer.size; ++i) {
        FT_Done_Glyph(glyph_buffer.glyphs[i]);
    }

    // Return string's extents.
    return extents;
}
