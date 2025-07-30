#include <stdio.h>
#include <string.h>


#include <errno.h>
#include <stdlib.h>
#include "utils.h"

//#define ADDR "127.0.0.1"
//#define PORT 8000
#define CONNECTIONS_LIMIT 1

//#define BUFFER_SIZE 1024 * 1024 
//#ifdef __linux__
//#define SAVE_FOLDER_PATH "/home/couldy/Documents/vm_to_host_server/transfer_folder"
//#elif _WIN32
//#define SAVE_FOLDER_PATH "C:\\Users\\qwe\\Desktop"
//#endif


#define BUFFER_SIZE 8192

typedef enum {
	SERVER,
	CLIENT

} MODE;


MODE mode;


#ifdef _WIN32
void server_windows(const char* addr, short port);
void client_windows(const char* addr, short port);

#else   // linuix and macOS
void server_test1_linux(const char* addr, short port);
void client_linux(const char* addr, short port);

#endif
char* get_str(const char* str);
char* get_filename(char* path);
void drag_and_drop_linux(char*** paths, int* path_count);
void drag_and_drop_windows(char*** paths, int* path_count);

void free_paths_array(char*** paths, int path_count);



int main(int argc, char** argv){
	printf("argc: %d\n", argc);
	if(argc == 2 && (strcmp(argv[1], "-h") == 0)){
        printf("Usage:\n ftransfer [MODE][IP][PORT]\n\n"
                " MODE : '-s' for sending files, '-c' for receiving files\n"
                " IP : ipv4 format (for '-s' it is 'IP' on which server will be started, for '-c' it is server 'IP' FROM which data will be received\n"
                " PORT: for '-s' it is PORT on which server will be started, for the '-c' it is server port FROM which data will be received\n\n"
                "Example of usage:\nSender: ftrasnfer -s 127.0.0.0 8000    - will start server on specified address and transfer files to connected clients\n\n"
                "Receiver: ftransfer -c 127.0.0.0 8000    - will connect to server with specified address and receive data\n");
        return 0;
	}
    if(argc != 4){
		printf("Invalid arguments. See ftransfer -h for help\n");
		return 0;
	}

    char* addr = argv[2];
    short port = (short)atoi(argv[3]);
	if(strcmp(argv[1], "-s") == 0){
		mode = SERVER;
		printf("Program set to a server mode\n");
		#ifdef _WIN32
			server_windows(addr, port);
		#else 
			server_test1_linux(addr, port);
		#endif
	}
	else if(strcmp(argv[1], "-c") == 0){
		mode = CLIENT;
		printf("Program set to a client mode\n");
		#ifdef _WIN32
			client_windows(addr, port);
		#else 
			client_linux(addr, port);
		#endif
	}


	return 0;
}

