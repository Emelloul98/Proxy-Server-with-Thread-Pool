#include <stdio.h>
#include<stdlib.h>
#include<string.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <netdb.h>
#include <fcntl.h>
#include "threadpool.h"
#define CHUNK_SIZE 3000
#define BAD_REQUEST 400
#define FORBIDDEN 403
#define NOT_FOUND 404
#define INTERNAL_SERVER_ERROR 500
#define NOT_SUPPORTED 501
#define MAX_PORT_SIZE 65535
void readFromClient(int,char*);
void handelRequest(char*,char*,int);
void errorHandling(int,int);
char* timeClock();
int isInFilter(char*,char*,char*);
int numOrName(const char*);
char* fromHostToIP(char*,int);
char* fromNumberToBinary(int);
char* parseIp(char*);
int connectToRealServer(char*,int);
int writeToSocket(char*,int);
int readFromServersSocket(int,int);
int writeToClientSocket(unsigned char*,int,ssize_t);
int dispatchTask(void*);
char* addHeaderConnectionClosed(char*);
char* readFileToString(FILE*);
// Structure to hold task arguments
typedef struct TaskArgs
{
    int clientSocket;
    char *filter;
}TaskArgs;
int main(int argc,char* argv[])
{
    //Input check:
    if( argc!=5 )
    {
        printf("Usage: proxyServer <port> <pool-size> <max-number-of-request> <filter>\n");
        exit(EXIT_FAILURE);
    }
    char *endPtr;
    long long port,poolSize,maxRequests;
    FILE* filterFile;
    errno=0;
    port=strtoll(argv[1], &endPtr, 10);
    if(errno != 0 || endPtr == argv[0])
    {
        printf("error: strtoll\n");
        exit(EXIT_FAILURE);
    }
    errno=0;
    poolSize=strtoll(argv[2], &endPtr, 10);
    if(errno != 0 || endPtr == argv[1])
    {
        printf("error: strtoll\n");
        exit(EXIT_FAILURE);
    }
    maxRequests=strtoll(argv[3], &endPtr, 10);
    if(errno != 0 || endPtr == argv[2])
    {
        printf("error: strtoll\n");
        exit(EXIT_FAILURE);
    }
    //checks for incorrect input from the user:
    if(port<0 || port>MAX_PORT_SIZE || poolSize<1 || maxRequests<1)
    {
        printf("Usage: proxyServer <port> <pool-size> <max-number-of-request> <filter>\n");
        exit(EXIT_FAILURE);
    }
    // Creating a threadPool:
    threadpool* myPool;
    myPool=create_threadpool((int)poolSize);
    char* filterFileName= strdup(argv[4]);
    if(filterFileName==NULL)
    {
        destroy_threadpool(myPool);
        exit(EXIT_FAILURE);
    }
    filterFile=fopen(filterFileName,"r");
    if(filterFile==NULL)
    {
        free(filterFileName);
        destroy_threadpool(myPool);
        printf("error: open filter file\n");
        exit(EXIT_FAILURE);
    }
    free(filterFileName);
    // Read filter file to a string:
    char* filter= readFileToString(filterFile);
    if(filter==NULL)
    {
        destroy_threadpool(myPool);
        exit(EXIT_FAILURE);
    }
    int welcome_socket,numOfRequests=0;
    struct sockaddr_in server;
    welcome_socket=socket(PF_INET, SOCK_STREAM, 0);
    if(welcome_socket < 0)
    {
        free(filter);
        destroy_threadpool(myPool);
        perror("error: socket\n");
        exit(EXIT_FAILURE);
    }
    server.sin_family=AF_INET;
    server.sin_port= htons(port);
    server.sin_addr.s_addr=htonl(INADDR_ANY);
    // Proxy server bind:
    int bindRes=bind(welcome_socket, (struct sockaddr*)&server, sizeof(struct sockaddr_in));
    if(bindRes < 0)
    {
        free(filter);
        destroy_threadpool(myPool);
        perror("error: bind\n");
        exit(EXIT_FAILURE);
    }
    //proxy server listen:
    int listenRes=listen(welcome_socket, 5);
    if(listenRes<0)
    {
        free(filter);
        destroy_threadpool(myPool);
        perror("error: listen\n");
        exit(EXIT_FAILURE);
    }
    //proxy server accept:
    while(numOfRequests<maxRequests)
    {
        // Creating a dynamic memory for each thread that will do a work
        TaskArgs* taskStruct=(TaskArgs*)malloc(sizeof(TaskArgs));
        if(taskStruct==NULL)
        {
            free(filter);
            destroy_threadpool(myPool);
            printf("error: malloc\n");
            exit(EXIT_FAILURE);
        }
        taskStruct->filter= strdup(filter);
        // Waiting for clients to connect:
        taskStruct->clientSocket= accept(welcome_socket,NULL,NULL);
        if (taskStruct->clientSocket< 0)
        {
            free(filter);
            free(taskStruct);
            destroy_threadpool(myPool);
            perror("error: accept\n");
            exit(EXIT_FAILURE);
        }
        numOfRequests++;
        // call read with threadPool threads here:
        dispatch(myPool,(dispatch_fn)dispatchTask,(void*)taskStruct);
    }
    // When all jobs finished
    destroy_threadpool(myPool);
    free(filter);
    close(welcome_socket);
}
// A function that gets a filter file and return his content in a char*
char* readFileToString(FILE* file)
{
    // Determine file size
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    rewind(file);
    // Allocate memory for the buffer
    char* buffer = (char*)malloc(file_size + 1); // +1 for null terminator
    if (buffer == NULL)
    {
        fclose(file);
        return NULL;
    }
    // Read file contents into buffer
    size_t result = fread(buffer, 1, file_size, file);
    if (result != file_size)
    {
        free(buffer);
        fclose(file);
        return NULL;
    }
    // Null-terminate the buffer
    buffer[file_size] = '\0';
    fclose(file);
    return buffer;
}
// The function that the thread will do:
int dispatchTask(void *arg)
{
    // Retrieve task arguments
    TaskArgs *args = (TaskArgs*)arg;
    int clientSocket = args->clientSocket;
    char *filter = args->filter;
    readFromClient(clientSocket,filter);
    free(args->filter);
    free(args);
    return 0;
}
// A function that gets a socket and reading the client request:
void readFromClient(int clientSocket,char* filter)
{
    long bytesNumber;
    int fullBytesNum=0,fullMallocedNum=CHUNK_SIZE,counter=1;
    char* buffer=(char*)malloc(CHUNK_SIZE+1);
    memset(buffer,0,CHUNK_SIZE+1);
    char* endPtr;
    while(1)
    {
        int numToRead=fullMallocedNum-fullBytesNum;
        bytesNumber = read(clientSocket, buffer + fullBytesNum, numToRead);
        if (bytesNumber < 0)
        {
            free(buffer);
            errorHandling(INTERNAL_SERVER_ERROR,clientSocket);
            return;
        }
        else if (bytesNumber == 0)
        {
            return; // the client closed the connection
        }
        fullBytesNum+=(int)bytesNumber;
        buffer[fullBytesNum]='\0';
        endPtr = strstr(buffer, "\r\n\r\n");
        if (endPtr != NULL)
        {
            int endIndex = (int)(endPtr - buffer + 4);
            buffer[endIndex] = '\0';
            break;
        }
        if(fullBytesNum==fullMallocedNum)
        {
            counter++;
            char *tempBuffer = realloc(buffer, sizeof(char) * (CHUNK_SIZE *counter)+1);
            if (tempBuffer == NULL) {
                free(buffer);
                errorHandling(INTERNAL_SERVER_ERROR, clientSocket);
                return;
            } else {
                fullMallocedNum+=CHUNK_SIZE;
                buffer = tempBuffer;
            }
        }
    }
    handelRequest(buffer,filter,clientSocket);
    close(clientSocket);
}
// A function that gets a request,checks it,and calls the next functions:
void handelRequest(char* request,char* filter,int clientSocket)//need to fix the 400 bad request
{
    int portNum=80;
    char *host,*hostPtr,*portPtr;
    int reqLen=(int)strlen(request);
    char requestCopy1[reqLen+1],requestCopy2[reqLen+1];
    memset(requestCopy1,0,reqLen+1);
    memset(requestCopy2,0,reqLen+1);
    strcpy(requestCopy1,request);
    strcpy(requestCopy2,request);
    char* firstLine=strtok(requestCopy1, "\r\n");
    int firstLen=(int)strlen(firstLine);
    char method[firstLen],path[firstLen],version[firstLen];
    memset(method,0,firstLen);
    memset(path,0,firstLen);
    memset(version,0,firstLen);
    // first line error handling:
    int num_matched=sscanf(firstLine,"%s %s %s\r\n",method,path,version);
    if(num_matched!=3)
    {
        errorHandling(BAD_REQUEST,clientSocket);
        free(request);
        return;
    }
    int method_len = (int)strcspn(firstLine, " ");   // Find length of method
    int path_len = (int)strcspn(firstLine + method_len + 2, " ");   // Find length of path
    int version_len = (int)strcspn(firstLine + method_len + path_len + 3, " \r\n");   // Find length of version
    method[method_len] = '\0';
    path[path_len] = '\0';
    version[version_len] = '\0';
    //checks if the request is http 1.0 or 1.1:
    if((strcmp(version,"HTTP/1.0")!=0 && strcmp(version,"HTTP/1.1")!=0))
    {
        free(request);
        errorHandling(BAD_REQUEST,clientSocket);
        return;
    }
    //if there is no host header:
    hostPtr= strstr(requestCopy2, "Host:");
    if(hostPtr==NULL)
    {
        errorHandling(BAD_REQUEST,clientSocket);
        free(request);
        return;
    }
    hostPtr+=6;//skips the host: and the space after it
    char*temp1= strtok(hostPtr,"\r\n");
    portPtr=strstr(temp1,":");
    if(portPtr!=NULL)// if there is a port number:
    {
        host= strtok(temp1,":");
        portPtr+=1;
        portNum= atoi(strtok(portPtr,"\n"));
    }
    else // when there is no port number
    {
        host= strtok(temp1,"\n");
    }
    // method not get error handling:
    if(strncmp(method,"GET",3)!=0)
    {
        errorHandling(NOT_SUPPORTED,clientSocket);
        free(request);
        return;
    }
    // Calls a function that converts the host name to an IP number
    char* hostIp=fromHostToIP(host,clientSocket);
    if(hostIp==NULL)
    {
        free(request);
        return;
    }
    // calls a function
    int res=isInFilter(host,hostIp,filter);
    if(res!=0)
    {
        if(res==1) errorHandling(FORBIDDEN,clientSocket);
        else errorHandling(INTERNAL_SERVER_ERROR,clientSocket);//when res=-1
        free(request);
        return;
    }
    //the request is legal:
    int proxyToServerSocket=connectToRealServer(host,portNum);
    if(proxyToServerSocket==-1)
    {
        errorHandling(INTERNAL_SERVER_ERROR,clientSocket);
        free(request);
        return;
    }
    //adding the connection: close header:
    char* newRequest= addHeaderConnectionClosed(request);
    if(newRequest==NULL)
    {
        errorHandling(INTERNAL_SERVER_ERROR,clientSocket);
        free(request);
        return;
    }
    free(request);
    int writeRes=writeToSocket(newRequest,proxyToServerSocket);
    if(writeRes==-1)
    {
        errorHandling(INTERNAL_SERVER_ERROR,clientSocket);
        free(newRequest);
        return;
    }
    free(newRequest);
    int readRes=readFromServersSocket(proxyToServerSocket,clientSocket);
    if(readRes==-1)
    {
        errorHandling(INTERNAL_SERVER_ERROR,clientSocket);
        return;
    }
}
// A function that gets an err number and write the error to client socket
void errorHandling(int errNum,int proxyToClientSocket)
{
    char* time=timeClock();
    int size1=25,size2=500;
    char description[size1],bodyDescription[size1],body[size2];
    memset(description,0,size1);
    memset(bodyDescription,0,size1);
    memset(body,0,size2);
    switch (errNum)
    {
        case BAD_REQUEST:
        {
            strcpy(description,"400 Bad Request");
            strcpy(bodyDescription,"Bad Request.");
            break;
        }
        case FORBIDDEN:
        {
            strcpy(description,"403 Forbidden");
            strcpy(bodyDescription,"Access denied.");
            break;
        }
        case NOT_FOUND:
        {
            strcpy(description,"404 Not Found");
            strcpy(bodyDescription,"File not found.");
            break;
        }
        case INTERNAL_SERVER_ERROR:
        {
            strcpy(description,"500 Internal Server Error");
            strcpy(bodyDescription,"Some server side error.");
            break;
        }
        case NOT_SUPPORTED:
        {
            strcpy(description,"501 Not supported");
            strcpy(bodyDescription,"Method is not supported.");
            break;
        }
        default: //cant arrive here,im the one how is sending the number.
        {
            free(time);
            return;
        }
    }
    sprintf(body,"<HTML><HEAD><TITLE>%s</TITLE></HEAD>\r\n<BODY><H4>%s</H4>\r\n%s\r\n</BODY></HTML>",description,description,bodyDescription);
    char* fullResponse=(char*)malloc(size2*2);
    memset(fullResponse,0,size2*2);
    sprintf(fullResponse,"HTTP/1.1 %s\r\nServer: webserver/1.0\r\n%s\r\nContent-Type: text/html\r\nContent-Length: %d\r\nConnection: close\r\n\r\n%s",description,time,(int)strlen(body),body);
    writeToSocket(fullResponse,proxyToClientSocket);//to write char and not char* we will use the writeToServerSocket
    free(fullResponse);
    free(time);
}
char* timeClock() // A function that return a dynamic data that needs to be free
{
    time_t rawtime;
    struct tm *timeinfo;
    const char *weekdays[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    const char *months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    time(&rawtime);
    timeinfo = gmtime(&rawtime);
    // Adjusting time for GMT clock
    timeinfo->tm_hour -= 3;
    timeinfo->tm_min -= 36;
    // Convert negative minutes to hours
    if (timeinfo->tm_min < 0) {
        timeinfo->tm_min += 60;
        timeinfo->tm_hour--;
    }
    // Convert negative hours to 24-hour clock
    if (timeinfo->tm_hour < 0) {
        timeinfo->tm_hour += 24;
    }
    int max_length = 50 + (int)strlen(weekdays[timeinfo->tm_wday]) + (int)strlen(months[timeinfo->tm_mon]);
    char *result = (char *)malloc(max_length * sizeof(char));
    sprintf(result, "Date: %s, %02d %s %d %02d:%02d:%02d GMT",
            weekdays[timeinfo->tm_wday], timeinfo->tm_mday, months[timeinfo->tm_mon],
            1900 + timeinfo->tm_year, timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
    return result;
}
// A function that
int isInFilter(char* host, char* hostIp, char* filter)
{
    int len=(int)strlen(filter)+1;
    char filterCopy[len];
    strcpy(filterCopy,filter);
    char* line = strtok(filter, "\n"); // Tokenize the string by lines
    int subnetSize;
    char* hostInBinary = parseIp(hostIp);
    if (hostInBinary == NULL)
    {
        free(hostIp);
        return -1;
    }
    while (line != NULL)
    {
        char* isSlashR=strstr(line,"\\r");
        if(isSlashR==NULL)
        {
            isSlashR=strstr(line,"\r");
        }
        if(isSlashR!=NULL)
        {
            *isSlashR='\0';
        }
        if (numOrName(line) == 0) // the line is a host name
        {
            if (strcmp(line,host) == 0)
            {
                free(hostIp);
                free(hostInBinary);
                return 1;
            }
        }
        else // the line is an IP address
        {
            if ((int)strlen(line) > 18) // if it's not IPv4
            {
                line = strtok(NULL, "\n");
                continue;
            }
            char* slash = strstr(line, "/");
            int size;
            if (slash == NULL)
            {
                subnetSize = 32; // all the IP
                size= (int)strlen(line);
            }
            else
            {
                subnetSize = (int)strlen(slash);
                size = (int)(slash - line);
            }
            char* filterIp = (char*)malloc(size + 1);
            if (filterIp == NULL)
            {
                free(hostIp);
                free(hostInBinary);
                return -1;
            }
            memset(filterIp, 0, size + 1);
            strncpy(filterIp, line, size);
            filterIp[size] = '\0';
            if (subnetSize != 32)
            {
                char* subnetString = (char*)malloc(subnetSize);
                if (subnetString == NULL) {
                    free(hostIp);
                    free(hostInBinary);
                    free(filterIp);
                    return -1;
                }
                memset(subnetString, 0, subnetSize);
                strncpy(subnetString, slash + 1, subnetSize - 1); // skipping the '/' sign
                subnetString[subnetSize - 1] = '\0';
                subnetSize = atoi(subnetString);
                free(subnetString);
            }
            if (subnetSize <=0 || subnetSize > 32) // the subnet number is incorrect
            {
                free(filterIp);
                line = strtok(NULL, "\n");
                continue;
            }
            char* binaryFilterIp = parseIp(filterIp);
            if (binaryFilterIp == NULL) {
                free(hostIp);
                free(hostInBinary);
                free(filterIp);
                return -1;
            }
            if (strncmp(binaryFilterIp,hostInBinary,subnetSize) == 0) {
                free(filterIp);
                free(hostIp);
                free(hostInBinary);
                free(binaryFilterIp);
                return 1;
            }
            free(filterIp);
            free(binaryFilterIp);
        }
        line = strtok(NULL, "\n");
    }
    free(hostIp);
    free(hostInBinary);
    return 0;
}
int numOrName(const char* host)
{
    // if it's an IP number:
    if(host[0]>='0' && host[0]<='9') return 1;
    //if it's a domain name:
    return 0;
}
char* fromHostToIP(char* domain_name,int clientSocket) //returns a dynamic data that needs to be free!
{
    struct hostent *host;
    char ipStr[INET_ADDRSTRLEN];
    char *result = NULL;
    char* getHostName;
    if(strncmp(domain_name,"www.",4)==0)
    {
        getHostName=domain_name+4;
    }
    else getHostName=domain_name;
    host = gethostbyname(getHostName);
    if (host == NULL)
    {
        herror("error: gethostbyname\n");
        errorHandling(NOT_FOUND,clientSocket);
        return NULL;
    }
    // Extract the IPv4 address
    struct in_addr **addr_list = (struct in_addr **)host->h_addr_list;
    if (addr_list[0] != NULL) {
        strcpy(ipStr, inet_ntoa(*addr_list[0]));
    } else {
        errorHandling(INTERNAL_SERVER_ERROR,clientSocket);
        return NULL; // No IP address found
    }
    // Allocate memory for the result string and copy the IP address
    result = (char*)malloc(INET_ADDRSTRLEN * sizeof(char));
    if (result == NULL)
    {
        errorHandling(INTERNAL_SERVER_ERROR,clientSocket);
        return NULL;
    }
    strcpy(result, ipStr);
    return result;
}
char* parseIp(char* ip) //returns a dynamic data that needs to be free!
{
    int octet[4];
    sscanf(ip, "%d.%d.%d.%d", &octet[0], &octet[1], &octet[2], &octet[3]);
    char* binary_ip =(char *)malloc(sizeof(char)*36); // 4 octets * 8 bits + 3 commas + null terminator
    if (binary_ip == NULL)
    {
        return NULL;
    }
    char* res1= fromNumberToBinary(octet[0]);
    if(res1==NULL) return NULL;
    char* res2= fromNumberToBinary(octet[1]);
    if(res2==NULL)
    {
        free(res1);
        return NULL;
    }
    char* res3= fromNumberToBinary(octet[2]);
    if(res3==NULL)
    {
        free(res1);
        free(res2);
        return NULL;
    }
    char* res4= fromNumberToBinary(octet[3]);
    if(res4==NULL)
    {
        free(res1);
        free(res2);
        free(res3);
        return NULL;
    }
    sprintf(binary_ip, "%08d%08d%08d%08d",atoi(res1),atoi(res2),atoi(res3),atoi(res4));
    free(res1);
    free(res2);
    free(res3);
    free(res4);
    return binary_ip;
}
char* fromNumberToBinary(int n) //returns a dynamic data that needs to be free!
{
    char *binary = (char *)malloc(sizeof(char)*9); // 8 bits + null terminator
    if (binary == NULL)
    {
        return NULL;
    }
    int i;
    for (i = 7; i >= 0; i--)
    {
        binary[i] = (n & 1) ? '1' : '0';
        n >>= 1;
    }
    binary[8] = '\0';
    return binary;
}
int connectToRealServer(char* hostName,int port)
{
    struct hostent* server_info=NULL;
    struct sockaddr_in sock_info;
    int sock_fd;
    if((sock_fd=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP))==-1)
    {
        return -1;
    }
    // building the hostent struct that will have the URL info of the server:
    char* getHostName;
    if(strncmp(hostName,"www.",4)==0)
    {
        getHostName=hostName+4;
    }
    else getHostName=hostName;
    server_info=gethostbyname(getHostName);
    if(!server_info)
    {
        herror("error: gethostbyname\n");
        close(sock_fd);
        return -1;
    }
    // sock_info initialization:
    memset(&sock_info,0,sizeof(struct sockaddr_in));
    sock_info.sin_family=AF_INET;
    sock_info.sin_port=htons((in_port_t)port);
    sock_info.sin_addr.s_addr=((struct in_addr*)server_info->h_addr)->s_addr;
    //connecting to the server:
    if(connect(sock_fd,(struct sockaddr*)&sock_info,sizeof(struct sockaddr_in))==-1)
    {
        close(sock_fd);
        return -1;
    }
    return sock_fd;
}
int writeToSocket(char* fullMessage, int sd) // Assuming that fullMessage is dynamic!!!
{
    size_t  alreadyWrite=0;
    size_t reqLen=strlen(fullMessage);
    while(alreadyWrite < reqLen)// We will continue writing to the server until he takes our request
    {
        ssize_t temp;
        //write to socket:
        temp=write(sd, fullMessage + alreadyWrite, reqLen - alreadyWrite);
        if(temp<0)// If the writing failed
        {
            close(sd);
            return -1;
        }
        alreadyWrite+=temp;// counting the total amount of chars that was writen
    }
    return 0;
}
int readFromServersSocket(int proxyToServerSocket, int proxyToClientSocket)
{
    unsigned char* buffer;
    ssize_t bytesRead;
    while(1)
    {
        buffer = (unsigned char *) malloc(sizeof(unsigned char) * CHUNK_SIZE);
        if (buffer == NULL) {
            close(proxyToServerSocket);
            close(proxyToClientSocket);
            return -1;
        }
        memset(buffer, 0, CHUNK_SIZE);//resets the buffer with zeros
        bytesRead = read(proxyToServerSocket, buffer, (size_t) CHUNK_SIZE);
        if (bytesRead < 0)
        {
            close(proxyToServerSocket);
            close(proxyToClientSocket);
            free(buffer);
            return -1;
        }
        if (bytesRead == 0) {
            free(buffer);
            break;
        }
        int writeRes = writeToClientSocket(buffer, proxyToClientSocket, bytesRead);
        free(buffer);
        if (writeRes == -1)
        {
            return -1;
        }
    }
    close(proxyToServerSocket);
    return 0;
}
int writeToClientSocket(unsigned char* buffer, int sd, ssize_t sum)
{
    ssize_t alreadyWrite = 0;
    while (alreadyWrite < sum)
    {
        ssize_t temp;
        temp = write(sd, buffer + alreadyWrite, sum - alreadyWrite);
        if (temp < 0)
        {
            close(sd);
            return -1;
        }
        alreadyWrite += temp;
    }
    return 0;
}
char* addHeaderConnectionClosed(char* request)//returns a dynamic data that needs to be free!
{
    int reqLen = (int) strlen(request) + 20;//adds new place for the new line
    char *newRequest = (char *) malloc(reqLen);
    if (newRequest == NULL)
    {
        return NULL;
    }
    memset(newRequest, 0, reqLen);
    char requestCopy[reqLen];
    memset(requestCopy, 0, reqLen);
    strcpy(requestCopy, request);
    char* temp1= strstr(request,"Connection:");
    char* temp2= strstr(request,"connection:");
    if(temp1!=NULL || temp2!=NULL)
    {
        char *line = strtok(requestCopy, "\n");
        while (line != NULL) {
            if (strncmp(line, "Connection:", 11) == 0 || strncmp(line, "connection:", 11) == 0) {
                strcat(newRequest, "Connection: close\r\n");
            } else {
                strcat(newRequest, line);
                strcat(newRequest, "\n");
            }
            line = strtok(NULL, "\n");
        }
    }
    else //there is no header named connection
    {
        char* temp=strstr(requestCopy,"\r\n\r\n");
        int len=(int)(temp-requestCopy)+2;//for the \r\n
        strncpy(newRequest,requestCopy,len);
        strcat(newRequest,"Connection: close\r\n\r\n");
    }
    return newRequest;
}




