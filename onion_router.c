#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <openssl/rsa.h>
#include <inttypes.h>

#include "networks.h"
#include "onion_router.h"
#include "rsa.h"
#include "smartalloc.h"

int clientSockets[MAX_SOCKETS];

char keyServerIP[4] = {127,0,0,1};

char *outPort = "40501";

RSA *privateKey;

int main(int argc, char **argv)
{

    if (argc > 1)
    {
        printf("%s\n", argv[1]);

        sscanf(argv[1],"%u.%u.%u.%u",&keyServerIP[0],&keyServerIP[1],&keyServerIP[2],&keyServerIP[3]);
        //keyServerIP[0] = atoi(argv[1]);

    }

    init();

    FILE *pub = fopen("id_rsa.pub", "a+");
    FILE *priv = fopen("id_rsa", "a+");
    fseek(pub,0,SEEK_SET);
    fseek(priv,0,SEEK_SET);

    if (!keyExists(priv))
    {
        RSA *key = genkey();
        writePublicKey(key, pub);
        writePrivateKey(key, priv);
        RSA_free(key);
    }
    postPublicKey(keyServerIP, pub);
    privateKey = readPrivateKey(priv);
    fclose(pub);
    fclose(priv);

    int serverSocket = 0; // socket descriptor for the server socket
    int startSocket = 0;
    // int clientSocket
    int portNumber = 40501;
    int startPort = 6969; // nice

    // create the server socket
    serverSocket = tcpServerSetup(portNumber);
    startSocket = tcpServerSetup(startPort);
    runRouter(serverSocket, portNumber, startSocket);
    return 0;
}

void runRouter(int serverSocket, int portNumber, int startSocket)
{
    fd_set rfds;
    int itr;
    struct clientNode *curNode = NULL;
    struct clientNode *headClientNode = curNode;
    int maxSocket =
        serverSocket; // need this because we are going to loop throug hte set of
    // sockets used and we need to know where to stop looking
    int numHops = 2; // change this obv

    for (itr = 0; itr < MAX_SOCKETS; itr++)
    {
        clientSockets[itr] = -1;
    }

    if (startSocket > maxSocket)
    {
        maxSocket = startSocket;
    }
    while (1)
    {
        printf("here\n");
        FD_ZERO(&rfds);
        FD_SET(serverSocket, &rfds); // watch socket for update
        FD_SET(startSocket, &rfds);
        // go through list of connected clients and watch each of the sockets
        curNode = headClientNode;
        while (curNode != NULL)
        {
            FD_SET(curNode->port_pair.in_socket, &rfds);
            if (maxSocket < curNode->port_pair.in_socket)
            {
                maxSocket = curNode->port_pair.in_socket;
            }
            FD_SET(curNode->port_pair.out_socket, &rfds);
            if (maxSocket < curNode->port_pair.out_socket)
            {
                maxSocket = curNode->port_pair.out_socket;
            }
            curNode = curNode->next;
        }
        if (select(maxSocket + 1, &rfds, NULL, NULL, NULL) < 0)
        {
            perror("server select call error");
            exit(-1);
        }
        // incoming starting node data
        if (FD_ISSET(startSocket, &rfds))
        {
            printf("new start node being accepted\n");
            newStart(startSocket, &headClientNode, numHops);
        }
        // new connection to server socket
        if (FD_ISSET(serverSocket, &rfds))
        {
            printf("new connection being accepted\n");
            newConnection(serverSocket, &headClientNode);
        }
        // now check for client activity by looping through the set of sockets
        curNode = headClientNode;
        while (curNode != NULL)
        {
            // check if curNode's socket is set
            if (FD_ISSET(curNode->port_pair.in_socket, &rfds))
            {
                if (curNode->nodeType < 0)
                {
                    printf("start node activitiy\n");
                    startClientActivity(curNode, numHops);
                }
                else if (curNode->nodeType == 0)
                {
                    printf("client activity\n");
                    clientActivity(curNode);
                }
                else
                {
                    //exit
                }
            }
            if (FD_ISSET(curNode->port_pair.out_socket, &rfds))
            {
                printf("return activitiy\n");
                clientReturnActivity(curNode);
            }
            curNode = curNode->next;

        }
    }
}


struct onionHeader getOnionHeader(int tmp_itr, int totalSize)
{
    struct onionHeader header;

    // this is all temporary for now
    switch (tmp_itr)
    {
    case 0:
        header.packetLength = htons(totalSize);
        header.next_hop[0] = 208;
        header.next_hop[1] = 94;
        header.next_hop[2] = 62;
        header.next_hop[3] = 131;
    case 1:
        header.packetLength = htons(totalSize - sizeof(struct onionHeader));
        header.next_hop[0] = 208;
        header.next_hop[1] = 94;
        header.next_hop[2] = 62;
        header.next_hop[3] = 132;
    case 2:
        header.packetLength = htons(totalSize - (sizeof(struct onionHeader) * 2));
        header.next_hop[0] = 127;
        header.next_hop[1] = 0;
        header.next_hop[2] = 0;
        header.next_hop[3] = 1;
    }
    return header;
}

