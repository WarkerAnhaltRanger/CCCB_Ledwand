#ifndef EMBEDDED_AUDIO_H_INCLUDED
#define EMBEDDED_AUDIO_H_INCLUDED

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>


#define EM_IP "172.23.42.192"
#define EM_PORT 1717
#define EM_BUFFERSIZE 65536

enum EM_CMD {AUDIO_FRAME = 1};

typedef struct em_audio_data{
    uint8_t type;
    uint32_t seq;
    uint16_t len;
} Em_Audio_Data;

typedef struct em_audio{
    int s_sock;
    struct sockaddr_in s_addr;
    uint32_t frame_no;
} Em_Audio;

int em_audio_init(Em_Audio *em_audio);

void em_audio_send(Em_Audio *em_audio, uint8_t *frame, const uint32_t framelen);

#endif // EMBEDDED_AUDIO_H_INCLUDED