#ifndef _WIN32
void server_test1_linux(const char* addr, short port){
	//struct sockaddr_in      server_addr;
    //struct sockaddr_in      peer_addr;

    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if(server_socket == -1) {
        fprintf(stderr, "Creating socket failed: %s\n", strerror(errno));
        exit(1);
    }
	
	//memset(&server_addr, 0, sizeof(server_addr));

    // make socket reusable (do not put it in TIME WAIT state after closing socket)
    int enable = 1;
    if(setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)) != 0) {
        fprintf(stderr, "setsockopt(SOL_SOCKET) failed: %s\n", strerror(errno));
        exit(1);
    }

    struct sockaddr_in local;
    local.sin_family = AF_INET;
    local.sin_port = htons(port);

    int res = inet_pton(AF_INET, addr, &local.sin_addr);
    if(res == 0) {
        fprintf(stderr, "Address '%s' is invalid!\n", addr);
        exit(1);
    }
    else if(res == -1) {
        fprintf(stderr, "Converting address failed: %s\n", strerror(errno));
        exit(1);
    }

    if(bind(server_socket, (struct sockaddr*)&local, sizeof(local)) != 0) {
        fprintf(stderr, "Bind failed: %s\n", strerror(errno));
        exit(0);
    }

    if(listen(server_socket, CONNECTIONS_LIMIT) != 0) {
        fprintf(stderr, "listen() failed: %s\n", strerror(errno));
        exit(0);
    }

    printf("Server on '%s:%d' started\n", addr, port);

    struct sockaddr_in client;
    socklen_t client_len = sizeof(struct sockaddr_in);

    int peer_socket = accept(server_socket, (struct sockaddr*)&client, &client_len);
    if(peer_socket < 0) {
        fprintf(stderr, "accept() failed: %s\n", strerror(errno));
        exit(1);
    }
    else {
        char client_addr[INET_ADDRSTRLEN];
        if(inet_ntop(AF_INET, &(client.sin_addr), client_addr, sizeof(client_addr)) == NULL) {
            printf("inet_ntop() failed: %s\n", strerror(errno));
            exit(1);
        }
        printf("Accepted connection from '%s:%hu' \n", client_addr, ntohs(client.sin_port));
    }

	char** file_paths;
	int path_count;
	drag_and_drop_linux(&file_paths, &path_count);

	//printf("sending %d files\n", path_count);
	//char path_count_buff[128];
	//sprintf(path_count_buff, "%d", path_count);
	//send(peer_socket, path_count_buff, sizeof(path_count_buff), 0); //send how many files to transfer to client
	
	struct stat file_stat;
	off_t offset;
	int remain_data;
	int sent_bytes = 0;
	for(int i = 0; i < path_count; i++){
		char* filename = get_filename(file_paths[i]);
		printf("filename: %s\n", filename);
        if(send_file(peer_socket, file_paths[i], filename) < 0)
            fprintf(stderr, "Error while transfiring file '%s'\n", filename);
        else
            printf("File '%s' transfered succesfully\n", filename);

        free(filename);
    }
	free_paths_array(&file_paths, path_count);

	//send(client_fd, END_SIGNAL, sizeof(END_SIGNAL), 0);
	//printf("sent END_SIGNAL\n");
    // TODO WARNING Close it right way according to SO_LINGER article
	close(peer_socket);
	printf("Connection closed\n");

    close(server_socket);
}

void client_linux(const char* addr, short port){
	
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if(server_socket == -1) 
        ERR_EXIT("socket()");

    struct sockaddr_in remote;
    remote.sin_family = AF_INET;
    remote.sin_port = htons(port);

    int res = inet_pton(AF_INET, addr, &remote.sin_addr);
    if(res == 0) {
        fprintf(stderr, "Address '%s' is invalid!\n", addr);
        exit(1);
    }
    else if(res == -1) {
        fprintf(stderr, "Converting address failed: %s\n", strerror(errno));
        exit(1);
    }


    if(connect(server_socket, (struct sockaddr*)&remote, sizeof(remote)) != 0)
        ERR_EXIT("connect()");
    else
        printf("connected to '%s:%hu'\n", addr, port);
	

	for (;;) {
        if(receive_file(server_socket) != 0)
            fprintf(stderr, "Error occured while transfering file\n");
        else
            printf("file transfered successfully\n");
	}

    //TODO close socket right way according to SO_LINGER article
	close(server_socket);
}
#endif

