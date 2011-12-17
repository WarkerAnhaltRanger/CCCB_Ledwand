#ifndef IMAGE_H_INCLUDED
#define IMAGE_H_INCLUDED
#include "ledwand.h"

#define IMAGE_FULLSIZE (LEDWAND_PIXEL_X * LEDWAND_PIXEL_Y)
#define BLACK 1

uint8_t* image_convert_frame(const uint8_t *buf, const uint32_t buf_len);

#endif // IMAGE_H_INCLUDED
