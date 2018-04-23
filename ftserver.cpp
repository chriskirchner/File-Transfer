/*
 * =====================================================================================
 *
 *       Filename:  ftserver.cpp
 *
 *    Description:  file transfer server serving directory and files to ftclient
 *
 *        Version:  1.0
 *        Created:  11/22/2016 10:44:34 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Christopher Kirchner
 *          Email:  kirchnch@oregonstate.edu
 *   Organization:  OSU 
 *
 * =====================================================================================
 */

#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <dirent.h>

int FILE_READ_SIZE = 1024*10;

//Citation - Beej's guide for c programming server
//Also used previous assignment's code

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  getAddr
 *  Description:  gets the address info for hostname and port
 * =====================================================================================
 */

struct addrinfo *getAddr(char *hostname, char *port){
    //setup hints to seed address info
    struct addrinfo hints, *ai;
    //clear hints memory space
    memset(&hints, 0, sizeof hints);
    //specify unspecified address family
    hints.ai_family = AF_UNSPEC;
    //specify byte stream socket
    hints.ai_socktype = SOCK_STREAM;
    //use host's network address
    hints.ai_flags = AI_PASSIVE;

    int status;
    if ((status = getaddrinfo(hostname, port, &hints, &ai)) != 0){
        fprintf(stderr, "Failed to resolve host address: %s\n",
                gai_strerror(status));
        exit(1);
    }
    return ai;
}

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  getPeerAddr
 *  Description:  gets perr address for hostname and port
 * =====================================================================================
 */

struct addrinfo *getPeerAddr(char *hostname, char *port){
    //setup hints to seed address info
    struct addrinfo hints, *ai;
    //clear hints memory space
    memset(&hints, 0, sizeof hints);
    //specify unspecified address family
    hints.ai_family = AF_UNSPEC;
    //specify byte stream socket
    hints.ai_socktype = SOCK_STREAM;
    //use host's network address
    hints.ai_flags = AI_PASSIVE;

    int status;
    if ((status = getaddrinfo(hostname, port, &hints, &ai)) != 0){
        fprintf(stderr, "Failed to resolve host address: %s\n",
                gai_strerror(status));
        exit(1);
    }
    return ai;
}

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  getBoundSocket
 *  Description:  gets socket bound to port in addrinfo 
 * =====================================================================================
 */

int getBoundSocket(struct addrinfo *ai){
    int sock = 0;
    //similar to Beej's for loop
    //find socket that binds
    struct addrinfo *tmp_ai = ai;
    int yes = 1;
	//find bindable sock in address info
    while (tmp_ai != 0 && sock == 0){
        if ((sock = socket(tmp_ai->ai_family,
                           tmp_ai->ai_socktype,
                           tmp_ai->ai_protocol)) == -1){
            perror("Failed to get socket");
        }
		//set socket to reuse
        if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR,
                       &yes, sizeof(int)) == -1){
            perror("Failed to set socket options");
        }
		//try to bind
        if ((bind(sock, tmp_ai->ai_addr, tmp_ai->ai_addrlen)) == -1){
            perror("Failed to bind socket");
            close(sock);
            sock = 0;
        }
        tmp_ai = tmp_ai->ai_next;
    }

	//free linked list
    freeaddrinfo(ai);

	//give up on life
    if (sock == 0){
        exit(1);
    }

    return sock;
}

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  getConnectedSocket
 *  Description:  gets socket connected to address in address info
 * =====================================================================================
 */

int getConnectedSocket(struct addrinfo *serv_info){
    struct addrinfo *ai = serv_info;
    int sock = 0;
    //iterate through addrinfo linked list
    while (ai != NULL && sock == 0){
        //get socket from addrinfo
        if ((sock = socket(serv_info->ai_family,
                           serv_info->ai_socktype,
                           serv_info->ai_protocol)) == -1){
            perror("data: failed to get socket");
        }

        //try to connect to socket
        if (connect(sock,
                    serv_info->ai_addr,
                    serv_info->ai_addrlen) == -1){

            close(sock);
            sock = 0;
            perror("data: failed to connect socket");
        }
        ai = ai->ai_next;
    }

    freeaddrinfo(serv_info);
    return sock;
}

/*
 * ===  FUNCTION  ======================================================================
 *         Name:  sendNum
 *  Description:  sends uint64_t number through socket
 * =====================================================================================
 */

