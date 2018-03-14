//
// Created by kyle on 3/11/18.
//

#include <openssl/pem.h>
#include <openssl/err.h>
#include <string.h>
#include <unistd.h>
#include "networks.h"

#include <arpa/inet.h>
#include <inttypes.h>

#include "rsa.h"

void init() {
    e = malloc(sizeof(BIGNUM));
    BN_init(e);
    unsigned char num = 17;
    BN_bin2bn(&num,1,e);
}
void stop(RSA *keypair) {
    BN_free(e);
    RSA_free(keypair);
}

int encrypt(RSA *keypair, char *data, char *encrypted_data, int length) {
    int size = 0;
    int pos = 0;
    int max = RSA_size(keypair)/2;
    char err[256];
    int temp;
    int e_len;
    while (pos < length) {
        e_len = length - pos > max ? max : length - pos;

        temp = RSA_public_encrypt(e_len,(unsigned char *)data + pos,(unsigned char *)encrypted_data + size,
                                   keypair,RSA_PKCS1_OAEP_PADDING);
        size+=temp;
        if (temp < 0) {
            ERR_load_crypto_strings();
            ERR_error_string(ERR_get_error(), err);
            fprintf(stderr, "Error decrypting message: %s\n", err);
        }
        pos+=max;
    }
    return size;
}

int decrypt(RSA *keypair, char *encrypted_data, char *decrypted_data, int length) {
    int size = 0;
    int pos = 0;
    int max = 256;
    char err[256];
    int temp;
    int d_len;
    while (pos < length) {
        d_len = length - pos > max ? max : length - pos;
        temp = RSA_private_decrypt(d_len,(unsigned char *)encrypted_data + pos,(unsigned char *)decrypted_data + size,
                                    keypair,RSA_PKCS1_OAEP_PADDING);
        size+=temp;
        if (temp < 0) {
            ERR_load_crypto_strings();
            ERR_error_string(ERR_get_error(), err);
            fprintf(stderr, "Error decrypting message: %s\n", err);
        }
        pos+=max;
    }
    return size;
}

RSA *genkey()  {
    RSA *keypair = RSA_new();
    RSA_generate_key_ex(keypair,KEYSIZE,e,0);
    return keypair;
}

void writePrivateKey(RSA *keypair, FILE *fd) {
    BIO *pri = BIO_new(BIO_s_mem());

    PEM_write_bio_RSAPrivateKey(pri, keypair, NULL, NULL, 0, NULL, NULL);

    size_t pri_len = BIO_pending(pri);

    char *pri_key = malloc(pri_len + 1);

    BIO_read(pri, pri_key, pri_len);

    pri_key[pri_len] = '\0';

    fwrite(pri_key,sizeof(char),pri_len, fd);

    BIO_free_all(pri);
    free(pri_key);
}

void writePublicKey(RSA *keypair, FILE *fd) {
    BIO *pub = BIO_new(BIO_s_mem());

    PEM_write_bio_RSAPublicKey(pub, keypair);

    size_t pub_len = BIO_pending(pub);

    char *pub_key = malloc(pub_len + 1);

    BIO_read(pub, pub_key, pub_len);

    pub_key[pub_len] = '\0';

    fwrite(pub_key,sizeof(char),pub_len, fd);

    BIO_free_all(pub);
    free(pub_key);
}

RSA *readPublicKey(FILE *fd) {
    RSA *key = RSA_new();
    char *pub_key = malloc(4096);
    BIO *pub = BIO_new(BIO_s_mem());

    fseek (fd, 0, SEEK_END);
    long pub_len = ftell (fd);
    fseek (fd, 0, 0);

    fread(pub_key,sizeof(char),pub_len,fd);

    pub_key[pub_len] = '\0';

    BIO_write(pub,pub_key,pub_len);

    PEM_read_bio_RSAPublicKey(pub,&key,NULL,NULL);

    BIO_free_all(pub);
    free(pub_key);

    return key;
}

RSA *readPrivateKey(FILE *fd) {
    BIO *pri = BIO_new(BIO_s_mem());
    RSA *key = RSA_new();
    char *pri_key = malloc(4096);

    fseek (fd, 0, SEEK_END);
    long pri_len = ftell (fd);
    fseek (fd, 0, 0);
    fseek (fd, 0, 0);

    fread(pri_key,sizeof(char),pri_len,fd);

    pri_key[pri_len] = '\0';

    BIO_write(pri,pri_key,pri_len);

    PEM_read_bio_RSAPrivateKey(pri,&key,NULL,NULL);

    BIO_free_all(pri);
    free(pri_key);

    return key;
}

