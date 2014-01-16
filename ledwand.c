#include "ledwand.h"

#include "ledwand_data.h"

#include <zlib.h>

/* more readable -Weverything */
#define UNUSED __attribute__((__unused__))

/* uncomment to see old & new in parallel */
//#define DITHER_TEST_SPLITSCREEN

int ledwand_init(Ledwand *ledwand){

    if(ledwand == NULL){
        ledwand = malloc(sizeof(*ledwand));
    }

    bzero(&ledwand->s_addr, sizeof(ledwand->s_addr));
    ledwand->s_addr.sin_family = AF_INET;
    ledwand->s_addr.sin_port = htons(2342);
    if(!inet_aton(LEDWAND_IP, &ledwand->s_addr.sin_addr)){
        perror("inet_aton failed\n");
        return -1;
    }

    if((ledwand->s_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1){
        perror("socket failed\n");
        return -1;
    }

    return 0;
}

void ledwand_send(const Ledwand *ledwand,
                  const uint16_t cmd,
                  const uint16_t xpos,
                  const uint16_t ypos,
                  const uint16_t xsize,
                  const uint16_t ysize,
                  const uint8_t *text,
                  const uint32_t text_len)
{
    uint8_t buf[LEDWAND_BUFSIZE];

    Request request;
    request.cmd = htons(cmd);
    request.xpos = htons(xpos);
    request.ypos = htons(ypos);
    request.xsize = htons(xsize);
    request.ysize = htons(ysize);

    size_t len = sizeof(request);

    if(text_len > (LEDWAND_BUFSIZE - text_len)){
        perror("ERROR: Textlen bigger than buflen\n");
        return;
    }

    memcpy(&buf, &request, len);
    if(text != NULL){
        memcpy(&buf[len], text, text_len);
        len += text_len;
    }

    if(sendto(ledwand->s_sock, buf, len, 0, (struct sockaddr*)&ledwand->s_addr, sizeof(ledwand->s_addr)) <= 0){
        perror("Send failed\n");
    }
}

void ledwand_draw_buffer(const Ledwand *ledwand, const uint8_t *buf, const uint32_t UNUSED buflen){

    uint32_t i = LEDWAND_PARTS, step = 0;
    do{
        ledwand_send(ledwand, LED_DRAW, step, LEDWAND_PARTSIZE, 0, 0, buf+step, step);
        step += LEDWAND_PARTSIZE;
//      usleep(400);
    } while(--i);
}

void ledwand_clear(const Ledwand *ledwand){
    ledwand_send(ledwand, CLEAR, 0, 0, 0, 0, NULL, 0);
}

void ledwand_set_brightness(const Ledwand *ledwand, const uint8_t brightness){
    ledwand_send(ledwand, SET_BRIGHTNESS, 0, 0, 0, 0, &brightness, sizeof(brightness));
}

/* (1) brightness correction XXX */
static inline void image_histogram_correction( uint8_t * buffer ) { /* {{{ */
    /* (pixel-pre_offset)*factor+post_offset : stuff below pre_offset will be
     * pulled into negative, hence not scaled out of cut off area when
     * multiplying with factor.  Awesome for starfields on not-exactly black.
     */
    int   pre_offset, post_offset;
    float factor;
    /* compute histogram */
    size_t i, j;
    size_t histogram[256];  for ( i = 0; i < 256 ; i++ )  histogram[i] = 0;
    for ( i = 0; i < LEDWAND_PIXEL_X*LEDWAND_PIXEL_Y ; histogram[buffer[i++]]++ ) /* nothing else */ ;
    /* find interesting spots: approx. 1 line completely white/black, 3 lines may be scaled out of gamut */
    uint8_t mincut, minshift, maxshift;
    for ( i =   0, j = 0 ; j <   LEDWAND_PIXEL_X ; j += histogram[i++] ) /* nothing else */ ;
    mincut = i;
    for ( /* keep i/j */ ; j < 3*LEDWAND_PIXEL_X ; j += histogram[i++] ) /* nothing else */ ;
    minshift = i;
    for ( i = 255, j = 0 ; j < 3*LEDWAND_PIXEL_X ; j += histogram[i--] ) /* nothing else */ ;
    maxshift = i;
    /* constrain cutoff values */
    if ( mincut   >  20 ) { mincut   =  20; }
    if ( minshift >  64 ) { minshift =  64; }
    if ( maxshift < 192 ) { maxshift = 192; }
    /* determine offsets & factors */
    pre_offset  = -mincut;
    post_offset = -minshift;
    factor      = (255.0-post_offset)/maxshift;
    /* iterate over image / scale & shift brightness */
    for ( int y = 0; y < LEDWAND_PIXEL_Y; y++ ) {
#ifdef DITHER_TEST_SPLITSCREEN
        for ( int x = 1; x < LEDWAND_PIXEL_X/2; x++ ) { /* @@SPLITSCREEN@@ */
#else
        for ( int x = 1; x < LEDWAND_PIXEL_X-1; x++ ) { /* @@NOSPLITSCREEN@@ */
#endif
            int v = ((float)buffer[y*LEDWAND_PIXEL_X+x]+pre_offset)*factor+post_offset;
            if ( v <   0 ) { v =   0; }
            if ( v > 255 ) { v = 255; }
            buffer[y*LEDWAND_PIXEL_X+x] = v;
        }
    }
}
/* }}} */

/* (2) blur XXX */
static inline void image_blur( uint8_t * in, uint8_t * out ) { /* {{{ */
    /* copy first line */
    for ( size_t x = 0; x < LEDWAND_PIXEL_X ; x++ ) { out[x] = in[x]; }
    /* process image */
    for ( size_t y = 1; y < LEDWAND_PIXEL_Y-1; y++ ) {
        size_t i = y*LEDWAND_PIXEL_X;
        /* copy border pixels */
        out[i] = in[i];
        out[i+LEDWAND_PIXEL_X-1] = in[i+LEDWAND_PIXEL_X-1];
        /* copy & blur image (minus border) */
#ifdef DITHER_TEST_SPLITSCREEN
        for ( size_t x = 1; x < LEDWAND_PIXEL_X/2; x++ ) { /* @@SPLITSCREEN@@ */
#else
        for ( size_t x = 1; x < LEDWAND_PIXEL_X-1; x++ ) { /* @@NOSPLITSCREEN@@ */
#endif
            size_t i1 = i-LEDWAND_PIXEL_X, i2 = i, i3 = i+LEDWAND_PIXEL_X;
            int v = ( in[i1+x-1] +   in[i1+x] + in[i1+x+1]
                    + in[i2+x-1] + 8*in[i2+x] + in[i2+x+1]
                    + in[i3+x-1] +   in[i3+x] + in[i3+x+1]
                    ) / 16;
            if ( v <   0 ) { v =   0; }
            if ( v > 255 ) { v = 255; }
            out[i2+x] = v;
        }
#ifdef DITHER_TEST_SPLITSCREEN
        /* copy other half if doing split-screen */
        for ( size_t x = LEDWAND_PIXEL_X/2 ; x < LEDWAND_PIXEL_X-1 ; x++ ) {
            out[i+x] = in[i+x];
        }
#endif
    }
    /* copy last line */
    for ( int x = 0; x < LEDWAND_PIXEL_X ; x++ ) { out[x] = in[x]; }
} /* }}} */

/* (3) sharpen TODO */
static inline void image_sharpen( uint8_t * in, uint8_t * out ) { /* {{{ */
    /* NOTE precarious optimization: blur & sharpen don't touch border, hence
     * assume that it is already in `out` and do not copy again */
    for ( size_t y = 1; y < LEDWAND_PIXEL_Y-1; y++ ) {
        size_t i = y*LEDWAND_PIXEL_X;
        /* copy & blur image (minus border) */
        for ( size_t x = 1; x < LEDWAND_PIXEL_X-1; x++ ) {
            size_t i1 = i-LEDWAND_PIXEL_X, i2 = i, i3 = i+LEDWAND_PIXEL_X;
            int v = - in[i1+x-1] -  in[i1+x] - in[i1+x+1]
                    - in[i2+x-1] +9*in[i2+x] - in[i2+x+1]
                    - in[i3+x-1] -  in[i3+x] - in[i3+x+1] ;
            if ( v <   0 ) { v =   0; }
            if ( v > 255 ) { v = 255; }
            out[i2+x] = v;
        }
    }
} /* }}} */

/* inner loops {{{ */

#define LASTLINE_FIRSTPIXEL (LEDWAND_PIXEL_X*(LEDWAND_PIXEL_Y-1)+1)

static inline void sub_image_dither_floydsteinberg( size_t i, ssize_t dir, uint8_t * in, uint8_t * out ) {
    signed short diff;
    int d, oldpixel = in[i];
    if (oldpixel > LEDWAND_BIAS) {
        out[i>>3] |= 1 << (7-(i%8));
        diff = oldpixel - 255;
    } else {
        diff = oldpixel;
        //out[i>>3] |= 0 << (7-(i%8)); /* ?!? does nothing, also already zeroed */
    }
    d = in[i+dir]+7*diff/16; d = (d<0)?0:(d>255?255:d);
        in[i+dir] = d;
    if (i < LASTLINE_FIRSTPIXEL) {
        d = in[i+LEDWAND_PIXEL_X-dir]+3*diff/16; d = (d<0)?0:(d>255?255:d);
            in[i+LEDWAND_PIXEL_X-dir] = d;
        d = in[i+LEDWAND_PIXEL_X    ]+5*diff/16; d = (d<0)?0:(d>255?255:d);
            in[i+LEDWAND_PIXEL_X    ] = d;
        d = in[i+LEDWAND_PIXEL_X+dir]+  diff/16; d = (d<0)?0:(d>255?255:d);
            in[i+LEDWAND_PIXEL_X+dir] = d;
    }
}

static inline void sub_image_dither_ostromoukhov( size_t i, ssize_t dir, uint8_t * in, uint8_t * out ) {
    uint8_t diff;
    int d, oldpixel = in[i];
    if (oldpixel > LEDWAND_BIAS) {
        out[i>>3] |= 1 << (7-(i%8));
        diff = -(oldpixel - 255);
        d = in[i+dir]-errordiff[diff][0]; d = (d<0)?0:(d>255?255:d);
            in[i+dir] = d;
        if (i < LASTLINE_FIRSTPIXEL) {
            d = in[i+LEDWAND_PIXEL_X-dir]-errordiff[diff][1]; d = (d<0)?0:(d>255?255:d);
                in[i+LEDWAND_PIXEL_X-dir] = d;
            d = in[i+LEDWAND_PIXEL_X    ]-errordiff[diff][2]; d = (d<0)?0:(d>255?255:d);
                in[i+LEDWAND_PIXEL_X    ] = d;
        }
    } else {
        //out[i>>3] &= ~(1 << (7-(i%8))); /* already zeroed */
        diff = oldpixel;
        d = in[i+dir]+errordiff[diff][0]; d = (d<0)?0:(d>255?255:d);
            in[i+dir] = d;
        if (i < LASTLINE_FIRSTPIXEL) {
            d = in[i+LEDWAND_PIXEL_X-dir]+errordiff[diff][1]; d = (d<0)?0:(d>255?255:d);
                in[i+LEDWAND_PIXEL_X-dir] = d;
            d = in[i+LEDWAND_PIXEL_X    ]+errordiff[diff][2]; d = (d<0)?0:(d>255?255:d);
                in[i+LEDWAND_PIXEL_X    ] = d;
        }
    }
}
/* }}} */

#ifdef DITHER_TEST_SPLITSCREEN /* {{{ */
static inline void image_dither( uint8_t * in, uint8_t * out ) {
    for ( size_t y = 0 ; y < LEDWAND_PIXEL_Y ; y++ ) {
        size_t base = y*LEDWAND_PIXEL_X;
        /* left half */
        if ( y%2 == 0 ) {
            for ( size_t x = 0 ; x < LEDWAND_PIXEL_X/2 ; x++ ) {
                sub_image_dither_ostromoukhov( base+x,  1, in, out );
            }
        } else {
            for ( ssize_t x = LEDWAND_PIXEL_X/2 ; x >= 0 ; x-- ) {
                sub_image_dither_ostromoukhov( base+x, -1, in, out );
            }
        }
        /* right half */
        for ( size_t x = LEDWAND_PIXEL_X/2 ; x < LEDWAND_PIXEL_X ; x++ ) {
            sub_image_dither_floydsteinberg( base+x,  1, in, out );
        }
    }
}
#else /* }}} */
static inline void image_dither( uint8_t * in, uint8_t * out ) { /* {{{ */
    for ( size_t y = 0 ; y < LEDWAND_PIXEL_Y ; y++ ) {
        size_t base = y*LEDWAND_PIXEL_X;
        if ( y%2 == 0 ) {
            for ( size_t x = 0 ; x < LEDWAND_PIXEL_X ; x++ ) {
                sub_image_dither_ostromoukhov( base+x,  1, in, out );
            }
        } else {
            for ( ssize_t x = LEDWAND_PIXEL_X-1 ; x >= 0 ; x-- ) {
                sub_image_dither_ostromoukhov( base+x, -1, in, out );
            }
        }
    }
}
#endif /* }}} */

void ledwand_draw_image(const Ledwand *ledwand, uint8_t *buffer, const uint32_t buf_len){

    uint8_t buf[8985]; // 10 + 56*8*20 + compress overhead
    uint16_t *b16 = (uint16_t *)buf;
    z_stream zs;

    if (buf_len != (LEDWAND_PIXEL_X * LEDWAND_PIXEL_Y)) {
        //printf("Buffer size (%d) should be 448*240\n", buf_len);
        return;
    }

    uint8_t tmpbuf[LEDWAND_PIXEL_X*LEDWAND_PIXEL_Y];
    /*
    improved algo: (1) brightness correction, (2) blur, (3) sharpen, (4) dither XXX
    */
    image_histogram_correction( buffer );
    image_blur( buffer, tmpbuf );
    /* NOTE precarious optimization: sharpen assumes the border is already in buffer */
    image_sharpen( tmpbuf, buffer );

    bzero( tmpbuf, buf_len );
    image_dither( buffer, tmpbuf );

    memset(buf, 0, 10);
    b16[0] = ntohs(18);
    b16[2] = ntohs((sizeof buf) - 10);
    b16[3] = ntohs(0x677a);

    memset(&zs, 0, sizeof zs);
    if (Z_OK != deflateInit(&zs, 9)) {
       perror("deflateInit");
       return;
    }

    zs.next_in = tmpbuf;
    zs.next_out = buf + 10;
    zs.avail_out = sizeof buf - 10;
    zs.data_type = Z_BINARY;

    for (int i = 0; i < 19; i++) {
        zs.avail_in = 448;
        if (Z_OK != deflate(&zs, Z_NO_FLUSH)) {
            printf("deflate error\n");
            return;
        }

        zs.next_in += 4*LEDWAND_MODULE_X;
    }

    zs.avail_in = 448;
    if (Z_STREAM_END != deflate(&zs, Z_FINISH)) {
        printf("deflate error\n");
        return;
    }

    if (Z_OK != deflateEnd(&zs)) {
       perror("deflateEnd");
       return;
    }

    if (-1 == sendto(ledwand->s_sock, buf, sizeof buf - zs.avail_out, 0, (struct sockaddr*)&ledwand->s_addr, sizeof ledwand->s_addr))
        perror("send");

    free( buffer );
}