#ifdef _WIN32
void server_windows(const char* addr, short port){
	struct sockaddr_in      server_addr;
	WSADATA wsa;
	SOCKET server_socket;

	printf("Initializing winsock\n");
	if(WSAStartup(MAKEWORD(2,2), &wsa) != 0){
        wsa_print_error("Failed tp initialize", WSAGetLastError());
		//printf("Failed. Error Code: %d\n", WSAGetLastError());
        exit(1);
	}
	printf("Initialized\n");

    server_socket = socket(AF_INET, SOCK_STREAM,0);
    if(server_socket == INVALID_SOCKET) {
        wsa_print_error("socket()", WSAGetLastError());
        exit(1);
    }
	
    // make socket reusable (do not put it in TIME WAIT state after closing socket)
    int enable = 1;
    if(setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, (char*)&enable, sizeof(enable)) != 0) {
        wsa_print_error("setsockopt(SOL_SOCKET) failed", WSAGetLastError());
        exit(1);
    }

	//memset(&server_addr, 0, sizeof(server_addr));

    struct sockaddr_in local;
	local.sin_family = AF_INET;
    local.sin_port = htons(port);

	int res = inet_pton(AF_INET, addr, &(local.sin_addr));
    if(res == 0) {
        fprintf(stderr, "Address '%s' is invalid!\n", addr);
        exit(1);
    }
    else if(res == -1) {
        wsa_print_error("converting address failed", WSAGetLastError());
        exit(1);
    }


    if (bind(server_socket, (struct sockaddr*)&local, sizeof(local)) != 0){
        wsa_print_error("bind()", WSAGetLastError());
        exit(1);
        return;
    }
    if(listen(server_socket, CONNECTIONS_LIMIT) != 0){
        wsa_print_error("listen()", WSAGetLastError());
        exit(1);
        return;
    }

    printf("Server started on '%s:%d'\n", addr, port);

	SOCKET peer_socket;
	int sock_len = sizeof(struct sockaddr_in);


    struct sockaddr_in      peer_addr;

    /* Accepting incoming peers */
    peer_socket = accept(server_socket, (struct sockaddr *)&peer_addr, &sock_len);
    if (peer_socket == INVALID_SOCKET) {
        wsa_print_error("accept()", WSAGetLastError());
		exit(1);
	}
    else {
        char client_addr[INET_ADDRSTRLEN];
        if(inet_ntop(AF_INET, &(peer_addr.sin_addr), client_addr, sizeof(client_addr)) == NULL) {
            wsa_print_error("inet_ntop()", WSAGetLastError());
            exit(1);
        }
        printf("Accepted connection from '%s:%hu'\n", client_addr, ntohs(peer_addr.sin_port));
    }


	char** file_paths;
	int path_count;
	drag_and_drop_windows(&file_paths, &path_count);
	
	struct stat file_stat;
	off_t offset;
	int remain_data;
	int sent_bytes = 0;
	for(int i = 0; i < path_count; i++){
		char* filename = get_filename(file_paths[i]);
		printf("filename: %s\n", filename);

        if(send_file(peer_socket, file_paths[i], filename) < 0)
            fprintf(stderr, "Error while transfiring file '%s'\n", filename);
        else
            printf("File '%s' transfered succesfully\n", filename);


		free(filename);
	}
	free_paths_array(&file_paths, path_count);

	//send(client_fd, END_SIGNAL, sizeof(END_SIGNAL), 0);
	//printf("sent END_SIGNAL\n");
	closesocket(peer_socket);
	printf("Connection closed\n");

	WSACleanup();
	closesocket(server_socket);
}
#endif

#ifdef _WIN32
void client_windows(const char* addr, short port){

	printf("Initializing winsock\n");
	WSADATA wsa;
	if(WSAStartup(MAKEWORD(2,2), &wsa) != 0){
        wsa_print_error("Initializing failed", WSAGetLastError());
        exit(1);
	}
	printf("Initialized\n");

	SOCKET server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if(server_socket == INVALID_SOCKET) {
        wsa_print_error("socket()", WSAGetLastError());
        exit(1);
    }

	struct sockaddr_in remote;
	remote.sin_family = AF_INET;
	remote.sin_port = htons(port);
	
	int res = inet_pton(AF_INET, addr, &(remote.sin_addr));  // for the VM with windows should be 10.0.2.2
    if(res == 0) {
        fprintf(stderr, "Address '%s' is invalid!\n", addr);
        exit(1);
    }
    else if(res == -1) {
        wsa_print_error("Converting address failed", WSAGetLastError());
        exit(1);
    }
                                                      

	if(connect(server_socket, (struct sockaddr *) &remote, sizeof(remote)) != 0){
        wsa_print_error("connect()", WSAGetLastError());
        exit(1);
	}
    else {
        printf("connected to '%s:%hu'\n", addr, port);
    }


    for(;;) {
        if(receive_file(server_socket) != 0)
            fprintf(stderr, "Error occured while transfering file\n");
        else
            printf("file transfered successfully\n");

	}

	WSACleanup();
	closesocket(server_socket);

}
#endif