int buildHops(char *buf, int numHops, short bodySize, struct entryClientNode *node)
{
    char swapBuff[MAX_PACKET_SIZE];
    char flag = 1;
    int itr;
    struct onionHeader *header = (struct onionHeader *)buf;
    //encrypt hop 0
    header->next_hop[0] = 0;
    header->next_hop[1] = 0;
    header->next_hop[2] = 0;
    header->next_hop[3] = 0;

    //bodySize = encrypt(node->keys[0],(char *)header,swapBuff,bodySize);
    //memcpy((char *)header,(char *)swapBuff,bodySize);

    for (itr = numHops - 1; itr >= 0; itr--) {

        //encrypt here
        bodySize = encrypt(node->keys[itr],(char *)header,swapBuff,bodySize + ONION_HDR_SIZE);
        memcpy(buf+ ONION_HDR_SIZE,(char *)swapBuff,bodySize);
        memcpy(header->next_hop, node->path[itr],4);
        header->packetLength = bodySize;
    }
    return bodySize;
}


void pickHops(struct clientNode *pNode, int numHops) {
    ip_list ips = getPublicKeys(keyServerIP);
    int i;

    int *array = calloc(1,ips.numIps);
    struct entryClientNode *node = (struct entryClientNode *) pNode;

    for (int i = 0; i < numHops; i++)
    { // fill array
        array[i] = i;
    }

    for (int i = 0; i < numHops; i++)
    { // shuffle array
        int temp = array[i];
        int randomIndex = rand() % numHops;

        array[i] = array[randomIndex];
        array[randomIndex] = temp;
    }

    for (i=0;i<numHops;i++) {
        sscanf(ips.ips[array[i]],"%u.%u.%u.%u",&node->path[i][0],
               &node->path[i][1],&node->path[i][2],&node->path[i][3]);
        node->keys[i] = readPublicKeyByIPname(ips.ips[array[i]]);
    }
}

void newStart(int startSocket, struct clientNode **head, int numHops)
{
    uint16_t sendLength;
    int clientInSocket;
    char buf[MAX_PACKET_SIZE];
    char packet[MAX_PACKET_SIZE];
    short packetSize;
    char nextHopIp[16];

    if ((clientInSocket =
             accept(startSocket, (struct sockaddr *)0, (socklen_t *)0)) < 0)
    {
        perror("accept call error");
        exit(-1);
    }

    struct entryClientNode *node = (struct entryClientNode *)addClientNode(head, clientInSocket, 0, -1);
    pickHops(node, numHops);

    node->port_pair.out_socket = tcpClientSetup(node->path[0], outPort, 0);

    packetSize = recv(node->port_pair.in_socket,buf,MAX_PACKET_SIZE,0);
    //shift to back of buffer
    memcpy(packet + ONION_HDR_SIZE,buf,packetSize);

    packetSize = buildHops(packet, numHops, packetSize, node);

    send(node->port_pair.out_socket, packet + ONION_HDR_SIZE, packetSize,0);
}

void newConnection(int serverSocket, struct clientNode **head)
{
    char buf[MAX_PACKET_SIZE];
    char dec[MAX_PACKET_SIZE];
    struct onionHeader *header;

    struct clientNode *curNode = addClientNode(head,0,0,0);
    curNode->port_pair.in_socket = accept(serverSocket,(struct sockaddr *)0, (socklen_t *)0);
    //printf("test,%d\n",header->next_hop[3]);

    ssize_t len = 0;
    if ((len = recv(curNode->port_pair.in_socket, buf, MAX_PACKET_SIZE,MSG_WAITALL)) < 0)
    {
        perror("Error recieving packet\n");
        //exit(-1);
    } else if (len == 0) {
        return;
    } else {
        //decrypt
        len = decrypt(privateKey, (char *)buf, (char *)dec, (int)len);
        header = (struct onionHeader *) dec;
        printf("here2:%d\n",header->next_hop[3]);
        if (isDest(*header)) {
            header++;
            curNode->nodeType = 1;
            exitNode((char *) header,curNode);
        } else {
            curNode->nodeType = 0;
            curNode->port_pair.out_socket = tcpClientSetup(header->next_hop,outPort,0);
            header++;
        }
        len -= sizeof(struct onionHeader);
        send(curNode->port_pair.out_socket, (char *) header, (size_t) len, 0);
    }
}

struct clientNode * addClientNode(struct clientNode **head, int in_socket, int out_socket, int nodeType)
{
    struct clientNode *newConnectionNode;
    if (nodeType == 1)
        newConnectionNode =(struct clientNode *)calloc(1, sizeof(struct entryClientNode));
    else
        newConnectionNode =(struct clientNode *)calloc(1, sizeof(struct clientNode));
    //struct clientNode *curNode;

