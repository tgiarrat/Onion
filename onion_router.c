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

#include "networks.h"
#include "onion_router.h"
#include "rsa.h"

int clientSockets[MAX_SOCKETS];

char *serverString = "127.0.0.1";

int main(int argc, char **argv)
{

    if (argc > 1)
    {
        serverString = argv[1];
        printf("%s\n", serverString);
    }

    init();

    FILE *pub = fopen("id_rsa.pub", "w+");
    FILE *priv = fopen("id_rsa", "w+");

    if (!keyExists(priv))
    {
        RSA *key = genkey();
        writePublicKey(key, pub);
        writePrivateKey(key, priv);
        RSA_free(key);
        postPublicKey(serverString, pub);
    }

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
    int ret;
    int numHops = 0; // change this obv

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
                    ret = startClientActivity(curNode, numHops);
                }
                printf("client activity\n");
                ret = clientActivity(curNode);
            }
            if (curNode != NULL)
            {
                curNode = curNode->next;
            }
        }
    }
}

void removeClientSocket(int clientSocket)
{
    int itr = 0;

    for (; itr < MAX_SOCKETS; itr++)
    {
        if (clientSockets[itr] == clientSocket)
        {
            clientSockets[itr] = -1;
            itr = MAX_SOCKETS + 1;
        }
    }
}

void setClientSocket(int clientSocket)
{
    int itr = 0;

    for (; itr < MAX_SOCKETS; itr++)
    {
        if (clientSockets[itr] < 0)
        {
            clientSockets[itr] = clientSocket;
            itr = MAX_SOCKETS + 1;
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

int buildHops(char *buf, int numHops, int bodySize)
{
    int array[numHops];

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

    ip_list ips = getPublicKeys(serverString);

    printf("first hop ip:%s", ips.ips[0]);

    uint16_t totalHeaderSize;
    int itr;
    struct onionHeader header;

    totalHeaderSize =
        (numHops + 1) * sizeof(struct onionHeader); // plus 1 for the end hop

    for (itr = 0; itr < numHops; itr++)
    {
        header = getOnionHeader(itr, totalHeaderSize + bodySize);
        memcpy(buf + (itr * sizeof(struct onionHeader)), &header,
               sizeof(struct onionHeader));
    }
    return totalHeaderSize + bodySize;
}

void newStart(int startSocket, struct clientNode **head, int numHops)
{
    uint16_t sendLength;
    int clientInSocket;
    int clientOutSocket;
    char buf[MAX_PACKET_SIZE];
    int packetSize;
    char nextHopIp[4];
    char *outPort = "40501";
    if ((clientInSocket =
             accept(startSocket, (struct sockaddr *)0, (socklen_t *)0)) < 0)
    {
        perror("accept call error");
        exit(-1);
    }
    printf("recieveing start...");
    packetSize = recieveStart(clientInSocket,
                              buf + ((numHops + 1) * sizeof(struct onionHeader)));
    sendLength = buildHops(buf, numHops, packetSize);
    sendLength -= sizeof(struct onionHeader);
    memcpy(nextHopIp, buf + sizeof(uint16_t), 4);
    printf("next hop ip is %d.%d.%d.%d\n", nextHopIp[0], nextHopIp[1],
           nextHopIp[2], nextHopIp[3]);
    clientOutSocket = tcpClientSetup(nextHopIp, outPort, 1);
    addClientNode(head, clientInSocket, clientOutSocket);
    // make a struct where the out and in sockets are paired
    sendPacket(clientOutSocket, buf, sendLength);
    // add client
}

void newConnection(int serverSocket, struct clientNode **head)
{
    int clientInSocket;
    int clientOutSocket;
    uint16_t sendLength;
    char *outPort = "40501";
    char buf[MAX_PACKET_SIZE];
    char nextHopIp[4];

    if ((clientInSocket =
             accept(serverSocket, (struct sockaddr *)0, (socklen_t *)0)) < 0)
    {
        perror("accept call error");
        exit(-1);
    }
    // setClientSocket(clientInSocket);
    recievePacket(clientInSocket, buf);

    // now setup this as a client for another node
    memcpy(&sendLength, buf, 2);
    sendLength = ntohs(sendLength);
    sendLength -= sizeof(struct onionHeader);
    memcpy(nextHopIp, buf + sizeof(uint16_t), 4);
    printf("next hop ip is %d.%d.%d.%d\n", nextHopIp[0], nextHopIp[1],
           nextHopIp[2], nextHopIp[3]);
    clientOutSocket = tcpClientSetup(nextHopIp, outPort, 1);

    addClientNode(head, clientInSocket, clientOutSocket);
    // make a struct where the out and in sockets are paired
    sendPacket(clientOutSocket, buf, sendLength);
}

void addClientNode(struct clientNode **head, int in_socket, int out_socket)
{
    struct clientNode *newConnectionNode =
        (struct clientNode *)calloc(1, sizeof(struct clientNode));
    struct clientNode *curNode;

    newConnectionNode->port_pair.in_socket = in_socket;
    newConnectionNode->port_pair.out_socket = out_socket;
    newConnectionNode->next = NULL;

    if (*head == NULL)
    {
        *head = newConnectionNode;
    }
    else
    {
        // iterate to the end of the list
        curNode = *head;
        while (curNode->next != NULL)
        {
            curNode = curNode->next;
        }
        curNode->next = newConnectionNode;
    }
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

int sendPacket(int out_socket, char *packet, int sendLength)
{
    int sent;
    // send packet:
    sent = send(out_socket, packet + sizeof(struct onionHeader), sendLength, 0);
    if (sent < 0)
    {
        perror("send call");
        exit(-1);
    }
    if (sent == 0)
    {
        perror("sent zero");
        exit(-1);
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
    int clientInSocket;
    int clientOutSocket;
    uint16_t bodySize;
    char buf[MAX_PACKET_SIZE];

    clientInSocket = startNode->port_pair.in_socket;
    clientOutSocket = startNode->port_pair.out_socket;

    bodySize = recieveStart(clientInSocket, buf);
    if (bodySize < 0)
    {
        perror("error recieving a starting packet!\n");
        exit(-1);
    }
    buildHops(buf, numHops, bodySize);

    return 0;
}

int clientActivity(struct clientNode *curNode)
{
    int clientInSocket;
    int clientOutSocket;
    uint16_t sendLength;
    char buf[MAX_PACKET_SIZE];
    struct onionHeader header;

    clientInSocket = curNode->port_pair.in_socket;
    clientOutSocket = curNode->port_pair.out_socket;
    //clientOutSocket = getOutSocket(clientInSocket, *head);
    //if (clientOutSocket < 0) {
    //    perror("Error getting matching out port\n");
    //    exit(-1);
    //}
    if (recievePacket(clientInSocket, buf) == 1)
    {
        perror("Error recieving packet\n");
        exit(-1);
    }

    memcpy(&header, buf, sizeof(struct onionHeader));
    sendLength = ntohs(header.packetLength);
    sendLength -= sizeof(struct onionHeader);
    if (!isDest(header))
    { // probably going to want some sort of check to see if
        // this is the final dest. maybe a hardcoded ip
        sendPacket(clientOutSocket, buf, sendLength);
    }
    else
    {
        printf("DEST LOGIC HERE\n\n\n\n");
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
    node->port_pair.out_socket = tcpClientSetup(url, port, 0);
    if (memcmp(packet, "GET ", 4) == 0)
    {
        //forward GET
        send(node->port_pair.out_socket, packet, strlen(packet), 0);
    }
    else
    {
        //no forward connect
    }
}