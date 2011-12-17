#include "image.h"

uint8_t* image_convert_frame(const uint8_t *displaybuf, const uint32_t displaybuf_len,
                             const uint8_t *buf, const uint32_t buf_len){

    if(buf == NULL || displaybuf == NULL){
        perror("buf or displaybuf is NULL");
        return NULL;
    }

    if(buf_len != IMAGE_FULLSIZE){
        perror("Imagebuf != FULLSIZE\n");
        return NULL;
    }

    if(displaybuf_len != (LEDWAND_PIXEL_X * LEDWAND_MODULES_Y)){
        perror("diplaybuf != 448*20\n");
        return NULL;
    }

}
