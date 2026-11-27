// hcrypt.h — шифры, хеши и сжатие для программ на Си: SHA-256, HMAC, PBKDF2, BLAKE2b, Argon2id,
// AES-256-GCM/CBC, ChaCha20-Poly1305, подписи ECDSA P-256, gzip. Байт в байт как .NET-код «Гермеса»
// (Windows CNG, DPAPI, GZipStream). Библиотека — исходниками в lib\hcrypt, wcc подключает её сам.
// AES и GCM идут командами процессора (AES-NI, PCLMULQDQ), если они есть, иначе таблицами.
//
//   #include <hcrypt.h>
//   unsigned char key[32], nonce[12], tag[16];
//   random_bytes(key, 32); random_bytes(nonce, 12);
//   aes256gcm_encrypt(key, nonce, 0, 0, text, n, text, tag);          // на месте
//   if (!aes256gcm_decrypt(key, nonce, 0, 0, text, n, tag, text)) ... // подделка или не тот ключ
#pragma once
#ifndef HCRYPT_INTERNAL
#pragma comment(lib, "hcrypt")
#endif

// --- случайность, хеши, выработка ключей ---
void random_bytes(void *buf, int n);   // криптостойкие случайные байты (rdseed/rdrand + часы через SHA-256)
void sha256(const void *data, long long n, unsigned char out[32]);
void hmac_sha256(const void *key, int keylen, const void *data, long long n, unsigned char out[32]);
void pbkdf2_sha256(const void *pw, int pwlen, const void *salt, int saltlen, int iterations, void *out, int outlen);   // как Rfc2898DeriveBytes(SHA256); 64 байта = два GetBytes(32)
unsigned int crc32(const void *data, long long n);   // как в gzip и zip
unsigned int crc32_update(unsigned int crc, const void *data, long long n);   // по частям: crc 0 перед первой
// --- хеш и HMAC по частям (файлы больше памяти): begin, сколько угодно update, end ---
typedef struct { unsigned long long s[16]; } sha256_ctx;        // состояние SHA-256 (внутрь не заглядывать)
typedef struct { unsigned long long s[32]; } hmac_sha256_ctx;   // состояние HMAC-SHA256
void sha256_begin(sha256_ctx *c);
void sha256_update(sha256_ctx *c, const void *data, long long n);
void sha256_end(sha256_ctx *c, unsigned char out[32]);
void hmac_sha256_begin(hmac_sha256_ctx *c, const void *key, int keylen);
void hmac_sha256_update(hmac_sha256_ctx *c, const void *data, long long n);
void hmac_sha256_end(hmac_sha256_ctx *c, unsigned char out[32]);   // состояние стирается
int secure_equal(const void *a, const void *b, int n);   // 1 = равны; время не зависит от места различия (для HMAC и тегов)
void blake2b(void *out, int outlen, const void *key, int keylen, const void *data, long long n);   // outlen 1..64, keylen 0..64 (0 = без ключа)
int argon2id(void *out, int outlen, const void *pw, int pwlen, const void *salt, int saltlen,
             const void *secret, int secretlen, const void *ad, int adlen,
             int passes, int memory_kib, int lanes);   // 0 = готово, -1 = неверные параметры или не хватило памяти; полосы — каждая в своём потоке
// --- шифры (in и out могут совпадать) ---
void aes256gcm_encrypt(const unsigned char key[32], const unsigned char nonce[12], const void *aad, int aadlen,
                       const void *in, long long n, void *out, unsigned char tag[16]);
int aes256gcm_decrypt(const unsigned char key[32], const unsigned char nonce[12], const void *aad, int aadlen,
                      const void *in, long long n, const unsigned char tag[16], void *out);   // 1 = тег верен; иначе 0 и out не тронут
