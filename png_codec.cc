// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "png_codec.h"

#include <iostream>

extern "C" {
#include <png.h>
#include <zlib.h>
}

namespace {

// Converts BGRA->RGBA and RGBA->BGRA.
void ConvertBetweenBGRAandRGBA(const unsigned char* input, int pixel_width,
                               unsigned char* output, bool* is_opaque) {
  for (int x = 0; x < pixel_width; x++) {
    const unsigned char* pixel_in = &input[x * 4];
    unsigned char* pixel_out = &output[x * 4];
    pixel_out[0] = pixel_in[2];
    pixel_out[1] = pixel_in[1];
    pixel_out[2] = pixel_in[0];
    pixel_out[3] = pixel_in[3];
  }
}

void ConvertRGBAtoRGB(const unsigned char* rgba, int pixel_width,
                      unsigned char* rgb, bool* is_opaque) {
  for (int x = 0; x < pixel_width; x++) {
    const unsigned char* pixel_in = &rgba[x * 4];
    unsigned char* pixel_out = &rgb[x * 3];
    pixel_out[0] = pixel_in[0];
    pixel_out[1] = pixel_in[1];
    pixel_out[2] = pixel_in[2];
  }
}

}  // namespace

// Decoder --------------------------------------------------------------------
//
// This code is based on WebKit libpng interface (PNGImageDecoder), which is
// in turn based on the Mozilla png decoder.

namespace {

// Gamma constants: We assume we're on Windows which uses a gamma of 2.2.
const double kMaxGamma = 21474.83;  // Maximum gamma accepted by png library.
const double kDefaultGamma = 2.2;
const double kInverseGamma = 1.0 / kDefaultGamma;

class PngDecoderState {
 public:
  // Output is a vector<unsigned char>.
  PngDecoderState(PNGCodec::ColorFormat ofmt, std::vector<unsigned char>* o)
      : output_format(ofmt),
        output_channels(0),
        is_opaque(true),
        output(o),
        row_converter(NULL),
        width(0),
        height(0),
        done(false) {
  }

  PNGCodec::ColorFormat output_format;
  int output_channels;

  // Used during the reading of an SkBitmap. Defaults to true until we see a
  // pixel with anything other than an alpha of 255.
  bool is_opaque;

  // The other way to decode output, where we write into an intermediary buffer
  // instead of directly to an SkBitmap.
  std::vector<unsigned char>* output;

  // Called to convert a row from the library to the correct output format.
  // When NULL, no conversion is necessary.
  void (*row_converter)(const unsigned char* in, int w, unsigned char* out,
                        bool* is_opaque);

  // Size of the image, set in the info callback.
  int width;
  int height;

  // Set to true when we've found the end of the data.
  bool done;

 private:
  PngDecoderState(const PngDecoderState& other);
  PngDecoderState& operator= (const PngDecoderState& other);
};

void ConvertRGBtoRGBA(const unsigned char* rgb, int pixel_width,
                      unsigned char* rgba, bool* is_opaque) {
  for (int x = 0; x < pixel_width; x++) {
    const unsigned char* pixel_in = &rgb[x * 3];
    unsigned char* pixel_out = &rgba[x * 4];
    pixel_out[0] = pixel_in[0];
    pixel_out[1] = pixel_in[1];
    pixel_out[2] = pixel_in[2];
    pixel_out[3] = 0xff;
  }
}

void ConvertRGBtoBGRA(const unsigned char* rgb, int pixel_width,
                      unsigned char* bgra, bool* is_opaque) {
  for (int x = 0; x < pixel_width; x++) {
    const unsigned char* pixel_in = &rgb[x * 3];
    unsigned char* pixel_out = &bgra[x * 4];
    pixel_out[0] = pixel_in[2];
    pixel_out[1] = pixel_in[1];
    pixel_out[2] = pixel_in[0];
    pixel_out[3] = 0xff;
  }
}

// Called when the png header has been read. This code is based on the WebKit
// PNGImageDecoder
void DecodeInfoCallback(png_struct* png_ptr, png_info* info_ptr) {
  PngDecoderState* state = static_cast<PngDecoderState*>(
      png_get_progressive_ptr(png_ptr));

  int bit_depth, color_type, interlace_type, compression_type;
  int filter_type, channels;
  png_uint_32 w, h;
  png_get_IHDR(png_ptr, info_ptr, &w, &h, &bit_depth, &color_type,
               &interlace_type, &compression_type, &filter_type);

  // Bounds check. When the image is unreasonably big, we'll error out and
  // end up back at the setjmp call when we set up decoding.  "Unreasonably big"
  // means "big enough that w * h * 32bpp might overflow an int"; we choose this
  // threshold to match WebKit and because a number of places in code assume
  // that an image's size (in bytes) fits in a (signed) int.
  unsigned long long total_size =
      static_cast<unsigned long long>(w) * static_cast<unsigned long long>(h);
  if (total_size > ((1 << 29) - 1))
    longjmp(png_jmpbuf(png_ptr), 1);
  state->width = static_cast<int>(w);
  state->height = static_cast<int>(h);

  // Expand to ensure we use 24-bit for RGB and 32-bit for RGBA.
  if (color_type == PNG_COLOR_TYPE_PALETTE ||
      (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8))
    png_set_expand(png_ptr);

  // Transparency for paletted images.
  if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS))
    png_set_expand(png_ptr);

  // Convert 16-bit to 8-bit.
  if (bit_depth == 16)
    png_set_strip_16(png_ptr);

  // Expand grayscale to RGB.
  if (color_type == PNG_COLOR_TYPE_GRAY ||
      color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
    png_set_gray_to_rgb(png_ptr);

  // Deal with gamma and keep it under our control.
  double gamma;
  if (png_get_gAMA(png_ptr, info_ptr, &gamma)) {
    if (gamma <= 0.0 || gamma > kMaxGamma) {
      gamma = kInverseGamma;
      png_set_gAMA(png_ptr, info_ptr, gamma);
    }
    png_set_gamma(png_ptr, kDefaultGamma, gamma);
  } else {
    png_set_gamma(png_ptr, kDefaultGamma, kInverseGamma);
  }

  // Tell libpng to send us rows for interlaced pngs.
  if (interlace_type == PNG_INTERLACE_ADAM7)
    png_set_interlace_handling(png_ptr);

  // Update our info now
  png_read_update_info(png_ptr, info_ptr);
  channels = png_get_channels(png_ptr, info_ptr);

  // Pick our row format converter necessary for this data.
  if (channels == 3) {
    switch (state->output_format) {
      case PNGCodec::FORMAT_RGB:
        state->row_converter = NULL;  // no conversion necessary
        state->output_channels = 3;
        break;
      case PNGCodec::FORMAT_RGBA:
        state->row_converter = &ConvertRGBtoRGBA;
        state->output_channels = 4;
        break;
      case PNGCodec::FORMAT_BGRA:
        state->row_converter = &ConvertRGBtoBGRA;
        state->output_channels = 4;
        break;
      default:
        std::cerr << "Unknown output format" << std::endl;
        break;
    }
  } else if (channels == 4) {
    switch (state->output_format) {
      case PNGCodec::FORMAT_RGB:
        state->row_converter = &ConvertRGBAtoRGB;
        state->output_channels = 3;
        break;
      case PNGCodec::FORMAT_RGBA:
        state->row_converter = NULL;  // no conversion necessary
        state->output_channels = 4;
        break;
      case PNGCodec::FORMAT_BGRA:
        state->row_converter = &ConvertBetweenBGRAandRGBA;
        state->output_channels = 4;
        break;
      default:
        std::cerr << "Unknown output format" << std::endl;
        break;
    }
  } else {
    std::cerr << "Unknown input channels" << std::endl;
    longjmp(png_jmpbuf(png_ptr), 1);
  }

  if (state->output) {
    state->output->resize(
        state->width * state->output_channels * state->height);
  }
}

