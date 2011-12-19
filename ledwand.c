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
        usleep(500);
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
    static signed short tmpbuffer[448*240+721];
    int16_t oldpixel = 0;
    signed short diff = 0;

    tmpbuffer[0] = buffer[0] * 5 - buffer[i+1] - buffer[i+447] - buffer[i+448] - buffer[i+449];

    while((++i) < 448){
        tmpbuffer[i] = buffer[i] * 6 - buffer[i-1] - buffer[i+1] - buffer[i+447] - buffer[i+448] - buffer[i+449];
    }

    while((++i) < 448*239){
        tmpbuffer[i] = buffer[i] * 9 - buffer[i-449] - buffer[i-448] - buffer[i-447] - buffer[i-1] - buffer[i+1] - buffer[i+447] - buffer[i+448] - buffer[i+449];
    }

    while((++i) < (448*240)-1){
        tmpbuffer[i] = buffer[i] * 6 - buffer[i-1] - buffer[i+1] - buffer[i-447] - buffer[i-448] - buffer[i-449];
    }
    ++i;
    tmpbuffer[i] = buffer[i] * 2 - buffer[i-1];

    bzero(buffer, buf_len);

    i = 0;
    do{
        oldpixel = tmpbuffer[i];
        if(oldpixel > LEDWAND_BIAS){
            buffer[i>>3] |= 1 << (7-(i%8)); // hier ist das problem
            diff = oldpixel - 255;
        }
        else{
            diff = oldpixel;
            buffer[i>>3] |= 0 << (7-(i%8)); // hier ist das problem
        }
        tmpbuffer[i+1] += 7 * diff / 16;
        tmpbuffer[i+447] += 3 * diff / 16;
        tmpbuffer[i+448] += 5 * diff / 16;
        tmpbuffer[i+449] += 1 * diff / 16;

    }while((++i) < 448*240);

    for(i = 0, j = 0; i < 448*29; i+= 448+4*56, j+= 448){
        ledwand_send(ledwand, 18, j, 448, 0, 0, &buffer[i], 448);
        usleep(400);
    }
}
