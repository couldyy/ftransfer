#include "utils.h"

uint64_t htonll(uint64_t value) {
#if __BYTE_ORDER == __LITTLE_ENDIAN
    return ((uint64_t)htonl(value & 0xFFFFFFFF) << 32) | htonl(value >> 32);
#else
    return value;
#endif
}

uint64_t ntohll(uint64_t value) {
#if __BYTE_ORDER == __LITTLE_ENDIAN
    return ((uint64_t)ntohl(value & 0xFFFFFFFF) << 32) | ntohl(value >> 32);
#else
    return value;
#endif
}

#ifdef _WIN32
void wsa_print_error(const char* prompt, int err) {
    //int err;
    char msgbuf [256];   // for a message up to 255 bytes.

    msgbuf [0] = '\0';    // Microsoft doesn't guarantee this on man page.
    //err = WSAGetLastError ();
    FormatMessage (FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,   // flags
                   NULL,                // lpsource
                   err,                 // message id
                   MAKELANGID (LANG_NEUTRAL, SUBLANG_DEFAULT),    // languageid
                   msgbuf,              // output buffer
                   sizeof (msgbuf),     // size of msgbuf, bytes
                   NULL);               // va_list of arguments

    if (! *msgbuf)
       snprintf (msgbuf, sizeof(msgbuf), "%d", err);  // provide error # if no string available
    
    fprintf(stderr, "%s: %s\n", prompt, msgbuf);
}

int sendfile_win(SOCKET out_fd, int in_fd, off_t *offset, size_t count){
    off_t orig;

    if (offset != NULL) {

        /* Save current file offset and set offset to value in '*offset' */

        orig = lseek(in_fd, 0, SEEK_CUR);
        if (orig == -1)
            return -1;
        if (_lseek(in_fd, *offset, SEEK_SET) == -1)
            return -1;
    }

    size_t totSent = 0;

    while (count > 0) {
        size_t toRead = min(BUFFER_SIZE, count);

        char buf[BUFFER_SIZE];
        ssize_t numRead = _read(in_fd, buf, toRead);
		//ssize_t numRead = _read(buf, sizeof(char), toRead, in_fd);
        if (numRead == -1)
            return -1;
        if (numRead == 0)
            break;                      /* EOF */

        //ssize_t numSent = write(out_fd, buf, numRead);
		ssize_t numSent = send(out_fd, buf, numRead,0);
        if (numSent == -1)
            return -1;
        if (numSent == 0)               /* Should never happen */
            printf("sendfile: write() transferred 0 bytes\n");

        count -= numSent;
        totSent += numSent;
    }

    if (offset != NULL) {

        /* Return updated file offset in '*offset', and reset the file offset
           to the value it had when we were called. */

        *offset = _lseek(in_fd, 0, SEEK_CUR);
        if (*offset == -1)
            return -1;
        if (_lseek(in_fd, orig, SEEK_SET) == -1)
            return -1;
    }

    return totSent;
}
#endif

int send_data(int socket, const void *buffer, size_t length, int flags, int success_response_value) 
{
    ssize_t sent_bytes = 0;
    while(sent_bytes < length) {
        int len = send(socket, ((char*)buffer) + sent_bytes, length - sent_bytes, 0);		
        if (len < 0) {
              fprintf(stderr, "send: %s\n", strerror(errno));
              exit(1);
        }
        sent_bytes += len;
        //printf("sent_bytes: %ld, length: %ld\n", sent_bytes, length);
    }

    int response_status;
    ssize_t recevied_bytes = 0;
        RECV(socket, &response_status, sizeof(response_status), 0);
        printf("response: %d\nhex: ", response_status);
        unsigned char* p = (unsigned char*)&response_status;
        for (int i = 0; i < sizeof(response_status); i++)
            printf("%02X ", p[i]);
        printf("\n");

    //}
    if(response_status == success_response_value)
        return 0;
    else 
        return -1;
}



