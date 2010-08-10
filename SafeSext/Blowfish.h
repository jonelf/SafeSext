/*
 * based on Paul Kocher's blowfish as well as the original reference impl.
 * public domain
 */

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  unsigned long P[16 + 2];
  unsigned long S[4*256];
} BLOWFISH_CTX;

void Blowfish_Init(BLOWFISH_CTX *ctx, unsigned char *key, int keyLen);
void Blowfish_Encrypt(BLOWFISH_CTX *ctx, unsigned long *xl, unsigned long *xr);
void Blowfish_Decrypt(BLOWFISH_CTX *ctx, unsigned long *xl, unsigned long *xr);

#ifdef __cplusplus
};
#endif