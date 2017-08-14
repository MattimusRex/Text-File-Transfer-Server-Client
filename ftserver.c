/*
    Name:  Matthew Lay
    Date: 07/29/2017
    Description:  Server for file transfer assignment.  Usage: ./ftserver <port number>.
    Supported commands: -l for listing contents of directory
                        -g for getting a file
    Server listens on specified port until client connects.  When client gives -l or -g command,
    the server opens a new connection on the port specified by the client and sends the requested data.
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <dirent.h>

int PACKET_SIZE = 4096;

//sets up the server address struct
void setupServerAddress(struct sockaddr_in* serverAddress, int port) {
	memset((char*)serverAddress, '\0', sizeof(*serverAddress));
	serverAddress->sin_family = AF_INET;
	serverAddress->sin_port = htons(port);
	serverAddress->sin_addr.s_addr = INADDR_ANY;
}

//creates the listening socket for the server and begins listening
int setupServerSocket(int port) {
    //setup address
    struct sockaddr_in serverAddress;
    setupServerAddress(&serverAddress, port);

    //create socket
    int socketFD = socket(AF_INET, SOCK_STREAM, 0);
    if (socketFD < 0) {
        printf("Error opening socket\n");
        exit(-2);
    }
    
    //bind socket
    if (bind(socketFD, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) < 0) {
        printf("Error on binding socket\n");
        exit(-3);
    }

    //begin listening for incoming connections
    listen(socketFD, 5);

    return socketFD;
}

//sends message until all chars are sent.
void sendMessage(int socketFD, char outMsg[]){
    char message[PACKET_SIZE];
    memset(message, '\0', sizeof(message));
    strcpy(message, outMsg);
    int charsWritten = send(socketFD, message, PACKET_SIZE, 0);
    while (charsWritten < PACKET_SIZE) {
        //start from last spot not written and write remainder
        charsWritten += send(socketFD, &message[charsWritten], PACKET_SIZE - charsWritten, 0);
    }
}

//reads contents of file and sends it to server - taken from my CS 344 project 4
void transmitFile(FILE* file, int socketFD) {
	//read in from plaintext and replace added newline char
	char plainBuffer[PACKET_SIZE];
	memset(plainBuffer, '\0', sizeof(plainBuffer));

    //use fread so it will fill up the buffer instead of stopping at new line.  makes it faster for short line files
    int charsRead = fread(plainBuffer, 1, PACKET_SIZE - 1, file);
    //while it reads full buffer keep going
    while(charsRead == PACKET_SIZE - 1) {
        int charsWritten = send(socketFD, plainBuffer, PACKET_SIZE, 0);
		//if entire string not sent, keep sending from amount that did send
		while (charsWritten < PACKET_SIZE) {
			charsWritten += send(socketFD, &plainBuffer[charsWritten], PACKET_SIZE - charsWritten, 0);
		}
        memset(plainBuffer, '\0', sizeof(plainBuffer));	
        charsRead = fread(plainBuffer, 1, PACKET_SIZE - 1, file);
    }

    //after loop buffer contains the last read of fread.  replace last char with null terminator to eliminate extra newline
    plainBuffer[charsRead - 1] = '\0';
    int charsWritten = send(socketFD, plainBuffer, PACKET_SIZE, 0);
    //if entire string not sent, keep sending from amount that did send
    while (charsWritten < PACKET_SIZE) {
        charsWritten += send(socketFD, &plainBuffer[charsWritten], PACKET_SIZE - charsWritten, 0);
    }	

}

//receives message on socket
int receiveMessage(int socketFD, char message[], int messageSize) {
    int charsReceived = recv(socketFD, message, messageSize, 0);
    return charsReceived;
}

//creates dataSocket for server to transfer data to client
int createDataSocket(struct sockaddr_in clientAddress, int dataPort) {
    int dataSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (dataSocket < 0) {
        printf("Error opening data socket\n");
        exit(-2);
    }
    clientAddress.sin_port = htons(dataPort);
    if (connect(dataSocket, (struct sockaddr*)&clientAddress, sizeof(clientAddress)) < 0) {
        printf("connection on data socket failed\n");
        exit(-4);
    }

    return dataSocket;
}


//executes the -l command
//gets the directory contents and sends them to the correct data port at client
//connects to client on new data socket
void sendFileList(struct sockaddr_in clientAddress, int dataPort) {
    //create socket to data port and connect
    int dataSocket = createDataSocket(clientAddress, dataPort);

    //get directory contents - taken from cs 344
    DIR* dir = opendir(".");
    if (dir <= 0) {
        printf("error opening directory structure\n");
        close(dataSocket);
        exit(-5);
    }
    //transmit the name of each file in directory
    struct dirent *files;
    while ((files = readdir(dir)) != NULL) {
        sendMessage(dataSocket, files->d_name);
    }

    //done with socket, close it
    close(dataSocket);
}


//sends file to the socket and port number given
void sendFile(struct sockaddr_in clientAddress, int dataPort, int controlFD, char* filename) {
    //open file
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        //if no file found, send error message on control socket
        char* fileNotFoundMsg = "Requested file not found";
        printf("%s\n", fileNotFoundMsg);
        sendMessage(controlFD, fileNotFoundMsg);
    }
    else {
        //create socket to data port and connect
        int dataSocket = createDataSocket(clientAddress, dataPort);
        printf("Transmitting file %s\n", filename);
        transmitFile(file, dataSocket);
        fclose(file);
        close(dataSocket);
        printf("Transfer Complete\n");
    }
}


//runs the file transfer protocol. waits for and accepts -l and -g as commands.
//both commands cause the server to open a data connection to client
//-l lists contents of current directory
//-g <filename> transmits the requested file to the client.
void startFT(int controlFD, struct sockaddr_in clientAddress) {
    char* errorStr = "That command is not supported.  Supported commands are -l and -g <filename>";
    
    //wait for command
    char cmd[PACKET_SIZE];
    memset(cmd, '\0', sizeof(cmd));
    int charsReceived = receiveMessage(controlFD, cmd, sizeof(cmd));
    //0 chars received means socket closed by client
    if (charsReceived == 0) {
        close(controlFD);
    }
    else {
        //else the command is received, use strok to get the parts
        char* token = strtok(cmd, " ");
    
        //if -l is received
        if (strcmp(token, "-l") == 0) {
            //get data port
            token = strtok(NULL, " ");
            int dataPort = atoi(token);

            //execute command
            sendFileList(clientAddress, dataPort);
        }

        //if -g is received
        else if (strcmp(token, "-g") == 0) {
            //get data port and filename
            char* filename = strtok(NULL, " ");
            char* portStr = strtok(NULL, " ");
            int dataPort = atoi(portStr);

            //execute command
            sendFile(clientAddress, dataPort, controlFD, filename);
        }

        //else command is invalide, send error message
        else {
            printf("Unsupported command received\n");
            char errorMsg[PACKET_SIZE];
            memset(errorMsg, '\0', sizeof(errorMsg));
            strcpy(errorMsg, errorStr);
            sendMessage(controlFD, errorMsg);
        }
    }
}

//begins listening for connections on socketFD
void startListening(int socketFD) {
    //enter loop to accept connections
    while (1) {
        struct sockaddr_in clientAddress;
        socklen_t sizeOfClientInfo = sizeof(clientAddress);
        int controlFD = accept(socketFD, (struct sockaddr*)&clientAddress, &sizeOfClientInfo);      
        if (controlFD < 0) {
            printf("Error in server accepting connection\n");
        }
        else {
            printf("Client connected\n");
            startFT(controlFD, clientAddress);
        }
    }
}

int main(int argc, char* argv[]) {
    //check for correct usage
    if (argc != 2) {
        printf("Error: incorrect usage.  Usage is ./ftserver <port number>\n");
        exit(-1);
    }

    //print hostname for testing
    char hostname[512];
    memset(hostname, '\0', sizeof(hostname));
    gethostname(hostname, sizeof(hostname));
    printf("hostname: %s\n", hostname);

    //server setup taken from CS 344 project 4 and the corresponding lecture notes
    //get port number
    int listeningPort = atoi(argv[1]);
    int socketFD = setupServerSocket(listeningPort);
    //begin listening for connections
    printf("Server listening on port: %d\n", listeningPort);
    startListening(socketFD);
}