int send_file(int socket, const char* file_path, const char* filename)
{
    uint32_t status; 
    int fd = OPEN(file_path, O_RDONLY);
    if (fd == -1) {
        ERR_EXIT("open");
    }

	struct STAT file_stat;
    /* Get file stats */
    if (FSTAT(fd, &file_stat) < 0) {
        ERR_EXIT("fstat");
    }
    
    //sprintf(file_size, "%ld", file_stat.st_size);

    //send filename size
    size_t filename_size = htonl(strlen(filename) + 1);
    SEND(socket, &filename_size, sizeof(filename_size), 0);
    filename_size = ntohl(filename_size);       // convert back for future operations

    size_t response_filename_size;
    RECV(socket, &response_filename_size, sizeof(response_filename_size), 0);
    response_filename_size = ntohl(response_filename_size);
    printf("received filename size %d\n", response_filename_size);

    if(filename_size != response_filename_size) {		//send file size (on success, client should send STATUS_OK)
        fprintf(stderr, "'%s': Error occured while sending filename size\n", filename);
        status = htonl((uint32_t)STATUS_DATA_LOST);
        SEND(socket, &status, sizeof(status), 0);
        return -1;
    }
    else {
        status = htonl((uint32_t)STATUS_OK);
        SEND(socket, &status, sizeof(status), 0);
    }
    fprintf(stdout, "'%s': Server sent filename size: %lu\n", filename, filename_size);


    //send filename
    SEND(socket, filename, filename_size, 0);
    RECV(socket, &status, sizeof(status), 0);
    status = ntohl(status);
    if(status != STATUS_OK) {		
        fprintf(stderr, "'%s': Error occured while sending filename\n", filename);
        return -1;
    }
    fprintf(stdout, "'%s': Server sent filename\n", filename);       // TODO replace with actual return value of send()

    //RECV(socket, &status, sizeof(status), 0);
    //if(status != STATUS_OK) {
    //    fprintf(stderr, "'%s': filename have not been received on liecnt successfully\n", filename);
    //    return -1;
    //}

    size_t file_size = htonll(file_stat.st_size);
    printf("file size: %lu\n", ntohll(file_size));
    SEND(socket, &file_size, sizeof(file_size), 0);
    file_size = ntohll(file_size);       //convert back for future operati0ns

    size_t response_filesize;
    RECV(socket, &response_filesize, sizeof(response_filesize), 0);
    response_filesize = ntohll(response_filesize);
    if(file_size != response_filesize) {		//send file size (on success, client should echo it back)
        status = htonl((uint32_t)STATUS_DATA_LOST);
        SEND(socket, &status, sizeof(status), 0);
        fprintf(stderr, "'%s': Error occured while sending file size\n", filename);
        return -1;
    }
    status = htonl((uint32_t)STATUS_OK);
    SEND(socket, &status, sizeof(status), 0);
    printf("'%s': Server sent filesize: %lu\n", filename, file_size);

    off_t offset = 0;
    int i = 0;
    ssize_t sent_bytes;
    ssize_t remain_data = file_size;
    /* Sending file data */

    while ((sent_bytes = SENDFILE(socket, fd, &offset, BUFSIZ)) > 0) {
        ++i;
        remain_data -= sent_bytes;
        fprintf(stdout, "'%s': [%d] sent %ld bytes \t offset: %ld \tremaining data: %ld bytes\n", filename, i, sent_bytes, offset, remain_data);
    }
    //status = STATUS_EOF;
    //if(send_data(socket, &status, sizeof(status), 0, STATUS_EOF) < 0) {
    //    fprintf(stderr, "error occured with file transfer\n");
    //    return -1;
    //}
    RECV(socket, &status, sizeof(status), 0);
    status = ntohl(status);
    if(status != STATUS_OK) {
        fprintf(stderr, "'%s': some data was lost while transfering file\n", filename);
        return -1;
    }
    //status = STATUS_OK;
    //SEND(socket, &status, sizeof(status), 0);
    printf("Successfully sent file '%s'\n",filename);

    CLOSE(fd);
    return 0;
}

