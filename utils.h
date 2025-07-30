#ifndef _UTILS_H_
#define _UTILS_H_

#ifdef _WIN32
#include <winsock2.h>
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <io.h>
#include <fcntl.h>
#include <ws2tcpip.h>

#else   // linux and macOS(not tested)
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <arpa/inet.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <sys/types.h>


#define SEND_MAX_TRIES 32
#define BUFFER_SIZE 8192

#define ERR_EXIT(str) \
    do { \
        fprintf(stderr, "%s: %s\n", str, strerror(errno)); \
        exit(1); \
    } while (0)

#define RECV(socket, buff, length, flags) \
    do { \
        size_t received = 0; \
        while (received < length) { \
            ssize_t r = recv(socket, (char*)buff + received, length - received, flags); \
            if (r < 0){ \
                fprintf(stderr, "recv(): %s\n", strerror(errno)); \
                return r; \
            } \
            else if (r == 0) { \
                printf("recv(): EOF reached\n"); \
                exit(0); \
            } \
            received += r; \
        } \
    } while(0)


#ifdef _WIN32
#define SEND(socket, buff, buff_size, flags) \
    do { \
        if(send(socket, (char*)buff, buff_size, flags) < 0){ \
            fprintf(stderr, "send(): %s\n", strerror(errno)); \
            return -1; \
        } \
    } while(0)
#else
#define SEND(socket, buff, buff_size, flags) \
    do { \
        if(send(socket, buff, buff_size, flags) < 0){ \
            fprintf(stderr, "send(): %s\n", strerror(errno)); \
            return -1; \
        } \
    } while(0)
#endif

#ifdef _WIN32
#define SENDFILE(out_fd, in_fd, offset, count) sendfile_win(out_fd, in_fd, offset, count)
#define OPEN(path, oflag) _open(path, _##oflag)
#define STAT _stat
#define FSTAT(fildes, buf) _fstat(fildes, buf)
#define CLOSE(fildes) _close(fildes)
#else
#define SENDFILE(out_fd, in_fd, offset, count) sendfile(out_fd, in_fd, offset, count)
#define OPEN(path, oflag) open(path, oflag)
#define STAT stat
#define FSTAT(fildes, buf) fstat(fildes, buf)
#define CLOSE(fildes) close(fildes)
#endif

typedef enum {
    STATUS_DATA_LOST = 1,
    STATUS_OK,
    STATUS_EOF,
    STATUS_ERROR,
} Status;

int send_data(int socket, const void *buffer, size_t length, int flags, int success_response_value);
int send_file(int socket, const char* file_path, const char* filename);
int receive_file(int socket);

uint64_t htonll(uint64_t value);
uint64_t ntohll(uint64_t value);

#ifdef _WIN32
int sendfile_win(SOCKET out_fd, int in_fd, off_t *offset, size_t count);
void wsa_print_error(const char* prompt, int err);
#endif


//  File transferting protocol
//  Server <--------Connects----------- Client
//  Server ---------Num_of_files------> Client
//  Server <--------Num_of_files------> Client   (send back as confirmation of successfully receiving data)
//
//  Server ---------Filename_size-----> Client
//  Server <--------Filename_size------ Client   (send back as confirmation of successfully receiving data)
//
//  Server ---------Filename----------> Client  
//  Server <--------STATUS------------- Client      (compare sum of received packets of `filename` and previously received filesize
//  Server ---------Filesize----------> Client
//  Server <--------Filesize----------- Client     (send back as confirmation of successfully receiving data)
//  Server ---------Data--------------> Client
//  Server <--------STATUS------------- Client   (result in comparing received filesize with counted filesize of data packets)




#endif  //_UTILS_H_