int connectToServer(char *server) {
    char *port = "8000";
    int client = tcpClientSetup(server,port,0);
    return client;
}

char *postKeyPacket = "PUT /%s HTTP/1.1\r\n"
        "Host: localhost:8001\r\n"
        "Connection: keep-alive\r\n"
        "Cache-Control: no-cache\r\n"
        "Content-Length: %d\r\n"
        "\r\n"
        "%s";

void postPublicKey(char *server, FILE *fd) {

    char buff[4096];
    char packet[4096];
    int serverSocket = connectToServer(server);

    fseek (fd, 0, SEEK_END);
    long pub_len = ftell (fd);
    fseek (fd, 0, 0);
    fread(buff,sizeof(char),pub_len,fd);
    fseek (fd, 0, 0);

    sprintf(packet,postKeyPacket,"xxxx",pub_len,buff);

    send(serverSocket,packet,strlen(packet),0);
    close(serverSocket);
}

char *getListPacket = "GET /list HTTP/1.1\r\n"
        "Host: localhost:8001\r\n"
        "Connection: keep-alive\r\n"
        "Cache-Control: no-cache\r\n"
        "User-Agent: Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/64.0.3282.167 Safari/537.36\r\n"
        "Accept: */*\r\n"
        "Accept-Encoding: identity\r\n"
        "Accept-Language: en-US,en;q=0.9\r\n"
        "\r\n";

char *getKeyPacket = "GET /keys/%s HTTP/1.1\r\n"
        "Host: localhost:8001\r\n"
        "Connection: keep-alive\r\n"
        "Cache-Control: no-cache\r\n"
        "User-Agent: Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/64.0.3282.167 Safari/537.36\r\n"
        "Postman-Token: fad3c1f4-ce54-ab01-3eab-b5efa1d4a980\r\n"
        "Accept: */*\r\n"
        "Accept-Encoding: identity\r\n"
        "Accept-Language: en-US,en;q=0.9\r\n"
        "\r\n";

char *getBody(char * head) {
    char *pos = strrchr(head,':');
    pos = strchr(pos,'\n');
    return strchr(pos + 1,'\n') + 1;
}

key_list getPublicKeys(char *server) {
    char buff[5096] = {0};
    char keyBuff[5096] = {0};
    char packet[512] = {0};
    char ip[16] = {0};
    key_list list;
    FILE *key;
    int serverSocket = connectToServer(server);


    if (-1 == send(serverSocket,getListPacket,strlen(getListPacket),MSG_MORE)) {
        perror("ERROR:\n");
    }

    if (-1 == recv(serverSocket,buff,5096,MSG_WAITALL)) {
        perror("ERROR:\n");
    }
    close(serverSocket);
    int count = 0;
    char *pos = getBody(buff);
    while (pos > buff && EOF != sscanf( pos,"%s\n",ip)) {
        memcpy(&list.ips[(int)list.numIps][0],ip,16);
        list.numIps++;
        key = fopen(ip,"w+");
        if (!keyExists(key)) {
            sprintf(packet,getKeyPacket,ip);
            serverSocket = connectToServer(server);
            if (-1 == send(serverSocket,packet,strlen(packet),0)) {
                perror("ERROR:\n");
            }

            if (-1 == recv(serverSocket,keyBuff,5096,MSG_WAITALL)) {
                perror("ERROR:\n");
            }
            close(serverSocket);
            //strchr(keyBuff)
            count++;
            pos=strchr(pos,'\n')+1;
            char *keyPos = getBody(keyBuff);
            fwrite(keyPos, sizeof(char),strlen(keyPos),key);
        }
        fclose(key);
    }
    return list;
}

int keyExists(FILE *fd) {
    fseek (fd, 0, SEEK_END);
    long len = ftell (fd);
    fseek (fd, 0, 0);
    return len > 0;
}


RSA *readPublicKeyByIPname(char * ip) {
    FILE *temp = fopen(ip,"w+");
    RSA *key = readPublicKey(temp);
    fclose(temp);
    return key;
}
RSA *readPublicKeyByIPdata(u_int8_t * ip) {
    char ipname[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    sprintf(ipname,"%"PRIu8".%"PRIu8".%"PRIu8".%"PRIu8"",ip[0],ip[1],ip[2],ip[3]);
    return readPublicKeyByIPname(ipname);
}

