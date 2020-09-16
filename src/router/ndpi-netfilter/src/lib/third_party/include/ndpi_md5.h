/*
  Contributed code pasted here to make nDPI self-contained with no
  external dependencies
*/
#ifndef NDPI_MD5_H
#define NDPI_MD5_H
/* **************************************** */

typedef struct ndpi_MD5Context {
  uint32_t buf[4];
  uint32_t bits[2];
  unsigned char in[64];
} ndpi_MD5_CTX;

/* **************************************** */

static  void ndpi_MD5Init(ndpi_MD5_CTX *ctx);
static  void ndpi_MD5Update(ndpi_MD5_CTX *ctx, unsigned char const *buf, unsigned len);
static  void ndpi_MD5Final(unsigned char digest[16], ndpi_MD5_CTX *ctx);
#endif