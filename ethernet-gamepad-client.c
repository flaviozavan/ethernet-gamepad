/*
Copyright (c) 2011, Flávio Zavan
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the <organization> nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <SDL/SDL.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <stdlib.h>
#include <netdb.h>

#define WIDTH 320
#define HEIGHT 240
#define BPP 24
#define DEFAULT_PORT 3185
#define FILENAME_COUNT 3

const char *bgFiles[FILENAME_COUNT] = {"bg.bmp",
    "/usr/local/share/ethernet-gamepad/bg.bmp",
    "/usr/share/ethernet-gamepad"};

int axes = 0;

int main(int argc, char *argv[]){
    SDL_Surface *screen, *image;
    SDL_Event event;
    struct addrinfo *result;
    struct sockaddr_in sAddr;
    int i;
    int map[1024];
    uint8_t toSend;
    int port = 0;
    int s;

    if(argc < 2){
        help:
        fprintf(stderr, "Usage: %s IP [FLAGS]\n", argv[0]);
        fprintf(stderr, "    -a    map the arrow keys to axes 0 and 1\n");
        fprintf(stderr, "    -p PORT    set the port\n");
        fprintf(stderr, "    -h    display this text\n");
        return 1;
    }
    /* Parse options */
    while((i = getopt(argc - 1, argv + 1, "ahp:")) >= 0){
        switch(i){
            case '?':
                if(optopt == 'p'){
                    fprintf(stderr, "Options -p requires an argument.\n");
                    goto help;
                }
            case 'p':
                port = atoi(optarg);
                break;

            case 'h':
                goto help;

            case 'a':
                axes = 1;
        }
    }

    /* Try to set the port */
    if(!port){
        port = DEFAULT_PORT;
    }

    printf("Connecting to %s using port %d.\n", argv[1], port);
    if(axes){
        printf("Using the arrow keys as axes.\n");
    }

    /* Open the socket */
    s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(s < 0){
        fprintf(stderr, "Failed to open a socket.\n");
        return 2;
    }

    /* Disable Naggle */
    i = 1;
    setsockopt(s, IPPROTO_TCP, TCP_NODELAY, &i, sizeof(i));
    setsockopt(s, IPPROTO_TCP, TCP_QUICKACK, &i, sizeof(i));

    /* Resolve hostname */
    if(getaddrinfo(argv[1], NULL, NULL, &result)){
        fprintf(stderr, "Failed to resolve %s\n", argv[1]);
        close(s);
        return 3;
    }

    /* Copy the IP */
    sAddr.sin_addr.s_addr =
        ((struct sockaddr_in *) result->ai_addr)->sin_addr.s_addr;
    /* Free the memory */
    freeaddrinfo(result);

    /* Set the rest */
    sAddr.sin_family = AF_INET;
    sAddr.sin_port = htons(port);
    for(i = 0; i < sizeof(sAddr.sin_zero); i++){
        sAddr.sin_zero[i] = 0;
    }

    /* Finally try to connect */
    if(connect(s, (struct sockaddr *) &sAddr, sizeof(sAddr))){
        fprintf(stderr, "Failed to connect to %s using port %d\n",
            argv[1], port);
        close(s);
        return 4;
    }


    /* Reset the map */
    memset(map, 102, sizeof(map));
    /* Shifts, alts controls and supers */
    map[304] = 0;
    map[306] = 1;
    map[308] = 2;
    map[303] = 3;
    map[305] = 4;
    map[313] = 5;
    map[311] = 6;
    map[312] = 7;

    /* Escape */
    map[27] = 10;
    /* Caps lock */
    map[301] = 11;
    /* Return */
    map[13] = 12;
    /* < + > / numerals ; - */
    for(i = 44; i <= 61; i++){
        map[i] = i - 31;
    }
    /* [ ] letters, from 29 */
    for(i = 91; i <= 122; i++){
        map[i] = i - 62;
    }
    /* Delete */
    map[127] = 60;
    /* Keypad, arrowkeys, insert, home, pageup, pagedown, function keys */
    for(i = 256; i <= 293; i++){
        map[i] = i - 195;
    }
    /* Pause, space and print screen */
    map[19] = 99;
    map[32] = 100;
    map[316] = 101;
    /* Remap arrow keys if in axes mode */
    if(axes){
        map[273] = 106;
        map[274] = 104;
        map[275] = 103;
        map[276] = 105;
    }
    
    
    /* Initialize SDL */
    if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER)){
        fprintf(stderr, "Could not initialize SDL.\n");

        return 5;
    }

    /* Create Screen */
    if(!(screen = SDL_SetVideoMode(WIDTH, HEIGHT, BPP, SDL_SWSURFACE))){
        SDL_Quit();
        fprintf(stderr, "Could not create screen.\n");

        return 6;
    }
    
    /* Set window caption */
    SDL_WM_SetCaption("Ethernet Gamepad Client", NULL);

    /* Load the image */
    for(i = 0; i < FILENAME_COUNT; i++){
        image = SDL_LoadBMP(bgFiles[i]);

        if(image){
            SDL_BlitSurface(image, NULL, screen, NULL);
            SDL_Flip(screen);
            break;
        }
    }
    if(!image){
        fprintf(stderr, "Failed to load the background file.\n");
    }

    do {
        toSend = 0;
        SDL_WaitEvent(&event);

        switch(event.type){
            case SDL_KEYDOWN:
            toSend = 0x80;

            case SDL_KEYUP:
            toSend |= map[event.key.keysym.sym] & 0x7F;

            if(send(s, &toSend, 1, 0) < 0){
                break;
            }
        }
    } while(event.type != SDL_QUIT);

    /* Cleanup */
    SDL_FreeSurface(screen);
    SDL_FreeSurface(image);
    SDL_Quit();
    close(s);

    return 0;
}
