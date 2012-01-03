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
#include <linux/input.h>
#include <linux/uinput.h>
#include <fcntl.h>
#include <stropts.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <netinet/tcp.h>
#include <signal.h>
#include <stdint.h>

#define FILENAME_COUNT 3
#define DEFAULT_PORT 3185
#define MAX_CLIENTS 32
#define AXIS_MIN -32767
#define AXIS_MAX 32767

const char *uinputFileNames[FILENAME_COUNT] = {"/dev/uinput",
    "/dev/input/uinput", "/dev/misc/uinput"};


typedef struct User {
    int s;
    int fd;
    struct uinput_user_dev uidev;
} User;

/* The users and the socket */
User users[MAX_CLIENTS];
int nUsers = 0;
int s;
uint8_t buffer[1024];
int translation[103];
struct input_event event;

void cleanup(){
    int i;

    /* Close the socket */
    shutdown(s, SHUT_RDWR);
    close(s);
    for(i = 0; i < nUsers; i++){
        ioctl(users[i].fd, UI_DEV_DESTROY);
        close(users[i].s);
        close(users[i].fd);
    }
}

void cleanup2(){
    int i;

    /* Close the socket */
    shutdown(s, SHUT_RDWR);
    close(s);
    for(i = 0; i < nUsers; i++){
        ioctl(users[i].fd, UI_DEV_DESTROY);
        close(users[i].s);
        close(users[i].fd);
    }
    exit(0);
}

void removeUser(int i){
    /* Destroy the device and close the sockets */
    ioctl(users[i].fd, UI_DEV_DESTROY);
    close(users[i].s);
    close(users[i].fd);

    /* Move the last user to this position */
    memcpy(&(users[i]), &(users[--nUsers]), sizeof(User));
}

int addUser(int s){
    int i;
    int fd;

    /* Open the uinput device */
    for(i = 0; i < FILENAME_COUNT; i++){
        fd = open(uinputFileNames[i], O_WRONLY | O_NONBLOCK);
        if(fd >= 0){
            break;
        }
    }

    if(fd < 0){
        return 1;
    }

    /* Disable naggle */
    setsockopt(s, IPPROTO_TCP, TCP_NODELAY, &i, sizeof(i));
    setsockopt(s, IPPROTO_TCP, TCP_QUICKACK, &i, sizeof(i));

    /* Enable key and axis events */
    ioctl(fd, UI_SET_EVBIT, EV_KEY);
    ioctl(fd, UI_SET_EVBIT, EV_ABS);

    /* Map most of the buttons */
    for(i = BTN_0; i <= BTN_THUMBR; i++){
        ioctl(fd, UI_SET_KEYBIT, i);
    }
    /* And 40 more happy buttons */
    for(i = BTN_TRIGGER_HAPPY1; i <= BTN_TRIGGER_HAPPY40; i++){
        ioctl(fd, UI_SET_KEYBIT, i);
    }
    /* Map the axes */
    ioctl(fd, UI_SET_ABSBIT, ABS_X);
    ioctl(fd, UI_SET_ABSBIT, ABS_Y);

    /* Create the device */
    memset(&(users[nUsers].uidev), 0, sizeof(struct uinput_user_dev));
    snprintf(users[nUsers].uidev.name, UINPUT_MAX_NAME_SIZE, "Ethernet Gamepad");
    users[nUsers].uidev.id.bustype = BUS_USB;
    users[nUsers].uidev.id.vendor  = 0x569;
    users[nUsers].uidev.id.product = 0x318;
    users[nUsers].uidev.id.version = 1;
    users[nUsers].uidev.absmin[ABS_X] = AXIS_MIN;
    users[nUsers].uidev.absmax[ABS_X] = AXIS_MAX;
    users[nUsers].uidev.absmin[ABS_Y] = AXIS_MIN;
    users[nUsers].uidev.absmax[ABS_Y] = AXIS_MAX;

    write(fd, &(users[nUsers].uidev), sizeof(struct uinput_user_dev));
    ioctl(fd, UI_DEV_CREATE);

    users[nUsers].s = s;
    users[nUsers++].fd = fd;

    return 0;
}

