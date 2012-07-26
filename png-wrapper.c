/*  PNG storage routine.
    Copyright (C) 2009  Morten Hustveit <morten.hustveit@gmail.com>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, version 2.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <err.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <png.h>

#include "png-wrapper.h"

int
png_write (const char *file_name, unsigned int width, unsigned int height, unsigned char* data)
{
  FILE* f;
  size_t i;

  png_structp png_ptr;
  png_infop info_ptr;
  png_bytepp rows;
  png_color_16 key;

  png_ptr = png_create_write_struct (PNG_LIBPNG_VER_STRING, 0, 0, 0);

  if (!png_ptr)
    return - 1;

  info_ptr = png_create_info_struct (png_ptr);

  if (!info_ptr)
    {
      png_destroy_write_struct (&png_ptr,  png_infopp_NULL);

      return - 1;
    }

  f = fopen (file_name, "wb");

  if (!f)
    {
      png_destroy_write_struct (&png_ptr, &info_ptr);

      return - 1;
    }

  rows = malloc (height * sizeof (png_bytep));

  for (i = 0; i < height; ++i)
    rows[i] = data + i * width * 3;

  png_init_io (png_ptr, f);

  png_set_IHDR (png_ptr, info_ptr, width, height,
               8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
               PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

  key.red = 0xff;
  key.green = 0x00;
  key.blue = 0xff;

  png_set_tRNS(png_ptr, info_ptr, 0, 1, &key);

  png_set_compression_level (png_ptr, Z_BEST_COMPRESSION);

  png_set_rows (png_ptr, info_ptr, (png_byte**) rows);
  png_write_png (png_ptr, info_ptr, PNG_TRANSFORM_IDENTITY, rows);
  png_write_end (png_ptr, info_ptr);

  png_destroy_write_struct (&png_ptr, &info_ptr);

  fclose (f);

  free (rows);

  return 0;
}

int
png_load (const char* path, void **ret_data, unsigned int *ret_width, unsigned int *ret_height)
{
  FILE* f;
  png_bytep* rows;
  png_structp png;
  png_infop pnginfo;
  png_uint_32 width, height;
  unsigned char* data;
  unsigned int row, col;
  int bit_depth, pixel_format, interlace_type;
  int no_alpha = 0;

  f = fopen(path, "rb");

  if(!f)
    err(EXIT_FAILURE, "Failed to open '%s'", path);

  png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);

  if(png == NULL)
    errx(EXIT_FAILURE, "png_create_read_struct failed");

  pnginfo = png_create_info_struct(png);

  if(pnginfo == NULL)
    errx(EXIT_FAILURE, "png_create_info_struct failed");

  if(setjmp(png_jmpbuf(png)))
    errx(EXIT_FAILURE, "PNG decoding failed (%s)", path);

  png_init_io(png, f);

  png_read_info(png, pnginfo);

  png_get_IHDR(png, pnginfo, &width, &height, &bit_depth, &pixel_format,
               &interlace_type, int_p_NULL, int_p_NULL);

  if(bit_depth != 8)
    errx(EXIT_FAILURE, "Unsupported bit depth %d in %s", bit_depth, path);

  switch(pixel_format)
    {
    case PNG_COLOR_TYPE_RGB_ALPHA:

      break;

    case PNG_COLOR_TYPE_RGB:

      no_alpha = 1;

      break;

    default:

      errx(EXIT_FAILURE, "Unsupported pixel format in %s", path);
    }

  data = malloc(height * width * 4);

  rows = malloc(sizeof(png_bytep) * height);

  for(row = 0; row < height; ++row)
    rows[row] = data + row * width * 4;

  png_read_image(png, rows);

  if (no_alpha)
    {
      for(row = height; row-- > 0; )
        {
          for (col = width; col-- > 0; )
            {
              rows[row][col * 4 + 3] = 0xff;
              rows[row][col * 4 + 2] = rows[row][col * 3 + 2];
              rows[row][col * 4 + 1] = rows[row][col * 3 + 1];
              rows[row][col * 4 + 0] = rows[row][col * 3 + 0];
            }
        }
    }

  free(rows);

  png_read_end(png, pnginfo);

  png_destroy_read_struct(&png, &pnginfo, png_infopp_NULL);

  fclose(f);

  *ret_data = data;
  *ret_width = width;
  *ret_height = height;

  return 0;
}
