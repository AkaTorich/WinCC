// hcrypt (lib\hcrypt): every function against the published test vectors (FIPS, NIST,
// RFC), the rest by a round trip. Returns the number of failed checks.
// With -DHCRYPT_TEST it runs twice: the processor's AES-NI path, then the tables.
#include <hcrypt.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int g_bad;

static void unhex(const char *h, unsigned char *out)
{
    for (int i = 0; h[2 * i] && h[2 * i + 1]; i++) {
        unsigned v;
        sscanf(h + 2 * i, "%2x", &v);
        out[i] = (unsigned char)v;
    }
}

static void check(const char *name, const unsigned char *got, const char *want_hex)
{
    unsigned char want[128];
    int n = (int)strlen(want_hex) / 2;
    unhex(want_hex, want);
    if (memcmp(got, want, (size_t)n) == 0) return;
    printf("%s: ", name);
    for (int i = 0; i < n; i++) printf("%02x", got[i]);
    printf("\n    want %s\n", want_hex);
    g_bad++;
}

static void ok(const char *name, int cond)
{
    if (!cond) { printf("%s: no\n", name); g_bad++; }
}

static void run(void)
{
    unsigned char out[128], k[64], iv[16], in[64], tag[16];

    // SHA-256, HMAC, PBKDF2, CRC-32
    sha256("abc", 3, out);
    check("sha256", out, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    sha256_ctx sc;
    sha256_begin(&sc); sha256_update(&sc, "a", 1); sha256_update(&sc, "bc", 2); sha256_end(&sc, out);
    check("sha256 by pieces", out, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    hmac_sha256("Jefe", 4, "what do ya want for nothing?", 28, out);
    check("hmac_sha256", out, "5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843");
    hmac_sha256_ctx hc;
    hmac_sha256_begin(&hc, "Jefe", 4); hmac_sha256_update(&hc, "what do ya ", 11);
    hmac_sha256_update(&hc, "want for nothing?", 17); hmac_sha256_end(&hc, out);
    check("hmac_sha256 by pieces", out, "5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843");
    pbkdf2_sha256("password", 8, "salt", 4, 1, out, 32);
    check("pbkdf2_sha256", out, "120fb6cffcf8b32c43e7225256c4f837a86548c92ccc35480805987cb70be17b");
    unsigned int crc = crc32("123456789", 9);
    ok("crc32", crc == 0xcbf43926u);
    ok("crc32_update", crc32_update(crc32_update(0, "1234", 4), "56789", 5) == 0xcbf43926u);
    ok("secure_equal", secure_equal("abcd", "abcd", 4) == 1 && secure_equal("abcd", "abce", 4) == 0);

    // BLAKE2b, Argon2id (RFC 9106: 4 lanes on 4 threads)
    blake2b(out, 64, 0, 0, "abc", 3);
    check("blake2b", out, "ba80a53f981c4d0d6a2797b69f12f6e94c212f14685ac4b74b12bb6fdbffa2d1"
                          "7d87c5392aab792dc252d5de4533cc9518d38aa8dbf1925ab92386edd4009923");
    unsigned char pw[32], salt[16], secret[8], ad[12];
    memset(pw, 1, 32); memset(salt, 2, 16); memset(secret, 3, 8); memset(ad, 4, 12);
    ok("argon2id runs", argon2id(out, 32, pw, 32, salt, 16, secret, 8, ad, 12, 3, 32, 4) == 0);
    check("argon2id", out, "0d640df58d78766c08c037a34a8b53c9d01ef0452d75b65eb52520e96b01e659");

    // AES-128 (FIPS-197), AES-CMAC (RFC 4493)
    unhex("000102030405060708090a0b0c0d0e0f", k);
    unhex("00112233445566778899aabbccddeeff", in);
    aes128_block(k, in, out);
    check("aes128_block", out, "69c4e0d86a7b0430d8cdb78070b4c55a");
    unhex("2b7e151628aed2a6abf7158809cf4f3c", k);
    aes_cmac(k, "", 0, out);
    check("aes_cmac", out, "bb1d6929e95937287fa37d129b756746");

    // AES-256-GCM (NIST cases 13 and 14)
    memset(k, 0, 32); memset(iv, 0, 12); memset(in, 0, 16);
    aes256gcm_encrypt(k, iv, 0, 0, in, 0, out, tag);
    check("aes256gcm tag (empty)", tag, "530f8afbc74536b9a963b4f1c4cb738b");
    aes256gcm_encrypt(k, iv, 0, 0, in, 16, out, tag);
    check("aes256gcm", out, "cea7403d4d606b6e074ec5d3baf39d18");
    check("aes256gcm tag", tag, "d0d1c8a799996bf0265b98b5d48ab919");
    ok("aes256gcm_decrypt", aes256gcm_decrypt(k, iv, 0, 0, out, 16, tag, out) == 1 && out[0] == 0 && out[15] == 0);
    tag[0] ^= 1;
    ok("aes256gcm forged", aes256gcm_decrypt(k, iv, 0, 0, out, 16, tag, out) == 0);

    // AES-256-CBC (SP 800-38A F.2.5), PKCS7 round trip
    unhex("603deb1015ca71be2b73aef0857d77811f352c073b6108d72d9810a30914dff4", k);
    unhex("000102030405060708090a0b0c0d0e0f", iv);
    unhex("6bc1bee22e409f96e93d7e117393172a", in);
    ok("aes256cbc_encrypt_blocks", aes256cbc_encrypt_blocks(k, iv, in, 16, out) == 16);
    check("aes256cbc", out, "f58c4c04d6e5f1ba779eabfb5f7bfbd6");
    unhex("000102030405060708090a0b0c0d0e0f", iv);
    long long cn = aes256cbc_encrypt(k, iv, "Hello, WinCC!", 13, out);
    unsigned char back[64];
    ok("aes256cbc round trip", cn == 16 && aes256cbc_decrypt(k, iv, out, cn, back) == 13 && !memcmp(back, "Hello, WinCC!", 13));

    // ChaCha20 (RFC 8439 2.4.2), Poly1305 (2.5.2), XChaCha20-Poly1305 round trip
    for (int i = 0; i < 32; i++) k[i] = (unsigned char)i;
    unhex("000000000000004a00000000", iv);
    chacha20_xor(k, iv, 1, "Ladies and Gentlemen of the class of '99: If I could offer you o", 64, out);
    check("chacha20_xor", out, "6e2e359a2568f98041ba0728dd0d6981e97e7aec1d4360c20a27afccfd9fae0b"
                               "f91b65c5524733ab8f593dabcd62b3571639d624e65152ab8f530c359f0861d8");
    unhex("85d6be7857556d337f4452fe42d506a80103808afb0db2fd4abff6af4149f51b", k);
    poly1305(k, "Cryptographic Forum Research Group", 34, out);
    check("poly1305", out, "a8061dc1305136c6c22b8baf0c0127a9");
    unsigned char xn[24];
    for (int i = 0; i < 24; i++) xn[i] = (unsigned char)(0x40 + i);
    xchacha20poly1305_encrypt(k, xn, "ad", 2, "secret text", 11, out, tag);
    ok("xchacha20poly1305", xchacha20poly1305_decrypt(k, xn, "ad", 2, out, 11, tag, back) == 1 && !memcmp(back, "secret text", 11));
    ok("xchacha20poly1305 wrong ad", xchacha20poly1305_decrypt(k, xn, "AD", 2, out, 11, tag, back) == 0);

    // P-256: the generator from 1, ECDSA by RFC 6979 (A.2.5, "sample"), ECDH
    unsigned char priv[32], pub[64], sig[64], h[32], pub2[64], priv2[32], s1[32], s2[32];
    memset(priv, 0, 32); priv[31] = 1;
    ok("p256_public", p256_public(priv, pub));
    check("p256 G", pub, "6b17d1f2e12c4247f8bce6e563a440f277037d812deb33a0f4a13945d898c296"
                         "4fe342e2fe1a7f9b8ee7eb4a7c0f9e162bce33576b315ececbb6406837bf51f5");
    unhex("c9afa9d845ba75166b5c215767b1d6934e50c3db36e89b127b8a622b120f6721", priv);
    sha256("sample", 6, h);
    ok("p256_sign", p256_sign(priv, h, sig));
    check("p256 r", sig, "efd48b2aacb6a8fd1140dd9cd45e81d69d2c877b56aaf991c34d0ea84eaf3716");
    p256_public(priv, pub);
    ok("p256_verify", p256_verify(pub, h, sig) == 1);
    h[0] ^= 1;
    ok("p256_verify wrong", p256_verify(pub, h, sig) == 0);
    ok("p256_keygen", p256_keygen(priv, pub) && p256_sign(priv, h, sig) && p256_verify(pub, h, sig));
    ok("p256_keypair", p256_keypair(priv, pub) && p256_keypair(priv2, pub2));
    ok("p256_shared", p256_shared(priv, pub2, s1) && p256_shared(priv2, pub, s2) && !memcmp(s1, s2, 32));

    // random bytes
    unsigned char r1[32], r2[32];
    random_bytes(r1, 32); random_bytes(r2, 32);
    ok("random_bytes", memcmp(r1, r2, 32) != 0);

    // gzip, deflate by pieces
    int n = 100000;
    char *text = malloc((size_t)n), *gz = malloc((size_t)n + n / 8 + 64), *un = malloc((size_t)n);
    for (int i = 0; i < n; i++) text[i] = "the quick brown fox "[i % 20] + (char)(i / 5000);
    long long g = gzip_pack(text, n, gz, n + n / 8 + 64);
    ok("gzip_pack", g > 0 && g < n / 4);
    ok("gzip_unpacked_size", gzip_unpacked_size(gz, g) == n);
    ok("gzip_unpack", gzip_unpack(gz, g, un, n) == n && !memcmp(un, text, (size_t)n));
    deflate_ctx *z = deflate_new();
    ok("deflate_new", z != 0);
    if (z) {
        long long a = deflate_piece(z, text, n / 2, 0, gz + 10, n);
        long long b = a < 0 ? -1 : deflate_piece(z, text + n / 2, n - n / 2, 1, gz + 10 + a, n);
        deflate_free(z);
        ok("deflate_piece", a >= 0 && b > 0);
        if (a >= 0 && b > 0) {              // the pieces as one gzip: header, stream, crc, size
            static const unsigned char hdr[10] = { 0x1f, 0x8b, 8, 0, 0, 0, 0, 0, 0, 255 };
            memcpy(gz, hdr, 10);
            unsigned char *t = (unsigned char *)gz + 10 + a + b;
            unsigned int c = crc32(text, n);
            for (int i = 0; i < 4; i++) { t[i] = (unsigned char)(c >> 8 * i); t[4 + i] = (unsigned char)(n >> 8 * i); }
            ok("deflate pieces unpack", gzip_unpack(gz, 10 + a + b + 8, un, n) == n && !memcmp(un, text, (size_t)n));
        }
    }
    free(text); free(gz); free(un);

    // local_seal
    unsigned char sealed[64];
    long long sn = local_seal("password", 8, sealed, sizeof(sealed));
    ok("local_seal", sn == 36);
    ok("local_unseal", local_unseal(sealed, sn, back, sizeof(back)) == 8 && !memcmp(back, "password", 8));
    sealed[20] ^= 1;
    ok("local_unseal damaged", local_unseal(sealed, sn, back, sizeof(back)) == -1);
}

#ifdef HCRYPT_TEST
void hc_test_soft(int on);
#endif

int main(void)
{
    run();
#ifdef HCRYPT_TEST
    hc_test_soft(1);
    run();
#endif
    if (g_bad) printf("failed: %d\n", g_bad);
    return g_bad;
}