void process(int u, int len){
    int i;
    int k;
    int t;

    for(i = 0; i < len; i++){
        /* The higher bit tells if it's a press or a release */
        k = buffer[i] & 0x7F;
        t = (buffer[i] & 0x80) >> 7;
        event.code = translation[k];

        if(k <= 102){
            event.type = EV_KEY;
            event.value = t;
            write(users[u].fd, &event, sizeof(event));
        }
        else if(k <= 106){
            event.type = EV_ABS;
            if(k <= 104){
                event.value = t * AXIS_MAX;
            }
            else {
                event.value = t * AXIS_MIN;
            }
            write(users[u].fd, &event, sizeof(event));
        }
    }
}

int main(int argc, char *argv[]){
    int i;
    int port;
    struct sockaddr_in sAddr;
    int len;
    fd_set set;

    /* Check for priviledges */
    for(i = 0; i < FILENAME_COUNT; i++){
        s = open(uinputFileNames[i], O_WRONLY | O_NONBLOCK);
        if(s >= 0){
            break;
        }
    }
    if(s < 0){
        fprintf(stderr, "Failed to open the uinput device, "
            "is the module loaded?\n");

        return 1;
    }
    close(s);

    /* Register the signals */
    signal(SIGINT, cleanup2);
    signal(SIGTERM, cleanup2);
    signal(SIGABRT, cleanup2);

    /* Try to set the port */
    if(argc < 2 || !(port = strtoul(argv[1], NULL, 0))){
        port = DEFAULT_PORT;
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

    /* Bind */
    sAddr.sin_family = AF_INET;
    sAddr.sin_port = htons(port);
    sAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    for(i = 0; i < sizeof(sAddr.sin_zero); i++){
        sAddr.sin_zero[i] = 0;
    }
    if(bind(s, (struct sockaddr *) &sAddr, sizeof(struct sockaddr_in))){
        fprintf(stderr, "Failed bind the socket to port %d\n", port);
        cleanup();
        return 3;
    }

    /* Listen */
    if(listen(s, MAX_CLIENTS)){
        fprintf(stderr, "Failed listen to port %d\n", port);
        cleanup();
        return 4;
    }

    /* Success */
    printf("Server running on port %d\n", port);

    /* Generate the table */
    for(i = BTN_0; i <= BTN_THUMBR; i++){
        translation[i - BTN_0] = i;
    }
    for(i = BTN_TRIGGER_HAPPY1; i <= BTN_TRIGGER_HAPPY40; i++){
        translation[i + 62 - BTN_TRIGGER_HAPPY] = i;
    }
    translation[103] = ABS_X;
    translation[104] = ABS_Y;
    translation[105] = ABS_X;
    translation[106] = ABS_Y;
    /* Zero the event struct */
    memset(&event, 0, sizeof(event));

    /* Wait for new clients and process the others */
    for(;;){
        /* Rebuild the set */
        FD_ZERO(&set);

        /* Add the connected users */
        for(i = 0; i < nUsers; i++){
            FD_SET(users[i].s, &set);
        }
        /* Add the listening socket */
        FD_SET(s, &set);

        /* Wait for one of the descriptors */
        if(!select(FD_SETSIZE, &set, NULL, NULL, NULL)){
            cleanup();
            return 5;
        }

        /* New connection */
        if(FD_ISSET(s, &set)){
            /* Accept if we have free slots */
            if(nUsers < MAX_CLIENTS){
                printf("New connection.\n");

                if(addUser(accept(s, NULL, 0))){
                    fprintf(stderr, "Failed to open the uinput device, "
                        "is the module loaded?\n");

                    cleanup();
                    return 1;
                }
            }
            else {
                close(accept(s, NULL, 0));
            }

            continue;
        }

        /* Data */
        for(i = 0; i < nUsers; i++){
            /* Skip the ones without new data */
            if(!(FD_ISSET(users[i].s, &set))){
                continue;
            }

            len = recv(users[i].s, buffer, 1024, 0);

            /* Check for disconnection */
            if(len <= 0){
                printf("A client left\n");
                removeUser(i);
                break;
            }
            /* Process the data */
            process(i, len);
        }
    }

    cleanup();

    return 0;
}
