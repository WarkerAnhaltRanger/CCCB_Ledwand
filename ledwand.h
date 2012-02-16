#ifndef LEDWAND_H_INCLUDED
#define LEDWAND_H_INCLUDED

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>

#define LEDWAND_MODULE_PIXEL_X 8
#define LEDWAND_MODULE_PIXEL_Y 12
#define LEDWAND_MODULE_X 56
#define LEDWAND_MODULE_Y 20
#define LEDWAND_PIXEL_X (LEDWAND_MODULE_X * LEDWAND_MODULE_PIXEL_X)
#define LEDWAND_PIXEL_Y (LEDWAND_MODULE_Y * LEDWAND_MODULE_PIXEL_Y)
#define LEDWAND_PARTS 14
#define LEDWAND_PARTSIZE ((LEDWAND_PIXEL_X * LEDWAND_PIXEL_Y) / LEDWAND_PARTS)

#define LEDWAND_BIAS 127

#define LEDWAND_BUFSIZE 65535
#define LEDWAND_IP "172.23.42.29"

enum CMD {CLEAR = 2, ASCII = 3, SET_BRIGHTNESS = 7, REDRAW = 8, HARD_RESET = 11, REFRESH = 17, LED_DRAW = 18};

typedef struct Ledwand {
    int s_sock;
    struct sockaddr_in s_addr;
} Ledwand;

typedef struct Request {
    uint16_t cmd;
    uint16_t xpos;
    uint16_t ypos;
    uint16_t xsize;
    uint16_t ysize;
}Request;

int ledwand_init(Ledwand *ledwand);

void ledwand_send(const Ledwand *ledwand,
                  const uint16_t cmd,
                  const uint16_t xpos,
                  const uint16_t ypos,
                  const uint16_t xsize,
                  const uint16_t ysize,
                  const uint8_t *text,
                  const uint32_t text_len);

void ledwand_clear(const Ledwand *ledwand);

void ledwand_set_brightness(const Ledwand *ledwand, const uint8_t brightness);

void ledwand_draw_buffer(const Ledwand *ledwand, const uint8_t *buf, const uint32_t buf_len);

void ledwand_draw_image(const Ledwand *ledwand, uint8_t *buf, const uint32_t buf_len);



#endif // LEDWAND_H_INCLUDED
