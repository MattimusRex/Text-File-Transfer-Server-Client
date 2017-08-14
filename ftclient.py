#!/usr/bin/python3
#Name:  Matthew Lay
#Date: 07/29/2017
#Description:  Client for file transfer assignment.  Usage: ./ftclient <server host> <server port> <command> <optional filename> <data port>.
#Supported commands: -l for listing contents of directory
#                    -g <filename> for getting a file

import sys
import socket
import select
import os.path

#setting up and using the socket taken from https://shakeelosmani.wordpress.com/2015/04/13/python-3-socket-programming-example/ - also showed use of encode and decode
#sets up the control socket for the client so the client can connect to server
def setupClientSocket(host, port):
    socketFD = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    socketFD.connect((host, port))
    return socketFD

#sets up the data socket for the client so the client can accept the request from the server and receive the data
def setupServerSocket(port):
    host = socket.gethostbyname(socket.gethostname())
    socketFD = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    socketFD.bind((host, int(port)))
    socketFD.listen(5)
    print("data port opened on port " + port)
    return socketFD

#sends the request to the server
def sendRequest(socketFD, cmd, port=None, filename=None):
    if (port == None):
        socketFD.sendall((cmd).encode())
    elif (filename == None):
        socketFD.sendall((cmd + " " + port).encode())
    else:
        socketFD.sendall((cmd + " " + filename + " " + port).encode())

#asks user if they want to overwrite duplicate file and handles response
def handleDuplicate(socketFD):
    result = None
    #loop until response to query
    while (result != "y" and result != "n"):
        result = input("A file with that name already exists.  Do you want to overwrite the old file? Please enter y or n. ")
        #dont overwrite, can only exit - be sure to close the opened socket!
        if (result == "n"):
            print("Exiting program")
            socketFD.close()
            sys.exit(0)
        elif (result != "y"):
            print("That response is not valid.  Please enter y or n.")

#processes the command line arguments and sets up data socket for response from server
def proccessCommands(socketFD):
    #get command
    cmd = sys.argv[3]
    #get dataPort
    if (len(sys.argv) == 5): 
        dataPort = sys.argv[4]
    else:
        dataPort = sys.argv[5]
    
    #check command value
    if (cmd == "-g"):
        filename = sys.argv[4]
        #see if there is a file with that name already - taken from https://stackoverflow.com/questions/82831/how-do-i-check-whether-a-file-exists-using-python
        if (os.path.isfile(filename)):
            #handleDuplicate will exit program if user doesnt want to overwrite file
            handleDuplicate(socketFD)
        #if program doesnt exit in handleDuplicate, can overwrite file
        inFile = open(filename, "w")
        dataSocket = setupServerSocket(dataPort)
        sendRequest(socketFD, cmd, dataPort, filename)

    elif (cmd == '-l'):
        inFile = []
        dataSocket = setupServerSocket(dataPort)
        sendRequest(socketFD, cmd, dataPort)

    #command invalid but have to send anyway according to assingment specs
    else: 
        sendRequest(socketFD, cmd)
        dataSocket = None
        inFile = []
    
    #create socket to listen to for response
    return dataSocket, cmd, inFile

#wait for response on both sockets - select use taken from https://docs.python.org/3/howto/sockets.html and https://stackoverflow.com/questions/5308080/python-socket-accept-nonblocking
def receiveResponse(controlSocket, dataSocket, inFile):
    #create list of sockets we want to read from
    if (dataSocket == None):
        potentialReadableSockets = [controlSocket]
    else:
        potentialReadableSockets = [controlSocket, dataSocket]
    #start listening to them
    readable, writeable, errored = select.select(potentialReadableSockets, [], [], 60)
    #if there is a readable socket, listen to it
    while (readable):
        for mySocket in readable:
            #if its data socket, we need to accept the connection from server
            if (mySocket is dataSocket):
                incomingSocket, address = dataSocket.accept()
                potentialReadableSockets.append(incomingSocket)
            #otherwise its info on the newly created data connection or error on control socket
            else:
                incMsg = mySocket.recv(4096).decode()
                #length of zero means socket has been closed.  close on this end and remove from list
                if (len(incMsg) == 0):
                    mySocket.close()
                    potentialReadableSockets.remove(mySocket)
                    continue
                while (len(incMsg) < 4096):
                    tempMsg = mySocket.recv(4096 - len(incMsg)).decode()
                    incMsg += tempMsg
                #if control socket, then error
                if (mySocket is controlSocket):
                    print(incMsg)

                #write to file if supposed
                elif (cmd == "-g"):
                    #split to get rid of null terminator padding
                    words = incMsg.split('\0')
                    inFile.write(words[0])
                
                #otherwise its -l, so print
                else:
                    print(incMsg)
        #listen for more
        readable, writeable, errored = select.select(potentialReadableSockets, [], [], 10)
    
    #done receiving froms server, close sockets
    if (sys.argv[3] == '-g'):
        print("Transfer Complete")
    for mySocket in potentialReadableSockets:
        mySocket.close()


#main method
if __name__ == "__main__":
    #check usage
    if (len(sys.argv) != 5 and len(sys.argv) != 6):
        print("Incorrect usage. Usage: python3 ftclient.py <server host> <server port> <command> <optional filename> <data port>")
        sys.exit()
    
    #get hostname and port from command line args
    serverHostname = sys.argv[1]
    serverPort = int(sys.argv[2])
    #create and connect to server on control socket
    controlSocket = setupClientSocket(serverHostname, serverPort)
    #get remaining arguments and send command
    #also create data socket
    dataSocket, cmd, inFile = proccessCommands(controlSocket)
    
    #wait for response
    receiveResponse(controlSocket, dataSocket, inFile)
    
    #finished with response - close file
    if (inFile):
        inFile.write("\n")
        inFile.close()


    


    





    
