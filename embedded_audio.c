#include "embedded_audio.h"

int em_audio_init(Em_Audio *em_audio){
    if(em_audio == NULL){
        em_audio = malloc(sizeof(*em_audio));
    }

    bzero(&em_audio->s_addr, sizeof(em_audio->s_addr));
    em_audio->s_addr.sin_family = AF_INET;
    em_audio->s_addr.sin_port = htons(EM_PORT);
    if(!inet_aton(EM_IP, &em_audio->s_addr.sin_addr)){
        perror("inet_aton failed for Embedded_Audio\n");
        return -1;
    }

    if((em_audio->s_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1){
        perror("socket for Embedded_Audio failed\n");
        return -1;
    }

    em_audio->frame_no = 0;

    return 0;
}

void em_audio_send(Em_Audio *em_audio, uint8_t *frame, const uint32_t framelen){

    uint8_t tmpbuffer[EM_BUFFERSIZE];
    Em_Audio_Data adata;

    adata.type = htons(AUDIO_FRAME);
    adata.seq = htonl(em_audio->frame_no++);
    adata.len = htonl(framelen + sizeof(adata));

    uint32_t len = sizeof(adata);

    memcpy(&tmpbuffer, &adata, len);

    memcpy(&tmpbuffer[len], frame, framelen);

    if(sendto(em_audio->s_sock, tmpbuffer, len+framelen, 0, (struct sockaddr*)&em_audio->s_addr, sizeof(em_audio->s_addr)) <= 0){
        perror("Send failed\n");
    }

    free(frame);
}