void DecodeRowCallback(png_struct* png_ptr, png_byte* new_row,
                       png_uint_32 row_num, int pass) {
  PngDecoderState* state = static_cast<PngDecoderState*>(
      png_get_progressive_ptr(png_ptr));

  if (pass != 0) std::cerr << "We didn't turn on interlace handling, but libpng is "
                       "giving us interlaced data." << std::endl;
  if (static_cast<int>(row_num) > state->height) {
    std::cerr << "Invalid row" << std::endl;
    return;
  }

  unsigned char* base = NULL;
  if (state->output)
    base = &state->output->front();

  unsigned char* dest = &base[state->width * state->output_channels * row_num];
  if (state->row_converter)
    state->row_converter(new_row, state->width, dest, &state->is_opaque);
  else
    memcpy(dest, new_row, state->width * state->output_channels);
}

void DecodeEndCallback(png_struct* png_ptr, png_info* info) {
  PngDecoderState* state = static_cast<PngDecoderState*>(
      png_get_progressive_ptr(png_ptr));

  // Mark the image as complete, this will tell the Decode function that we
  // have successfully found the end of the data.
  state->done = true;
}

// Automatically destroys the given read structs on destruction to make
// cleanup and error handling code cleaner.
class PngReadStructDestroyer {
 public:
  PngReadStructDestroyer(png_struct** ps, png_info** pi) : ps_(ps), pi_(pi) {
  }
  ~PngReadStructDestroyer() {
    png_destroy_read_struct(ps_, pi_, NULL);
  }
 private:
  png_struct** ps_;
  png_info** pi_;
};

bool BuildPNGStruct(const unsigned char* input, size_t input_size,
                    png_struct** png_ptr, png_info** info_ptr) {
  if (input_size < 8)
    return false;  // Input data too small to be a png

  // Have libpng check the signature, it likes the first 8 bytes.
  if (png_sig_cmp(const_cast<unsigned char*>(input), 0, 8) != 0)
    return false;

  *png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  if (!*png_ptr)
    return false;

  *info_ptr = png_create_info_struct(*png_ptr);
  if (!*info_ptr) {
    png_destroy_read_struct(png_ptr, NULL, NULL);
    return false;
  }

  return true;
}

}  // namespace

// static
bool PNGCodec::Decode(const unsigned char* input, size_t input_size,
                      ColorFormat format, std::vector<unsigned char>* output,
                      int* w, int* h) {
  png_struct* png_ptr = NULL;
  png_info* info_ptr = NULL;
  if (!BuildPNGStruct(input, input_size, &png_ptr, &info_ptr))
    return false;

  PngReadStructDestroyer destroyer(&png_ptr, &info_ptr);
  if (setjmp(png_jmpbuf(png_ptr))) {
    // The destroyer will ensure that the structures are cleaned up in this
    // case, even though we may get here as a jump from random parts of the
    // PNG library called below.
    return false;
  }

  PngDecoderState state(format, output);

  png_set_progressive_read_fn(png_ptr, &state, &DecodeInfoCallback,
                              &DecodeRowCallback, &DecodeEndCallback);
  png_process_data(png_ptr,
                   info_ptr,
                   const_cast<unsigned char*>(input),
                   input_size);

  if (!state.done) {
    // Fed it all the data but the library didn't think we got all the data, so
    // this file must be truncated.
    output->clear();
    return false;
  }

  *w = state.width;
  *h = state.height;
  return true;
}
