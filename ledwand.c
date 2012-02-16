#include "ledwand.h"

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

    int len = sizeof(request);

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

void ledwand_draw_buffer(const Ledwand *ledwand, const uint8_t *buf, const uint32_t buflen){

    uint32_t i = LEDWAND_PARTS, step = 0;
    do{
        ledwand_send(ledwand, LED_DRAW, step, LEDWAND_PARTSIZE, 0, 0, buf+step, step);
        step += LEDWAND_PARTSIZE;
        usleep(400);
    } while(--i);
}

void ledwand_clear(const Ledwand *ledwand){
    ledwand_send(ledwand, CLEAR, 0, 0, 0, 0, NULL, 0);
}

void ledwand_set_brightness(const Ledwand *ledwand, const uint8_t brightness){
    ledwand_send(ledwand, SET_BRIGHTNESS, 0, 0, 0, 0, &brightness, sizeof(brightness));
}

void ledwand_draw_image(const Ledwand *ledwand, uint8_t *buffer, const uint32_t buf_len){

    if(buf_len != (LEDWAND_PIXEL_X * LEDWAND_PIXEL_Y)){
        printf("Buffer size (%d) should be 448*240\n", buf_len);
        return;
    }

    uint32_t i = 0, j = 0;
    static signed short tmpbuffer[LEDWAND_PIXEL_X * LEDWAND_PIXEL_Y];
    int16_t oldpixel = 0;
    signed short diff = 0;

    for(i=0; i < LEDWAND_PIXEL_X+1; ++i){
        tmpbuffer[i] = buffer[i];
    }

    for(i=449; i < ((LEDWAND_PIXEL_X * (LEDWAND_PIXEL_Y-1))-1); ++i){
        tmpbuffer[i] = buffer[i] * 9 - buffer[i-LEDWAND_PIXEL_X-1] - buffer[i-LEDWAND_PIXEL_X] - buffer[i-LEDWAND_PIXEL_X+1] - buffer[i-1] - buffer[i+1] - buffer[i+LEDWAND_PIXEL_X-1] - buffer[i+LEDWAND_PIXEL_X] - buffer[i+LEDWAND_PIXEL_X+1];
    }

    for(i=(LEDWAND_PIXEL_X*(LEDWAND_PIXEL_Y-1)); i < (LEDWAND_PIXEL_X*LEDWAND_PIXEL_Y); ++i){
        tmpbuffer[i] = buffer[i];
    }
    bzero(buffer, buf_len);

    i = 0;
    do{
        oldpixel = tmpbuffer[i];
        if(oldpixel > LEDWAND_BIAS){
            buffer[i>>3] |= 1 << (7-(i%8));
            diff = oldpixel - 255;
        }
        else{
            diff = oldpixel;
            buffer[i>>3] |= 0 << (7-(i%8));
        }
        tmpbuffer[i+1] += 7 * diff / 16;
        tmpbuffer[i+LEDWAND_PIXEL_X-1] += 3 * diff / 16;
        tmpbuffer[i+LEDWAND_PIXEL_X] += 5 * diff / 16;
        tmpbuffer[i+LEDWAND_PIXEL_X+1] += 1 * diff / 16;

    }while((++i) < LEDWAND_PIXEL_X*LEDWAND_PIXEL_Y);

    for(i = 0, j = 0; i < LEDWAND_PIXEL_X*(LEDWAND_MODULE_Y+9); i+= LEDWAND_PIXEL_X+3*LEDWAND_MODULE_X, j+= LEDWAND_PIXEL_X){
        ledwand_send(ledwand, 18, j, LEDWAND_PIXEL_X, 0, 0, &buffer[i], LEDWAND_PIXEL_X);
        usleep(400);
    }

    free(buffer);
}