ssize_t sendNum(uint64_t num, int sock){
    ssize_t sn;
    //convert to network big-endian
	//big big number
    num = htobe64(num);
    if ((sn = send(sock, &num, sizeof num, 0)) == -1){
        perror("Failed to send value");
    }
        //check if proper bytes count sent
    else if (sn != sizeof num){
        perror("Failed to send correct value");
    }
    return sn;
}

/*
 * ===  FUNCTION  ======================================================================
 *         Name:  sendMsg
 *  Description:  sends char message through socket
 * =====================================================================================
 */

int sendMsg(char *msg, int sock){
    uint64_t msgSize = strlen(msg);
    ssize_t sn;

    //communicate msg length to host
    if ((sn = sendNum(msgSize, sock)) == -1){
        return -1;
    }

    //send message through socket in chunks
    int sent = 0;
    while (sent < msgSize){
        sn = send(sock, msg+sent, msgSize-sent, 0);
        if (sn == -1){
            return -1;
        }
        sent += (int) sn;
    }

    return 1;
}


/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  sendFile
 *  Description:  sends file through socket
 * =====================================================================================
 */

int sendFile(FILE *file, int sock){
    //citation - http://stackoverflow.com/questions/238603/how-can-i-get-a-files-size-in-c
	//get the file size and send to client
    fseek(file, 0L, SEEK_END);
    uint64_t fileSize = (uint64_t) ftell(file);
    rewind(file);
    ssize_t sn;

    //communicate msg length to host
    if ((sn = sendNum(fileSize, sock)) == -1){
        return -1;
    }

    //read file FILE_READ_SIZE at a time
    char *buffer[FILE_READ_SIZE+1];
    size_t read = 0;
    size_t rn = 0;
    while (read < fileSize){
        memset(buffer, 0, sizeof buffer);
        rn = fread(buffer, 1, FILE_READ_SIZE+1, file);

        //send message through socket in chunks
        size_t sent = 0;
        while (sent < rn){
            sn = send(sock, buffer+sent, rn-sent, 0);
            if (sn == -1){
                return -1;
            }
            sent += sn;
        }

        read += rn;
    }
    return 1;
}

/*
 * ===  FUNCTION  ======================================================================
 *         Name:  getNum
 *  Description:  returns received uint64_t value from socket
 * =====================================================================================
 */

uint64_t getNum(int sock){
    ssize_t rn;
    uint64_t msgSize;
    if ((rn = read(sock, &msgSize, sizeof msgSize)) == -1){
        perror("Failed to get size of msg");
        return -1;
    }
    //return host converted value
    return be64toh(msgSize);
}


/*
 * ===  FUNCTION  ======================================================================
 *         Name:  getMsg
 *  Description:  returns received char message from socket
 * =====================================================================================
 */

char *getMsg(int sock){
    ssize_t rn;
    //get message size from host
    uint64_t msgSize = getNum(sock);

    //receive message from socket in chunks
    int received = 0;
    char *msg = (char*) malloc(msgSize+1);
    //make sure message is clean with nulls
    memset(msg, 0, msgSize+1);
    while (received < msgSize){
        if ((rn = read(sock, msg+received, msgSize-received)) == -1){
            perror("Failed to receive msg");
            return NULL;
        }
        received += rn;
    }

    return msg;
}

