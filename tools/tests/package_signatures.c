// Actual package authenticity and installation-gate bodies, with injected
// provider outcomes. The optional GnuTLS build also checks genuine signatures.
#include <assert.h>
#include <alloca.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#ifdef PACKAGE_REAL_CRYPTO
#include <gnutls/gnutls.h>
#include <gnutls/abstract.h>
#include <gnutls/crypto.h>
#include <gnutls/x509.h>
#endif
#define true 1
#define false 0
#define MAX_QPATH 128
#define countof(a) (sizeof(a)/sizeof((a)[0]))
#define stricmp strcasecmp
#define CON_ERROR ""
#define Con_Printf(...) ((void)0)
#define ishexcode(c) (((c)>='0' && (c)<='9') || ((c)>='a' && (c)<='f') || ((c)>='A' && (c)<='F'))
typedef unsigned char qbyte;
typedef int qboolean;
enum hashvalidation_e {VH_UNSUPPORTED, VH_AUTHORITY_UNKNOWN, VH_INCORRECT, VH_CORRECT};
/* ENGINE_FLAGS */
struct packagedep_s {struct packagedep_s *next; int dtype; char name[64];};
enum {DEP_FILE, DEP_CACHEFILE, DEP_REQUIRE};
enum {EXTRACT_COPY, EXTRACT_ZIP, EXTRACT_EXPLICITZIP};
typedef struct {
    char *mirror[8], *signature, *filesha512, *filesha1, *packprefix, *qhash;
    char gamedir[16]; uint64_t filesize; unsigned int flags; int extract;
    struct packagedep_s *deps;
} package_t;
typedef struct {
    unsigned int digestsize, contextsize;
    void (*init)(void *);
    void (*process)(void *, const void *, size_t);
    void (*terminate)(unsigned char *, void *);
} hashfunc_t;
// Only prefix hashing uses this host shim. The crypto build hashes the exact
// collected message with real SHA-512; policy tests use a fixed digest sentinel.
struct hashctx {size_t len; qbyte data[512];};
static void hash_init(void *p) {((struct hashctx *)p)->len = 0;}
static void hash_process(void *p, const void *data, size_t n) {
    struct hashctx *ctx = p; assert(n <= sizeof(ctx->data)-ctx->len);
    memcpy(ctx->data+ctx->len, data, n); ctx->len += n;
}
static void hash_end(qbyte *digest, void *p) {
    struct hashctx *ctx = p;
    assert(ctx->len > 64 && ctx->data[ctx->len-65] == 0);
#ifdef PACKAGE_REAL_CRYPTO
    assert(!gnutls_hash_fast(GNUTLS_DIG_SHA512, ctx->data, ctx->len, digest));
#else
    memset(digest, 0x7d, 64);
#endif
}
static hashfunc_t hash_sha2_512 = {64, sizeof(struct hashctx), hash_init, hash_process, hash_end};
static qbyte fake_cert[] = {1, 2, 3};
static qbyte *certificate = fake_cert;
static size_t certificate_size = sizeof(fake_cert);
static const qbyte *Auth_GetKnownCertificate(const char *name, size_t *n) {
    if (strcmp(name, "TestAuthority") && strcmp(name, "Spike")) return NULL;
    *n = certificate_size; return certificate;
}
static const char *COM_GetFileExtension(const char *name, const char *unused) {
    const char *ext = strrchr(name, '.'); (void)unused; return ext ? ext : "";
}
struct crypto_provider {
    enum hashvalidation_e (*VerifyHash)(const qbyte *, size_t, const qbyte *, size_t, const qbyte *, size_t);
};
static int calls, second_calls;
static enum hashvalidation_e outcome;
static enum hashvalidation_e verify(const qbyte *hash, size_t nh, const qbyte *cert,
                                    size_t nc, const qbyte *sig, size_t ns) {
    assert(hash && nh == 64 && cert == certificate && nc == certificate_size && sig && ns);
    calls++; return outcome;
}
static enum hashvalidation_e second_verify(const qbyte *h, size_t nh, const qbyte *c,
                                           size_t nc, const qbyte *s, size_t ns) {
    (void)h; (void)nh; (void)c; (void)nc; (void)s; (void)ns;
    second_calls++; return VH_CORRECT;
}
static struct crypto_provider first = {verify}, second = {second_verify};
static struct crypto_provider *cryptolib[] = {&first, NULL, NULL};
static const int cryptolib_count = 3;
/* ENGINE_FUNCTIONS */
#ifdef PACKAGE_REAL_CRYPTO
#define qgnutls_pubkey_init gnutls_pubkey_init
#define qgnutls_x509_crt_init gnutls_x509_crt_init
#define qgnutls_x509_crt_import gnutls_x509_crt_import
#define qgnutls_pubkey_import_x509 gnutls_pubkey_import_x509
#define qgnutls_pubkey_verify_hash2 gnutls_pubkey_verify_hash2
#define qgnutls_x509_crt_deinit gnutls_x509_crt_deinit
#define qgnutls_pubkey_deinit gnutls_pubkey_deinit
static int Init_GNUTLS(void) {return 1;}
/* ENGINE_CRYPTO */
#endif
static char digest_hex[129], sha1_hex[41];
static struct packagedep_s native = {NULL, DEP_FILE, "plugin.dylib"};
static const unsigned int signature_flags = DPF_SIGNATUREACCEPTED|DPF_SIGNATUREREJECTED|DPF_SIGNATUREUNKNOWN;
static package_t package(void) {
    package_t p = {0}; p.filesha512 = digest_hex; p.filesize = 3;
    p.signature = "TestAuthority:QUI="; p.deps = &native; return p;
}
static void expect(package_t p, enum hashvalidation_e parent, unsigned int want) {
    // Revalidation must clear all stale signature flags and keep unrelated bits.
    p.flags = signature_flags|DPF_CORRUPT; calls = second_calls = 0;
    PM_ValidateAuthenticity(&p, parent);
    assert((p.flags & signature_flags) == want && (p.flags & DPF_CORRUPT));
    assert(PM_SignatureOkay(&p) == (want == DPF_SIGNATUREACCEPTED));
}
static void policy(void) {
    // Mirror origin, mixed hosts, empty lists, and holes cannot grant trust.
    for (int mirrors = 0; mirrors < 5; mirrors++) {
        package_t p = package();
        if (mirrors) p.mirror[0] = ENGINEWEBSITE "/unsigned/plugin.dylib";
        if (mirrors == 2) p.mirror[1] = "https://example.invalid/plugin.dylib";
        if (mirrors == 3) for (int i = 1; i < 8; i++) p.mirror[i] = p.mirror[0];
        if (mirrors == 4) p.mirror[7] = p.mirror[0];
        outcome = VH_CORRECT; expect(p, VH_UNSUPPORTED, DPF_SIGNATUREACCEPTED); assert(calls == 1);
        outcome = VH_INCORRECT; expect(p, VH_CORRECT, DPF_SIGNATUREREJECTED); assert(calls == 1);
        outcome = VH_UNSUPPORTED; expect(p, VH_UNSUPPORTED, DPF_SIGNATUREUNKNOWN);
        cryptolib[0] = NULL; expect(p, VH_UNSUPPORTED, DPF_SIGNATUREUNKNOWN); assert(!calls);
        cryptolib[0] = &first;
        p.signature = "UnknownAuthority:QUI="; expect(p, VH_UNSUPPORTED, DPF_SIGNATUREUNKNOWN); assert(!calls);
        p.signature = NULL; expect(p, VH_UNSUPPORTED, 0); assert(!calls);
        expect(p, VH_CORRECT, DPF_SIGNATUREACCEPTED); // Authenticated catalog.
        expect(p, VH_INCORRECT, 0);
        p.filesize = 0; expect(p, VH_CORRECT, 0);
        p.filesize = 3; p.filesha512 = NULL; expect(p, VH_CORRECT, 0);
        p.filesha1 = sha1_hex; expect(p, VH_CORRECT, DPF_SIGNATUREACCEPTED);
    }
    package_t p = package(); outcome = VH_UNSUPPORTED;
    cryptolib[2] = &second; expect(p, VH_UNSUPPORTED, DPF_SIGNATUREACCEPTED); assert(calls == 1 && second_calls == 1);
    outcome = VH_INCORRECT; expect(p, VH_CORRECT, DPF_SIGNATUREREJECTED); assert(calls == 1 && !second_calls);
    cryptolib[2] = NULL;
    outcome = VH_CORRECT; p.signature = "QUI="; expect(p, VH_UNSUPPORTED, DPF_SIGNATUREACCEPTED); // Legacy Spike authority.
    // A valid prefix followed by junk, missing hashes, or invalid encodings must
    // fail even with a permissive provider or an authenticated parent catalog.
    char junk_hash[132]; snprintf(junk_hash, sizeof(junk_hash), "%sz", digest_hex);
    char *bad_hashes[] = {"", "00", "gg", junk_hash};
    for (size_t i = 0; i < countof(bad_hashes); i++) {
        p = package(); p.filesha512 = bad_hashes[i]; expect(p, VH_CORRECT, DPF_SIGNATUREREJECTED); assert(!calls);
        p.signature = NULL; expect(p, VH_CORRECT, DPF_SIGNATUREREJECTED); assert(!calls);
    }
    p = package(); p.filesha512 = NULL; expect(p, VH_CORRECT, DPF_SIGNATUREREJECTED); assert(!calls);
    p = package(); p.filesha1 = "bad"; expect(p, VH_CORRECT, DPF_SIGNATUREREJECTED); assert(!calls);
    char long_authority[180]; memset(long_authority, 'A', 150); strcpy(long_authority+150, ":QUI=");
    char long_signature[1600]; strcpy(long_signature, "TestAuthority:");
    memset(long_signature+14, 'A', 1500); long_signature[1514] = 0;
    char *bad_signatures[] = {"", ":QUI=", "TestAuthority:", "TestAuthority:A", "TestAuthority:A===",
        "TestAuthority:QU!I", "TestAuthority:QUI=AAAA", "TestAuthority:QUI==", "TestAuthority:QU I", long_authority, long_signature};
    for (size_t i = 0; i < countof(bad_signatures); i++) {
        p = package(); p.signature = bad_signatures[i]; expect(p, VH_CORRECT, DPF_SIGNATUREREJECTED); assert(!calls);
    }
    p = package(); p.packprefix = "id1/"; expect(p, VH_UNSUPPORTED, DPF_SIGNATUREACCEPTED); assert(calls == 1);
    // Keep existing unsigned data-only archive and manually installed policies.
    struct packagedep_s file = {NULL, DEP_FILE, "asset.pk3"};
    p = package(); p.signature = NULL; p.flags = 0; p.qhash = "1234";
    strcpy(p.gamedir, "id1"); p.deps = &file;
    assert(PM_SignatureOkay(&p)); strcpy(file.name, "asset.pak"); assert(PM_SignatureOkay(&p));
    strcpy(file.name, "plugin.so"); assert(!PM_SignatureOkay(&p));
    strcpy(file.name, "plugin.dll"); assert(!PM_SignatureOkay(&p));
    strcpy(file.name, "plugin.exe"); assert(!PM_SignatureOkay(&p));
    strcpy(file.name, "asset.pk3"); p.extract = EXTRACT_ZIP; assert(!PM_SignatureOkay(&p));
    p.extract = EXTRACT_COPY; p.qhash = NULL; assert(!PM_SignatureOkay(&p));
    file.dtype = DEP_CACHEFILE; assert(PM_SignatureOkay(&p));
    p.gamedir[0] = 0; assert(!PM_SignatureOkay(&p));
    p.flags = DPF_TRUSTED; assert(PM_SignatureOkay(&p));
    p.flags |= DPF_SIGNATUREREJECTED; assert(!PM_SignatureOkay(&p));
    p.flags = DPF_NATIVE; assert(PM_SignatureOkay(&p));
    puts("Package trust: mirror independence, provider failure, malformed metadata, catalogs and native/data gates passed");
}
static void encodings(void) {
    qbyte output[1024], input[1024]; char text[1400];
    for (size_t n = 1; n < sizeof(input); n++) {
        for (size_t i = 0; i < n; i++) input[i] = (qbyte)(i*37+n);
        size_t len = Base64_EncodeBlock(input, n, text, sizeof(text));
        assert(PM_DecodeSignature(text, output, sizeof(output)) == n && !memcmp(input, output, n));
        while (len && text[len-1] == '=') text[--len] = 0;
        for (size_t i = 0; i < len; i++) {if (text[i] == '+') text[i] = '-'; if (text[i] == '/') text[i] = '_';}
        assert(PM_DecodeSignature(text, output, sizeof(output)) == n && !memcmp(input, output, n));
        assert(PM_DecodeSignature(text, output, n) == 0);
    }
    for (int c = 0; c < 256; c++) {
        char invalid[] = "QU!I"; invalid[2] = (char)c;
        int allowed = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                      (c >= '0' && c <= '9') || c == '+' || c == '/' || c == '-' || c == '_';
        if (!allowed && c != 0) assert(!PM_DecodeSignature(invalid, output, sizeof(output)));
    }
    puts("Signature encodings: standard/URI-safe, optional padding, all byte values, and capacity boundaries passed");
}
#ifdef PACKAGE_REAL_CRYPTO
static void genuine_signatures(void) {
    gnutls_x509_privkey_t key; gnutls_privkey_t signing_key; gnutls_x509_crt_t cert;
    gnutls_datum_t pem, signature; qbyte raw_hash[64], serial[] = {1}; char signed_text[1500], encoded[1400];
    assert(!gnutls_global_init()); assert(!gnutls_x509_privkey_init(&key));
    assert(!gnutls_x509_privkey_generate(key, GNUTLS_PK_RSA, 2048, 0));
    assert(!gnutls_x509_crt_init(&cert)); assert(!gnutls_x509_crt_set_version(cert, 3));
    assert(!gnutls_x509_crt_set_serial(cert, serial, sizeof(serial)));
    assert(!gnutls_x509_crt_set_dn(cert, "CN=Fixture", NULL));
    assert(!gnutls_x509_crt_set_activation_time(cert, time(NULL)-3600));
    assert(!gnutls_x509_crt_set_expiration_time(cert, time(NULL)+86400));
    assert(!gnutls_x509_crt_set_key(cert, key));
    assert(!gnutls_x509_crt_sign2(cert, cert, key, GNUTLS_DIG_SHA256, 0));
    assert(!gnutls_x509_crt_export2(cert, GNUTLS_X509_FMT_PEM, &pem));
    certificate = pem.data; certificate_size = pem.size;
    assert(!gnutls_privkey_init(&signing_key));
    assert(!gnutls_privkey_import_x509(signing_key, key, GNUTLS_PRIVKEY_IMPORT_COPY));
    first.VerifyHash = GNUTLS_VerifyHash;
    for (int prefix = 0; prefix < 2; prefix++) {
        package_t p = package(); Base16_DecodeBlock(digest_hex, raw_hash, sizeof(raw_hash));
        if (prefix) {
            struct hashctx ctx; p.packprefix = "id1/";
            hash_init(&ctx); hash_process(&ctx, p.packprefix, strlen(p.packprefix));
            hash_process(&ctx, "\0", 1); hash_process(&ctx, raw_hash, sizeof(raw_hash)); hash_end(raw_hash, &ctx);
        }
        gnutls_datum_t hash = {raw_hash, sizeof(raw_hash)};
        assert(!gnutls_privkey_sign_hash(signing_key, GNUTLS_DIG_SHA512, 0, &hash, &signature));
        Base64_EncodeBlock(signature.data, signature.size, encoded, sizeof(encoded));
        snprintf(signed_text, sizeof(signed_text), "TestAuthority:%s", encoded); p.signature = signed_text;
        p.mirror[0] = ENGINEWEBSITE "/package"; expect(p, VH_UNSUPPORTED, DPF_SIGNATUREACCEPTED);
        digest_hex[0] = '0'; expect(p, VH_CORRECT, DPF_SIGNATUREREJECTED); digest_hex[0] = '4';
        if (prefix) {p.packprefix = "other/"; expect(p, VH_CORRECT, DPF_SIGNATUREREJECTED); p.packprefix = "id1/";}
        signed_text[14] = signed_text[14] == 'A' ? 'B' : 'A'; expect(p, VH_CORRECT, DPF_SIGNATUREREJECTED);
        gnutls_free(signature.data);
    }
    gnutls_privkey_deinit(signing_key); gnutls_x509_crt_deinit(cert);
    gnutls_x509_privkey_deinit(key); gnutls_free(pem.data); gnutls_global_deinit();
    puts("Real engine GnuTLS verifier: genuine RSA/SHA-512 signatures accepted; changed hashes, signatures and archive prefixes rejected");
}
#endif
int main(void) {
    memset(digest_hex, '4', 128); digest_hex[128] = 0;
    memset(sha1_hex, 'b', 40); sha1_hex[40] = 0;
    policy(); encodings();
#ifdef PACKAGE_REAL_CRYPTO
    genuine_signatures();
#endif
    return 0;
}
