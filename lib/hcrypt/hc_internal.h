// What hcrypt.c and inflate.c took from the DynastyOS kernel, made on Windows,
// and the names: the code calls hc_*, a program calls the names of <hcrypt.h>.
#pragma once
#include <windows.h>
#include <string.h>
#include <stdlib.h>
#include <intrin.h>

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;
typedef long long i64;

#define PAGE_SIZE 4096ull
#define kmalloc(n) malloc((size_t)(n))
#define kfree(p) free(p)

// hc_host.c: memory by pages, the lanes of Argon2id on threads, entropy, the key file
#define page_alloc hc_page_alloc
#define page_free hc_page_free
#define smp_parallel hc_smp_parallel
#define timer_ticks hc_timer_ticks
#define mouse_seq hc_mouse_seq
#define fs_read hc_fs_read
#define fs_write hc_fs_write
void *page_alloc(u64 count);
void page_free(void *addr, u64 count);
void smp_parallel(int n, void (*fn)(int i, void *ctx), void *ctx);
u64 timer_ticks(void);
u64 mouse_seq(void);
const char *fs_read(const char *name, u64 *size);     // a malloc'ed copy; 0 = no such file
int fs_write(const char *name, const char *data, u64 size);

// inflate.c
#define inflate_buf hc_inflate_buf
#define inflate_raw hc_inflate_raw
u8 *inflate_buf(const u8 *src, u64 len, u64 *outlen);
u8 *inflate_raw(const u8 *src, u64 len, u64 hint, u64 *outlen, int *bad);

// deflate in pieces
#define HC_DEFLATE_PIECE (1 << 20)
typedef struct HcDeflate HcDeflate;

// the names a program sees (hcrypt.h)
#define hc_random_bytes random_bytes
#define hc_sha256 sha256
#define hc_hmac_sha256 hmac_sha256
#define hc_pbkdf2_sha256 pbkdf2_sha256
#define hc_sha256_begin sha256_begin
#define hc_sha256_update sha256_update
#define hc_sha256_end sha256_end
#define hc_hmac_sha256_begin hmac_sha256_begin
#define hc_hmac_sha256_update hmac_sha256_update
#define hc_hmac_sha256_end hmac_sha256_end
#define hc_secure_equal secure_equal
#define hc_crc32 crc32
#define hc_crc32_update crc32_update
#define hc_blake2b blake2b
#define hc_argon2id argon2id
#define hc_aes256gcm_encrypt aes256gcm_encrypt
#define hc_aes256gcm_decrypt aes256gcm_decrypt
#define hc_aes256cbc_encrypt aes256cbc_encrypt
#define hc_aes256cbc_decrypt aes256cbc_decrypt
#define hc_aes256cbc_encrypt_blocks aes256cbc_encrypt_blocks
#define hc_aes256cbc_decrypt_blocks aes256cbc_decrypt_blocks
#define hc_aes256ctr_le aes256ctr_le
#define hc_chacha20_xor chacha20_xor
#define hc_poly1305 poly1305
#define hc_xchacha20poly1305_encrypt xchacha20poly1305_encrypt
#define hc_xchacha20poly1305_decrypt xchacha20poly1305_decrypt
#define hc_aes128_block aes128_block
#define hc_aes_cmac aes_cmac
#define hc_p256_keypair p256_keypair
#define hc_p256_public p256_public
#define hc_p256_shared p256_shared
#define hc_p256_keygen p256_keygen
#define hc_p256_sign p256_sign
#define hc_p256_verify p256_verify
#define hc_gzip_pack gzip_pack
#define hc_gzip_unpack gzip_unpack
#define hc_gzip_unpacked_size gzip_unpacked_size
#define hc_deflate_new deflate_new
#define hc_deflate_piece deflate_piece
#define hc_deflate_free deflate_free
#define hc_local_seal local_seal
#define hc_local_unseal local_unseal

// called before their definitions in hcrypt.c (the kernel declared them in kernel.h)
void hc_random_bytes(void *buf, int n);
void hc_aes256gcm_encrypt(const unsigned char key[32], const unsigned char nonce[12], const void *aad, int aadlen,
                          const void *in, long long n, void *out, unsigned char tag[16]);
int hc_aes256gcm_decrypt(const unsigned char key[32], const unsigned char nonce[12], const void *aad, int aadlen,
                         const void *in, long long n, const unsigned char tag[16], void *out);
void hc_sha256(const void *data, long long n, unsigned char out[32]);
void hc_hmac_sha256(const void *key, int keylen, const void *data, long long n, unsigned char out[32]);
