
// Hugh Smith April 2017
// Network code to support TCP client server connections

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>

#include "networks.h"
#include "gethostbyname6.h"

/* This function sets the server socket.  It lets the system
determine the port number.  The function returns the server
socket number and prints the port number to the screen.  */

int tcpServerSetup(int portNumber)
{
	int server_socket= 0;
	struct sockaddr_in server;      /* socket address for local side  */
	socklen_t len= sizeof(server);  /* length of local address        */

	/* create the tcp socket  */
	server_socket= socket(AF_INET, SOCK_STREAM, 0);

    int option = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));

	if(server_socket < 0)
	{
		perror("socket call");
		exit(1);
	}

	server.sin_family= AF_INET;         		
	server.sin_addr.s_addr = INADDR_ANY;   //wild card machine address
	server.sin_port= htons(portNumber);         

	/* bind the name (address) to a port */
	if (bind(server_socket, (struct sockaddr *) &server, sizeof(server)) < 0)
	{
		perror("bind call");
		exit(-1);
	}
	
	//get the port name and print it out
	if (getsockname(server_socket, (struct sockaddr*)&server, &len) < 0)
	{
		perror("getsockname call");
		exit(-1);
	}

	if (listen(server_socket, BACKLOG) < 0)
	{
		perror("listen call");
		exit(-1);
	}
	
	printf("Server is using port %d \n", ntohs(server.sin_port));
	
	return server_socket;
}

int tcpAccept(int server_socket, int debugFlag)
{
	struct sockaddr_in clientInfo;   
	int clientInfoSize = sizeof(clientInfo);
	int client_socket= 0;
	//uint8_t *s_addr_conv;

	if ((client_socket = accept(server_socket, (struct sockaddr*) &clientInfo, (socklen_t *) &clientInfoSize)) < 0)
	{
		perror("accept call");
		exit(-1);
	}
	

	//if (debugFlag)
	//{
	//	printf("Client accepted.  Client IP: %s Client Port Number: %d\n",  
	//			getIPAddressString(clientInfo.sin_addr.s_addr), ntohs(clientInfo.sin_port));
	//}
	

	return(client_socket);
}

int tcpClientSetup(char * serverName, char * port, int debugFlag) 
{
	// This is used by the client to connect to a server using TCP
	unsigned char *ips = (unsigned char *)serverName;
    int x = ips[3]*256*256*256 + ips[2]*256*256 + ips[1]*256 + ips[0];

	int socket_num;
	uint8_t * ipAddress = NULL;
	struct sockaddr_in server;      

	// create the socket
	if ((socket_num = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		perror("socket call");
		exit(-1);
	}
	// setup the server structure
	server.sin_family = AF_INET;
	server.sin_port = htons(atoi(port));
	server.sin_addr.s_addr = x;

	// get the address of the server 
//	if ((ipAddress = getIPAddress(serverName, &server)) == NULL)
//	{
//		perror("get ip call");
//		exit(-1);
//	}

	if(connect(socket_num, (struct sockaddr*)&server, sizeof(server)) < 0)
	{
		perror("connect call");
		exit(-1);
	}
	if (debugFlag)
	{
		printf("Connected to %s IP: %s Port Number: %d\n", serverName, getIPAddressString(ipAddress), atoi(port));
	}
	
	return socket_num;
}
int tcpClientSetupChar(char * serverName, char * port, int debugFlag)
{
    // This is used by the client to connect to a server using TCP

    int socket_num;
    uint8_t * ipAddress = NULL;
    struct sockaddr_in server;

    // create the socket
    if ((socket_num = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("socket call");
        exit(-1);
    }
    // setup the server structure
    server.sin_family = AF_INET;
    server.sin_port = htons(atoi(port));
    //server.sin_addr.s_addr = (int) *serverName;

    // get the address of the server
	if ((ipAddress = getIPAddress(serverName, &server)) == NULL)
	{
		perror("get ip call");
		return (-1);
	}

    if(connect(socket_num, (struct sockaddr*)&server, sizeof(server)) < 0)
    {
        perror("connect call");
        exit(-1);
    }
    if (debugFlag)
    {
        printf("Connected to %s IP: %s Port Number: %d\n", serverName, getIPAddressString(ipAddress), atoi(port));
    }

    return socket_num;
}

//char name[256];
//char * getInterfaceIpAddress(int socket) {
//    struct ifreq ifr;
//    struct sockaddr sockaddrIn;
//    socklen_t socklen = 4;
//
//    //fd = tcpClientSetup(server,"8000",0);
////    fd = socket(AF_INET, SOCK_DGRAM, 0);
//
//    /* I want to get an IPv4 IP address */
//    ifr.ifr_addr.sa_family = AF_INET;
//
//    /* I want IP address attached to "eth0" */
////    strncpy(ifr.ifr_name, interface, IFNAMSIZ-1);
//
//    //ioctl(fd, SIOCGIFADDR, &ifr);
//
//
//    int x = getsockname(socket, &sockaddrIn,&socklen);
//    printf("%s%d\n",sockaddrIn.sa_data,x);
//    //close(fd);
//
//    char *temp = inet_ntoa(( (struct sockaddr_in *)&ifr.ifr_addr )->sin_addr);
////    printf("%s - %s\n" , interface , temp);
//    return temp;
//}