    newConnectionNode->port_pair.in_socket = in_socket;
    newConnectionNode->port_pair.out_socket = out_socket;
    newConnectionNode->nodeType = nodeType;
    newConnectionNode->next = NULL;

    if (*head == NULL)
    {
        *head = newConnectionNode;
    }
    else
    {
        newConnectionNode->next = *head;
        *head = newConnectionNode;
    }
    return newConnectionNode;
}

int recieveStart(int socket, char *packet)
{
    int recvLen = -1;

    recvLen = recv(socket, packet, 2048, MSG_WAITALL);

    printf("Start node recieved %d bytes\n", recvLen);
    return recvLen;
}

int recievePacket(int socket, char *packet)
{
    uint16_t packetLength = 0;
    int recvLen = -1;

    if ((recvLen = recv(socket, packet, sizeof(uint16_t), MSG_WAITALL)) < 2)
    {
        if (recvLen <= 0)
        {
            return 1;
        }
    }
    memcpy(&packetLength, packet, sizeof(uint16_t));
    packetLength = ntohs(packetLength);

    recvLen += recv(socket, packet + sizeof(uint16_t),
                    packetLength - sizeof(uint16_t), MSG_WAITALL);
    if (recvLen < packetLength)
    {
        perror("error recieveing packet");
    }
    return 0;
}



int getOutSocket(int clientInSocket, struct clientNode *head)
{
    struct clientNode *curNode;
    curNode = head;

    while (curNode != NULL)
    {
        if (curNode->port_pair.in_socket == clientInSocket)
        {
            return curNode->port_pair.out_socket;
        }
        curNode = curNode->next;
    }
    return -1;
}

int isDest(struct onionHeader header)
{
    int itr;

    for (itr = 0; itr < 4; itr++)
    {
        if (header.next_hop[itr] != 0x00)
        {
            return 0;
        }
    }
    return 1;
}

int startClientActivity(struct clientNode *startNode, uint16_t numHops)
{
    char buf[MAX_PACKET_SIZE];
    char packet[MAX_PACKET_SIZE];
    ssize_t packetSize;


    packetSize = recv(startNode->port_pair.in_socket,buf,MAX_PACKET_SIZE,0);
    //shift to back of buffer
    memcpy(packet + ONION_HDR_SIZE,buf,packetSize);

    packetSize = buildHops(packet, numHops, packetSize, (struct entryClientNode *) startNode);

    send(startNode->port_pair.out_socket, packet + ONION_HDR_SIZE, packetSize,0);

    return 0;
}

int clientActivity(struct clientNode *curNode)
{
    char buf[MAX_PACKET_SIZE];
    char dec[MAX_PACKET_SIZE];
    struct onionHeader *header;

    ssize_t len = 0;
    if ((len = recv(curNode->port_pair.in_socket, buf, MAX_PACKET_SIZE,0)) < 0)
    {
        perror("Error recieving packet\n");
        //exit(-1);
    } else {
        //decrypt
        //if (curNode->nodeType == 1) {
        //    header = (struct onionHeader *) buf;
        //} else {
        len = decrypt(privateKey, (char *)buf, (char *)dec, (int)len);
        header = (struct onionHeader *) dec;
        //}
        len -= sizeof(struct onionHeader);
        header++;
        send(curNode->port_pair.out_socket, (char *) header, (size_t) len, 0);
    }
    return 0;
}

int clientReturnActivity(struct clientNode *curNode)
{
    char buf[MAX_PACKET_SIZE];

    ssize_t len = 0;
    if ((len = recv(curNode->port_pair.out_socket, buf, MAX_PACKET_SIZE,0)) < 0)
    {
        perror("Error recieving packet\n");
        //exit(-1);
    } else {
        send(curNode->port_pair.in_socket,buf,(size_t)len,0);
    }
    return 0;
}


char port[6];
char url[256];
char *port80 = "80";

void setPortAndURL(char *head)
{
    head = strstr(head, "Host: ") + 6;
    char *tail = strchr(head, '\r');
    if (tail == NULL)
        tail = strchr(head, '\n');
    char *split = strchr(head, ':');
    if (split < head || split > tail)
    {
        strcpy(port, port80);
        memcpy(url, head, tail - head);
        url[tail - head] = 0;
    }
    else
    {
        memcpy(port, split + 1, tail - (split + 1));
        port[tail - (split + 1)] = 0;
        memcpy(url, head, (split - 1) - head);
        url[(split - 1) - head] = 0;
    }
}
//set in sock before (packet should be pointed to an unencrpted HTTP header string)
void exitNode(char *packet, struct clientNode *node)
{
    setPortAndURL(packet);
    node->port_pair.out_socket = tcpClientSetupChar(url, port, 0);
    if (memcmp(packet, "GET ", 4) == 0)
    {
        //forward GET
        send(node->port_pair.out_socket, packet, strlen(packet), 0);
    }
    else
    {
        send(node->port_pair.out_socket, packet, strlen(packet), 0);
        //no forward connect
    }
}