int receive_file(int socket)
{
    uint32_t status;
    size_t filename_size;
    // get filename_size
    RECV(socket, &filename_size, sizeof(filename_size), 0);
    filename_size = ntohl(filename_size);       //TODO probably dont need that conversion since it will be echoed back to server anyway
    printf("Received filename_size: %ld\n", filename_size);

    // echo it back

    filename_size = htonl(filename_size);
    SEND(socket, &filename_size, sizeof(filename_size), 0);
    printf("Send filename_size for approving\n");
    filename_size = ntohl(filename_size);   // convert back for memory allocations

    // get STATUS from server
    RECV(socket, &status, sizeof(status), 0);

    //printf("received status %d\n", status);
    status = ntohl(status);
    if(status != STATUS_OK) {
        fprintf(stderr, "Data was lost while transfering filename_size\n");
        return -1;
    }
    printf("filename_size approved\n");

    if(filename_size <= 0)
        return -1;

    // receive filename
    char* filename = malloc(filename_size);
    if(filename == NULL) {
        fprintf(stderr, "malloc failed\n");
        return -1;
    }
    RECV(socket, filename, filename_size, 0);

    printf("strlen: %ld, size: %lu\n", strlen(filename), filename_size);
    //if(strlen(filename) != filename_size -1 ) {     // FIXME issues here with receiveing or comparrison
    if(filename[filename_size - 1] != '\0' ) {     
        status = htonl((uint32_t)STATUS_DATA_LOST);
        SEND(socket, &status, sizeof(status), 0);
        fprintf(stderr, "Data was lost while transfering filename\n");
        return -1;
    }
    status = htonl((uint32_t)STATUS_OK);
    SEND(socket, &status, sizeof(status), 0);
    //printf("sending filename status: %d\n", status);
    printf("Received filename '%s'\n", filename);

    // receive filesize
    size_t filesize;
    RECV(socket, &filesize, sizeof(filesize), 0);
    filesize = ntohll(filesize);     // TODO maybe i dont need to convert this at that point since it will eventually be echoed back
    printf("Received filesize '%lu'\n", filesize);

    // echo it back
    filesize = htonll(filesize);     // conver to network format
    SEND(socket, &filesize, sizeof(filesize), 0);
    printf("Sent filesize for approving\n");
    filesize = ntohll(filesize);     // convert back for future allocations

    // get STATUS from server
    RECV(socket, &status, sizeof(status), 0);
    status = htonl(status);
    if(status != STATUS_OK) {
        fprintf(stderr, "Data was lost while transfering filesize\n");
        return -1;
    }
    printf("filesize approved\n");


    // receive file
    FILE* f = fopen(filename, "wb");
    char buff[BUFFER_SIZE];
    size_t received_bytes = 0;
    size_t remainig_bytes = filesize;
    unsigned int i = 0;
    while(remainig_bytes > 0) {
        ++i;
        ssize_t len = recv(socket, buff, sizeof(buff), 0);
        if(len == 0) {
            printf("'%s': EOF reached\n", filename);
            status = STATUS_EOF;
            break;
        }
        else if (len < 0) {
            fprintf(stderr, "'%s': recv(): %s\n", filename, strerror(errno));
            status = STATUS_ERROR;  // TODO probably dont need that
            exit(1);        // TODO make something smarter then just exiting from program
            break;      // TODO return on fail
        }
        printf("[%d] '%s': received %ld bytes, remaining: %ld bytes\n", i, filename, len, remainig_bytes); 
        fwrite(buff, len, sizeof(char), f);
        received_bytes += len;
        remainig_bytes -= len;
    }
    printf("finished receiving file '%s'\n", filename);
    fclose(f);

    if(received_bytes == filesize) {
        status = htonl((uint32_t)STATUS_OK);
        SEND(socket, &status, sizeof(status), 0);
    }
    else { 
        status = htonl((uint32_t)STATUS_DATA_LOST);
        SEND(socket, &status, sizeof(status), 0);
        return -1;
    }

    return 0;
}
