//
// Created by kyle on 3/11/18.
//

#ifndef RSA_TEST_RSA_H
#define RSA_TEST_RSA_H

#define KEYSIZE 2048

BIGNUM *e;
void init();
void stop(RSA *keypair);

int encrypt(RSA *keypair,char *data, char *encrypted_data, int length);
int decrypt(RSA *keypair,char *encrypted_data, char *decrypted_data, int length);
RSA *genkey();

void writePrivateKey(RSA *keypair, FILE *fd);
void writePublicKey(RSA *keypair, FILE *fd);

typedef struct key_list_t {
    char    ips[16][32];
    char    numIps;
}ip_list;

int connectToServer(char *server);
void postPublicKey(char *server, FILE *fd);
ip_list getPublicKeys(char *server);

RSA *readPrivateKey(FILE *fd);
RSA *readPublicKey(FILE *fd);
int keyExists(FILE *fd);

RSA *readPublicKeyByIPname(char * ip);
RSA *readPublicKeyByIPdata(u_int8_t * ip);

#endif //RSA_TEST_RSA_H