//Beej's function
//didn't get it at until it was typed in
//returns the IPV4 or IPV6 address given the protocol independent sockaddr struct
void *get_in_addr(struct sockaddr *sa){
    if (sa->sa_family == AF_INET){
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    else if (sa->sa_family == AF_INET6){
        return &(((struct sockaddr_in6*)sa)->sin6_addr);
    }
}

//citation - http://stackoverflow.com/questions/612097/how-can-i-get-the-list-of-files-in-a-directory-using-c-or-c

/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  getDirList
 *  Description:  returns directory contents
 * =====================================================================================
 */

char *getDirList(char *directory){
    char *dirList = 0;
	//setup directory pointer
    DIR *dirP;
	//setup directory entry pointer
    struct dirent *entryP;
	//open directory
    if ((dirP = opendir(directory)) != NULL){
		//list directory entries
        while((entryP = readdir(dirP)) != NULL){
			//add first entry
            if (dirList == 0){
                dirList = (char*) malloc(sizeof entryP->d_name);
                strcpy(dirList, entryP->d_name);
            }
			//add more entries by reallocation
            else {
                dirList = (char *) realloc(dirList, sizeof entryP->d_name);
                strcat(dirList, entryP->d_name);
            }
            strcat(dirList, "\n");
        }
    }
    else {
        perror("Failed to open directory");
    }
    return dirList;
}


/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  handleRequest
 *  Description:  handles data requests from ctrl socket
 * =====================================================================================
 */

void handleRequest(int ctrl_sock, char *host, char *port, char *command, char *filename){

    struct addrinfo *ai = getAddr(host, port);
    int data_sock = getConnectedSocket(ai);

	//sends directory list to client
    if (strcmp(command, "LIST") == 0){
		//echoes clients command
        sendMsg("LIST_REPLY", ctrl_sock);
        printf("ftserver: list directory requested on port %s\n", port);
        fflush(stdout);
        //citation - http://stackoverflow.com/questions/298510/how-to-get-the-current-directory-in-a-c-program
        char currentDir[PATH_MAX];
        getcwd(currentDir, sizeof currentDir);
		//get directory contents
        char *dirList = getDirList(currentDir);
		//send directory
        sendMsg(dirList, data_sock);
        free(dirList);
    }
	//handle file request
    else if (strcmp(command, "GET") == 0){
		//send file reply
        sendMsg("GET_REPLY", ctrl_sock);
        printf("File \"%s\" requested on port %s\n", filename, port);
        fflush(stdout);
        FILE *file = fopen(filename, "rb");
		//handle 'file not found' error
        if (file == 0){
            sendMsg("FAIL", ctrl_sock);
        }
		//send file
        else if (sendMsg("OK", ctrl_sock) && sendFile(file, data_sock) == -1){
            fprintf(stderr, "Failed to send file\n");
        }
        fclose(file);
    }
	//handle "unknown command" error (even though client validates commands)
    else {
        sendMsg("UNK_CMD", ctrl_sock);
    }
    close(data_sock);
    exit(0);
}


/* 
 * ===  FUNCTION  ======================================================================
 *         Name:  FTLoop
 *  Description:  loops over server socket to handle requests
 * =====================================================================================
 */

void FTLoop(int ctrl_server){
    //setup socket address for both IPV4 and IPV6
    //as used in Beej's Guide
    struct sockaddr_storage cli_ss;

  	//loop	
	while (1){
        int ctrl_sock = 0;
        socklen_t addr_len = sizeof(cli_ss);
        if ((ctrl_sock = accept(ctrl_server,
                   (struct sockaddr*) &cli_ss,
                   &addr_len)) == -1){
            perror("Failed to accept connection");
            continue;
        }

        char cli_addr_st[INET6_ADDRSTRLEN];

        //used Beej's get_in_addr function
		//convert network to presentatin string
        inet_ntop(cli_ss.ss_family,
                  get_in_addr((struct sockaddr*) &cli_ss),
                  cli_addr_st, sizeof(cli_addr_st));

   		//retrieve peer information     
		char host[HOST_NAME_MAX];
        char service[20];
        getnameinfo((sockaddr*) &cli_ss, sizeof cli_ss,
                    host, sizeof host,
                    service, sizeof service, 0);
        printf("Connection from %s\n", host);
        fflush(stdout);
		//get client data port
        char *port = getMsg(ctrl_sock);
		//get client command
        char *command = getMsg(ctrl_sock);
		//get requested filename
        char *filename = 0;
        if (strcmp(command, "GET") == 0){
            filename = getMsg(ctrl_sock);
        }

        //forking structure similar to man pages for fork()
		//create process to handle client request to allow other client connections
        int pid;
        if ((pid = fork()) == 0){
            close(ctrl_server);
            handleRequest(ctrl_sock, host, port, command, filename);
            close(ctrl_sock);
        }
        close(ctrl_sock);
    }
}


int main(int argc, char *argv[]) {

	//validate port argument
    char *port = argv[1];
    if (atoi(port) < 1024){
        fprintf(stderr, "ftserver: error: reserved ports disallowed");
    }
    else if (atoi(port) > 65535){
        fprintf(stderr, "ftserver: error: out-of-bounds port provided");
    }

	//get host address info and bind to it
    struct addrinfo *ai = getAddr(0, port);
    int ctrl_server = getBoundSocket(ai);

    if (listen(ctrl_server, 1) == -1){
        perror("Failed to listen to socket");
        exit(1);
    }

    printf("ftserver: open on port %d\n", atoi(port));
    fflush(stdout);
    FTLoop(ctrl_server);

    return 0;
}