char* get_filename(char* path){
    int pos = -1;
    int len = strlen(path);
    int new_size = 0;
	/*
	if(path[len-1] == '\n'){
        path[len-1] = '\0';		//if '\n' is stored in the end of str
								//test it later when uploading multiple paths
        len -=1 ;
    }*/
   	#ifdef __linux__
    char delimeter = '/';
	#elif _WIN32
	char delimeter = '\\';
	#endif
    for(int i = len; i >= 0; i--, new_size++){
        if(path[i] == delimeter){
            pos = i+1;
            break;
        }   
    }

    if(pos != -1){
        char* filename = (char*)malloc(sizeof(char) * (new_size+1));
        for(int i = 0; i < new_size; i++, pos++){
            filename[i] = path[pos]; 
        }
        filename[new_size] = '\0';
        return filename;

    }
    else
        return path;
} 

char* get_str(const char* str){
	int len = 0;
	for(; str[len] != '\0'; len++);
	
	char* newStr = (char*)malloc((len+1)*sizeof(char));
	for(int i = 0; i< len; i++)
		newStr[i] = str[i];
	newStr[len] = '\0';
	return newStr;
}


void drag_and_drop_linux(char ***paths, int *path_count) {
    char buff[4096];
	printf("Drag&Drop files here:\n");
    //scanf("%4095s",buff); //get filepaths user dropped
    fgets(buff, sizeof(buff), stdin);
    
	size_t len = strlen(buff);
    if (len > 0 && buff[strlen(buff) - 1] == '\n') {
        buff[len - 1] = '\0';
    }                        
    char *str = strdup(buff);
    
    const char *start = str;
    const char *end;
    *path_count = 0;  // Initialize path count

    *paths = NULL;  // Initialize the paths array to NULL

    while ((start = strchr(start, '\'')) != NULL) {
        start++;  // Move past the starting quote
        end = strchr(start, '\'');
        if (end) {
            // Calculate the length of the path
            int length = end - start;

            // Reallocate memory for the new path
            *paths = realloc(*paths, (*path_count + 1) * sizeof(char *));
            if (*paths == NULL) {
                perror("Failed to reallocate memory");
                exit(EXIT_FAILURE);
            }

            // Allocate memory for the new path and copy it
            (*paths)[*path_count] = malloc((length + 1) * sizeof(char));
            if ((*paths)[*path_count] == NULL) {
                perror("Failed to allocate memory");
                exit(EXIT_FAILURE);
            }

            strncpy((*paths)[*path_count], start, length);
            (*paths)[*path_count][length] = '\0';  // Null-terminate the string
            (*path_count)++;

            start = end + 1;  // Move past the ending quote
        } else {
            break;  // No closing quote found
        }
    }
    free(str);
}

void drag_and_drop_windows(char*** paths, int* path_count){         
    char buff[4096];                                                
	printf("Drag&Drop files here:\n");
    //scanf("%4095s",buff); //get filepaths user dropped            
    fgets(buff, sizeof(buff), stdin);
	//if(buff[strlen(buff) - 1] == '\n')
	//	printf("char is '\\n'");
	size_t len = strlen(buff);
    if (len > 0 && buff[strlen(buff) - 1] == '\n') {
        buff[len - 1] = '\0';
    }                        
   // printf("%s", buff);                                             
    char* str = strdup(buff);                                       
    char* theRest = str;                                            
    char* token = "\"";                                             
    // get number of files, create an array of paths and return it; 
                                                                    
    *path_count = 0;                                                
    char* path;         
     
    //printf("part: %s, iteration %d\n", path_size, *size);         
    while((path = strtok_r(theRest, token, &theRest)) != NULL){    
        if(*path_count < 1)
            *paths = (char**)malloc((*path_count+1) * sizeof(char*));
        else
            *paths = realloc(*paths, (*path_count+1) * sizeof(char *));
        (*paths)[*path_count] = strdup(path);
        *path_count += 1;                                           
        //path_size = strtok_r(buff, token, &buff);    
		printf("%s\n", path);             
    }                                                              
	free(str);
}



void free_paths_array(char*** paths, int path_count){
	for(int i = 0; i < path_count; i++){
		free((*paths)[i]);
	}
	free(*paths);
}


