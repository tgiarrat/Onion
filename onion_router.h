
#define MAX_SOCKETS 256
#define ONION_HDR_SIZE sizeof(struct onionHeader)
#define MAX_BUF 65536

struct port_pair
{
    int in_socket;
    int out_socket;
} __attribute__((packed));

struct clientNode
{
    uint8_t nodeType; // -1 for start, 0 for transit, 1 for end
    struct port_pair port_pair;
    //int clientSockets[MAX_SOCKETS];
    struct clientNode *next;
};

struct onionHeader
{
    uint16_t packetLength; //total packet length including this header
    uint8_t next_hop[4];
} __attribute__((packed));

void runRouter(int serverSocket, int portNumber, int startSocket);
void newConnection(int serverSocket, struct clientNode **head);
int clientActivity(struct clientNode *curNode);
int startClientActivity(struct clientNode *startNode, uint16_t numHops);
int recievePacket(int socket, char *packet);
int sendPacket(int out_socket, char *packet, int sendLength);
void addClientNode(struct clientNode **head, int in_socket, int out_socket, int nodeType);
void newStart(int startSocket, struct clientNode **head, int numHops);
int recieveStart(int socket, char *packet);
int isDest(struct onionHeader header);
void exitNode(char *packet, struct clientNode *node);