long long aes256cbc_encrypt(const unsigned char key[32], const unsigned char iv[16], const void *in, long long n, void *out);   // PKCS7: вернёт (n/16+1)*16
long long aes256cbc_decrypt(const unsigned char key[32], const unsigned char iv[16], const void *in, long long n, void *out);   // длина без добивки или -1 (неверная добивка)
long long aes256cbc_encrypt_blocks(const unsigned char key[32], unsigned char iv[16], const void *in, long long n, void *out);   // CBC целыми блоками без добивки: n кратно 16;
long long aes256cbc_decrypt_blocks(const unsigned char key[32], unsigned char iv[16], const void *in, long long n, void *out);   //   iv на выходе — последний блок шифра (с него идёт следующий кусок); n или -1
long long aes256ctr_le(const unsigned char key[32], unsigned char ctr[16], const void *in, long long n, void *out);   // CTR как в WinZip AES (счётчик little-endian); ctr на выходе — следующий
void chacha20_xor(const unsigned char key[32], const unsigned char nonce[12], unsigned int counter, const void *in, long long n, void *out);   // RFC 8439
void poly1305(const unsigned char key[32], const void *data, long long n, unsigned char tag[16]);   // одноразовый ключ!
void xchacha20poly1305_encrypt(const unsigned char key[32], const unsigned char nonce[24], const void *aad, int aadlen,
                               const void *in, long long n, void *out, unsigned char tag[16]);   // Гермес (Уа) пишет cipher || tag
int xchacha20poly1305_decrypt(const unsigned char key[32], const unsigned char nonce[24], const void *aad, int aadlen,
                              const void *in, long long n, const unsigned char tag[16], void *out);   // 1 = тег верен
// --- подписи ECDSA P-256 + SHA-256 (перстень, как ECDsaCng) ---
int p256_keygen(unsigned char priv[32], unsigned char pub[64]);   // pub = X||Y; 1 = готово
int p256_sign(const unsigned char priv[32], const unsigned char hash[32], unsigned char sig[64]);   // hash = sha256(данных); sig = r||s
int p256_verify(const unsigned char pub[64], const unsigned char hash[32], const unsigned char sig[64]);   // 1 = подпись верна
// --- общий ключ ECDH P-256 и AES-128 / AES-CMAC (сопряжение Bluetooth LE); числа big-endian, pub = X||Y ---
int p256_keypair(unsigned char priv[32], unsigned char pub[64]);   // случайная пара; 1 = готово
int p256_public(const unsigned char priv[32], unsigned char pub[64]);   // открытый ключ по закрытому; 0 = priv не от 1 до n-1
int p256_shared(const unsigned char priv[32], const unsigned char peer[64], unsigned char out[32]);   // out = X точки priv*peer; 0 = peer не на кривой
void aes128_block(const unsigned char key[16], const unsigned char in[16], unsigned char out[16]);   // один блок AES-128 (шифрование)
void aes_cmac(const unsigned char key[16], const void *msg, long long n, unsigned char out[16]);   // AES-CMAC (RFC 4493), n — сколько угодно, и 0
// --- сжатие gzip ---
long long gzip_pack(const void *in, long long n, void *out, long long cap);     // длина .gz или -1; cap >= n + n/8 + 64 хватает всегда
long long gzip_unpack(const void *in, long long n, void *out, long long cap);   // длина распакованного или -1 (не gzip, порча, мало места)
long long gzip_unpacked_size(const void *in, long long n);   // размер распакованного из хвоста .gz (или -1)
// поток deflate кусками (запись ZIP любого размера): кусок до DEFLATE_PIECE байт, last = 1 у последнего;
// вернёт длину сжатого куска или -1; cap >= n + n/8 + 64 хватает
#define DEFLATE_PIECE (1 << 20)
typedef struct HcDeflate deflate_ctx;
deflate_ctx *deflate_new(void);
long long deflate_piece(deflate_ctx *z, const void *in, long long n, int last, void *out, long long cap);
void deflate_free(deflate_ctx *z);
// --- печать этой установки (вместо DPAPI): ключ в %LOCALAPPDATA%\hcrypt\hcrypt.key ---
long long local_seal(const void *in, long long n, void *out, long long cap);     // вернёт n + 28 или -1
long long local_unseal(const void *in, long long n, void *out, long long cap);   // вернёт n - 28 или -1 (не этой машины и пользователя или порча)
