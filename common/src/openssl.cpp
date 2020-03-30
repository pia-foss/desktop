// Copyright (c) 2020 Private Internet Access, Inc.
//
// This file is part of the Private Internet Access Desktop Client.
//
// The Private Internet Access Desktop Client is free software: you can
// redistribute it and/or modify it under the terms of the GNU General Public
// License as published by the Free Software Foundation, either version 3 of
// the License, or (at your option) any later version.
//
// The Private Internet Access Desktop Client is distributed in the hope that
// it will be useful, but WITHOUT ANY WARRANTY; without even the implied
// warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with the Private Internet Access Desktop Client.  If not, see
// <https://www.gnu.org/licenses/>.

#include "common.h"
#line SOURCE_FILE("openssl.cpp")

#include "openssl.h"
#include "path.h"

#include <QDir>
#include <QLibrary>
#include <QSslSocket>

#include <cctype>

#if defined(Q_OS_WIN)
    #if defined(_WIN64)
        static const QString libsslName = QStringLiteral("libssl-1_1-x64.dll");
        static const QString libcryptoName = QStringLiteral("libcrypto-1_1-x64.dll");
    #else
        static const QString libsslName = QStringLiteral("libssl-1_1.dll");
        static const QString libcryptoName = QStringLiteral("libcrypto-1_1.dll");
    #endif
#elif defined(Q_OS_MACOS)
        static const QString libsslName = QStringLiteral("libssl.1.1.dylib");
        static const QString libcryptoName = QStringLiteral("libcrypto.1.1.dylib");
#elif defined(Q_OS_LINUX)
        static const QString libsslName = QStringLiteral("libssl.so.1.1");
        static const QString libcryptoName = QStringLiteral("libcrypto.so.1.1");
#endif

// libssl

struct EVP_MD_CTX;
struct EVP_PKEY_CTX;
struct EVP_MD;
struct ENGINE;
struct BIO;
struct EVP_PKEY;
struct X509;
struct X509_STORE;
struct X509_STORE_CTX;
struct stack_st;
struct stack_st_X509;
typedef int (*pem_password_cb)(char* buf, int size, int rwflag, void* u);
typedef int (*OPENSSL_sk_compfunc)(const void *, const void *);

enum
{
    EVP_PKEY_X25519 = 1034, // NID_X25519
};

static void (*CRYPTO_free)(void*) = nullptr;

static void (*ERR_print_errors_cb)(int (*cb)(const char* str, size_t len, void* u), void* u) = nullptr;

static BIO* (*BIO_new_mem_buf)(const void *buf, int len) = nullptr;
static void (*BIO_free)(BIO* bp) = nullptr;

static EVP_PKEY* (*PEM_read_bio_PUBKEY)(BIO* bp, EVP_PKEY** x, pem_password_cb* cb, void* u);
static X509* (*PEM_read_bio_X509)(BIO*, X509**, pem_password_cb*, void*) = nullptr;
static X509* (*PEM_read_bio_X509_AUX)(BIO*, X509**, pem_password_cb*, void*) = nullptr;

static EVP_MD_CTX* (*EVP_MD_CTX_new)() = nullptr;
static void (*EVP_MD_CTX_free)(EVP_MD_CTX* ctx) = nullptr;

static const EVP_MD* (*EVP_sha1)() = nullptr;
static const EVP_MD* (*EVP_sha256)() = nullptr;

static void (*EVP_PKEY_free)(EVP_PKEY* pkey) = nullptr;

static int (*EVP_DigestVerifyInit)(EVP_MD_CTX* ctx, EVP_PKEY_CTX** pctx, const EVP_MD* type, ENGINE* e, EVP_PKEY* pkey) = nullptr;
static int (*EVP_DigestUpdate)(EVP_MD_CTX* ctx, const void* d, size_t cnt) = nullptr;
static int (*EVP_DigestVerifyFinal)(EVP_MD_CTX* ctx, const unsigned char* sig, size_t siglen) = nullptr;

static void (*X509_free)(X509*) = nullptr;
static int (*X509_check_host)(X509*, const char *, size_t, unsigned int, char**) = nullptr;

static X509_STORE* (*X509_STORE_new)(void) = nullptr;
static void (*X509_STORE_free)(X509_STORE*) = nullptr;
static int (*X509_STORE_add_cert)(X509_STORE*, X509*) = nullptr;

static X509_STORE_CTX* (*X509_STORE_CTX_new)(void) = nullptr;
static void (*X509_STORE_CTX_free)(X509_STORE_CTX *) = nullptr;
static int (*X509_STORE_CTX_init)(X509_STORE_CTX*, X509_STORE*, X509*, stack_st_X509*) = nullptr;
static int (*X509_STORE_CTX_get_error)(X509_STORE_CTX*) = nullptr;
static int (*X509_STORE_CTX_get_error_depth)(X509_STORE_CTX*) = nullptr;

static int (*X509_verify_cert)(X509_STORE_CTX*) = nullptr;
static const char * (*X509_verify_cert_error_string)(long) = nullptr;

static X509* (*d2i_X509)(X509**, unsigned char **, long) = nullptr;

static EVP_PKEY_CTX* (*EVP_PKEY_CTX_new_id)(int, ENGINE*) = nullptr;
static void (*EVP_PKEY_CTX_free)(EVP_PKEY_CTX*) = nullptr;

static int (*EVP_PKEY_keygen_init)(EVP_PKEY_CTX *) = nullptr;
static int (*EVP_PKEY_keygen)(EVP_PKEY_CTX*, EVP_PKEY**) = nullptr;
static int (*EVP_PKEY_get_raw_private_key)(const EVP_PKEY*, unsigned char *, size_t *) = nullptr;
static int (*EVP_PKEY_get_raw_public_key)(const EVP_PKEY*, unsigned char *, size_t *) = nullptr;

static stack_st* (*OPENSSL_sk_new_reserve)(OPENSSL_sk_compfunc, int);
static int (*OPENSSL_sk_push)(stack_st*, const void*) = nullptr;
static void (*OPENSSL_sk_free)(stack_st*) = nullptr;

static bool checkOpenSSL()
{
    static bool attempted = false, successful = false;

    if (successful)
        return true;
    if (attempted)
        return false;

    attempted = true;

    const auto &libcryptoPath = Path::LibraryDir / libcryptoName;
    // The library is never unloaded (note that the QLibrary dtor does not
    // unload the library)
    QLibrary libcrypto{libcryptoPath};

    if(!libcrypto.load())
    {
        qWarning() << "Unable to load libcrypto from" << libcryptoPath;
        return false;
    }

    qInfo() << "Loaded libcrypto from" << libcrypto.fileName();

#define TRY_RESOLVE_OPENSSL_FUNCTION(name) \
    (name = reinterpret_cast<decltype(name)>(libcrypto.resolve(#name)))
#define RESOLVE_OPENSSL_FUNCTION(name) \
    if (!TRY_RESOLVE_OPENSSL_FUNCTION(name)) { qError() << "Unable to resolve symbol" << #name; return false; } else ((void)0)

    RESOLVE_OPENSSL_FUNCTION(CRYPTO_free);

    RESOLVE_OPENSSL_FUNCTION(ERR_print_errors_cb);

    RESOLVE_OPENSSL_FUNCTION(BIO_new_mem_buf);
    RESOLVE_OPENSSL_FUNCTION(BIO_free);

    RESOLVE_OPENSSL_FUNCTION(PEM_read_bio_PUBKEY);
    RESOLVE_OPENSSL_FUNCTION(PEM_read_bio_X509);
    RESOLVE_OPENSSL_FUNCTION(PEM_read_bio_X509_AUX);

    RESOLVE_OPENSSL_FUNCTION(EVP_MD_CTX_new);
    RESOLVE_OPENSSL_FUNCTION(EVP_MD_CTX_free);

    RESOLVE_OPENSSL_FUNCTION(EVP_sha1);
    RESOLVE_OPENSSL_FUNCTION(EVP_sha256);

    RESOLVE_OPENSSL_FUNCTION(EVP_PKEY_free);

    RESOLVE_OPENSSL_FUNCTION(EVP_DigestVerifyInit);
    RESOLVE_OPENSSL_FUNCTION(EVP_DigestUpdate);
    RESOLVE_OPENSSL_FUNCTION(EVP_DigestVerifyFinal);

    RESOLVE_OPENSSL_FUNCTION(X509_free);
    RESOLVE_OPENSSL_FUNCTION(X509_check_host);

    RESOLVE_OPENSSL_FUNCTION(X509_STORE_new);
    RESOLVE_OPENSSL_FUNCTION(X509_STORE_free);
    RESOLVE_OPENSSL_FUNCTION(X509_STORE_add_cert);

    RESOLVE_OPENSSL_FUNCTION(X509_STORE_CTX_new);
    RESOLVE_OPENSSL_FUNCTION(X509_STORE_CTX_free);
    RESOLVE_OPENSSL_FUNCTION(X509_STORE_CTX_init);
    RESOLVE_OPENSSL_FUNCTION(X509_STORE_CTX_get_error);
    RESOLVE_OPENSSL_FUNCTION(X509_STORE_CTX_get_error_depth);

    RESOLVE_OPENSSL_FUNCTION(X509_verify_cert);
    RESOLVE_OPENSSL_FUNCTION(X509_verify_cert_error_string);

    RESOLVE_OPENSSL_FUNCTION(d2i_X509);

    RESOLVE_OPENSSL_FUNCTION(EVP_PKEY_CTX_new_id);
    RESOLVE_OPENSSL_FUNCTION(EVP_PKEY_CTX_free);

    RESOLVE_OPENSSL_FUNCTION(EVP_PKEY_keygen_init);
    RESOLVE_OPENSSL_FUNCTION(EVP_PKEY_keygen);
    RESOLVE_OPENSSL_FUNCTION(EVP_PKEY_get_raw_private_key);
    RESOLVE_OPENSSL_FUNCTION(EVP_PKEY_get_raw_public_key);

    RESOLVE_OPENSSL_FUNCTION(OPENSSL_sk_new_reserve);
    RESOLVE_OPENSSL_FUNCTION(OPENSSL_sk_free);
    RESOLVE_OPENSSL_FUNCTION(OPENSSL_sk_push);

#undef RESOLVE_OPENSSL_FUNCTION
#undef TRY_RESOLVE_OPENSSL_FUNCTION

    successful = true;

    return true;
}

static const EVP_MD* getMD(QCryptographicHash::Algorithm algorithm)
{
    switch (algorithm)
    {
    case QCryptographicHash::Sha1:
        return EVP_sha1();
    case QCryptographicHash::Sha256:
        return EVP_sha256();
    default:
        // Note: See other EVP_*() functions for supporting other hash algorithms.
        qError() << "Unsupported hash algorithm";
        return nullptr;
    }
}

static EVP_PKEY* createPublicKeyFromPem(const QByteArray& pem)
{
    if (pem.isEmpty())
        return nullptr;
    BIO *bio = BIO_new_mem_buf(const_cast<char*>(pem.data()), pem.size());
    EVP_PKEY* result = PEM_read_bio_PUBKEY(bio, nullptr, nullptr, nullptr);
    BIO_free(bio);
    return result;
}

bool verifySignature(const QByteArray& publicKeyPem, const QByteArray& signature, const QByteArray& data, QCryptographicHash::Algorithm hashAlgorithm)
{
    // For the time being, treat OpenSSL errors (e.g. unable to find the
    // library) as though the signature validated successfully.
    if (!checkOpenSSL()) return true;

    auto md = getMD(hashAlgorithm);
    if (!md) return false;

    EVP_PKEY* pkey = createPublicKeyFromPem(publicKeyPem);
    if (!pkey) return false;
    AT_SCOPE_EXIT(EVP_PKEY_free(pkey));

    auto ctx = EVP_MD_CTX_new();
    if (!ctx) return false;
    AT_SCOPE_EXIT(EVP_MD_CTX_free(ctx));

    if (1 == EVP_DigestVerifyInit(ctx, nullptr, md, nullptr, pkey) &&
        1 == EVP_DigestUpdate(ctx, data.data(), static_cast<size_t>(data.size())) &&
        1 == EVP_DigestVerifyFinal(ctx, reinterpret_cast<const unsigned char*>(signature.data()), static_cast<size_t>(signature.size())))
        return true;

    ERR_print_errors_cb([](const char* str, size_t len, void*) {
        qWarning() << QLatin1String(str, static_cast<int>(len));
        return 0;
    }, nullptr);

    return false;
}

class OpenSSLDeleter
{
private:
    void del(EVP_PKEY_CTX *p){EVP_PKEY_CTX_free(p);}
    void del(EVP_PKEY *p){EVP_PKEY_free(p);}
    void del(X509_STORE *p){X509_STORE_free(p);}
    void del(X509_STORE_CTX *p){X509_STORE_CTX_free(p);}
    void del(X509 *p){X509_free(p);}
    void del(BIO *p){BIO_free(p);}
    void del(stack_st *p){OPENSSL_sk_free(p);}
    void del(char *p){CRYPTO_free(p);}

public:
    // Don't try to free nullptrs - the OpenSSL functions might not have been
    // loaded in that case.
    template<class T>
    void operator()(T *p){if(p) del(p);}
};

template<class T>
using OpenSSLPtr = std::unique_ptr<T, OpenSSLDeleter>;

struct PrivateCA::data
{
    OpenSSLPtr<X509_STORE> pCertStore;
};

OpenSSLPtr<X509> convertCert(const QSslCertificate &cert)
{
    auto der = cert.toDer();
    auto pDataPos = reinterpret_cast<unsigned char*>(der.data());
    return OpenSSLPtr<X509>{d2i_X509(nullptr, &pDataPos, der.size())};
}

PrivateCA::PrivateCA(const QByteArray &caCertPem)
    : _pData{new data{}}
{
    if(!checkOpenSSL())
        return;

    // Create the cert store.  Put it in _pData only once we've successfully
    // loaded the certificate.
    OpenSSLPtr<X509_STORE> pCertStore{X509_STORE_new()};
    if(!pCertStore)
    {
        qWarning() << "Unable to create cert store";
        return;
    }

    // Read the specified CA certificate
    OpenSSLPtr<BIO> pCaFile{BIO_new_mem_buf(caCertPem.data(), caCertPem.size())};
    if(!pCaFile)
    {
        qWarning() << "Can't open CA data";
        return;
    }

    OpenSSLPtr<X509> pCaCert{PEM_read_bio_X509_AUX(pCaFile.get(), nullptr, nullptr, nullptr)};
    if(!pCaCert)
    {
        qWarning() << "Can't read CA cert data";
        return;
    }

    if(!X509_STORE_add_cert(pCertStore.get(), pCaCert.get()))
    {
        qWarning() << "Can't add CA cert to cert store";
        return;
    }

    // Successfully initialized the cert store containing the specified root
    // certificate
    _pData->pCertStore = std::move(pCertStore);
}

PrivateCA::~PrivateCA()
{
    // Out of line to destroy _pData; PrivateCA::data is opaque
}

bool PrivateCA::verifyHttpsCertificate(const QList<QSslCertificate> &certificateChain,
                                       const QString &peerName)
{
    Q_ASSERT(_pData);   // Class invariant
    // If we were not able to load the cert store, fail.  This includes failure
    // to load OpenSSL itself, and also includes failures to read the cert, etc.
    if(!_pData->pCertStore)
    {
        qWarning() << "Could not load OpenSSL";
        return false;
    }

    // Convert the certificates
    std::vector<OpenSSLPtr<X509>> certObjs;
    certObjs.reserve(certificateChain.size());
    for(const auto &cert : certificateChain)
    {
        auto pCertObj = convertCert(cert);
        if(!pCertObj)
        {
            qWarning() << "Can't convert certificate" << certObjs.size()
                << "for peer" << peerName;
            return false;
        }
        certObjs.push_back(std::move(pCertObj));
    }

    if(certObjs.empty())
    {
        qWarning() << "No certificates in chain for" << peerName;
        return false;
    }

    // The first cert is the leaf cert.  The remaining certs are intermediate
    // certs, put them in an OpenSSL stack.  (Note the stack does not own the
    // objects, they're still owned by certObjs.)
    OpenSSLPtr<stack_st> pIntermediateCerts{OPENSSL_sk_new_reserve(nullptr, certObjs.size()-1)};
    if(!pIntermediateCerts)
    {
        qWarning() << "Can't allocate stack of intermediate certs for" << peerName;
        return false;
    }

    auto itNextCert = certObjs.begin();
    ++itNextCert;   // Skip leaf cert
    while(itNextCert != certObjs.end())
    {
        OPENSSL_sk_push(pIntermediateCerts.get(), itNextCert->get());
        ++itNextCert;
    }

    // Create a validation context using the certificates specified
    OpenSSLPtr<X509_STORE_CTX> pContext{X509_STORE_CTX_new()};
    if(!pContext)
    {
        qWarning() << "Can't allocate validation context for" << peerName;
        return false;
    }

    if(!X509_STORE_CTX_init(pContext.get(), _pData->pCertStore.get(),
                            certObjs.front().get(),
                            reinterpret_cast<stack_st_X509*>(pIntermediateCerts.get())))
    {
        qWarning() << "Can't initialize validation context for" << peerName;
        return false;
    }

    int verifyResult = X509_verify_cert(pContext.get());
    // OpenSSL returns 1 for success, 0 for failure, or "in exceptional
    // circumstances it can also return a negative code" - check for 1 exactly
    if(verifyResult != 1)
    {
        qWarning() << "Cert validation failed with result" << verifyResult
            << "for" << peerName;
        int errorCode = X509_STORE_CTX_get_error(pContext.get());
        qWarning() << "Validation error" << errorCode << "at depth"
            << X509_STORE_CTX_get_error_depth(pContext.get()) << "-"
            << QString::fromUtf8(X509_verify_cert_error_string(errorCode));
        return false;
    }

    // Check the host name
    const auto &peerUtf8 = peerName.toUtf8();
    char *pMatchedNameRaw{nullptr};
    int checkHostResult = X509_check_host(certObjs.front().get(),
                                          peerUtf8.data(), peerUtf8.size(), 0,
                                          &pMatchedNameRaw);
    // Own the matched name
    OpenSSLPtr<char> pMatchedName{pMatchedNameRaw};
    if(checkHostResult != 1)
    {
        qWarning() << "Cert hostname validation failed with result"
            << checkHostResult << "for" << peerName;
        return false;
    }

    qInfo() << "Accepted matching name" << QString::fromUtf8(pMatchedName.get())
        << "for peer" << peerName;
    return true;
}

bool genCurve25519KeyPair(unsigned char *pPubkey, unsigned char *pPrivkey)
{
    if(!checkOpenSSL())
        return false;

    OpenSSLPtr<EVP_PKEY_CTX> pPkeyCtx{EVP_PKEY_CTX_new_id(EVP_PKEY_X25519, nullptr)};
    if(!pPkeyCtx)
    {
        qWarning() << "Unable to create curve25519 context";
        return false;
    }

    int result = EVP_PKEY_keygen_init(pPkeyCtx.get());
    if(result != 1)
    {
        qWarning() << "Unable to initialize curve25519 keygen context -" << result;
        return false;
    }

    EVP_PKEY *pPkeyRaw{nullptr};
    result = EVP_PKEY_keygen(pPkeyCtx.get(), &pPkeyRaw);
    OpenSSLPtr<EVP_PKEY> pPkey{pPkeyRaw};
    if(result != 1 || !pPkey)
    {
        qWarning() << "Unable to generate key -" << result;
        return false;
    }

    size_t keylen{0};
    result = EVP_PKEY_get_raw_private_key(pPkey.get(), nullptr, &keylen);
    if(result != 1 || keylen != Curve25519KeySize)
    {
        qWarning() << "Unable to get private key length -" << result << "-" << keylen;
        return false;
    }
    result = EVP_PKEY_get_raw_private_key(pPkey.get(), pPrivkey, &keylen);
    if(result != 1 || keylen != Curve25519KeySize)
    {
        qWarning() << "Unable to get private key -" << result << "-" << keylen;
        return false;
    }

    result = EVP_PKEY_get_raw_public_key(pPkey.get(), nullptr, &keylen);
    if(result != 1 || keylen != Curve25519KeySize)
    {
        qWarning() << "Unable to get public key length -" << result << "-" << keylen;
        return false;
    }
    result = EVP_PKEY_get_raw_public_key(pPkey.get(), pPubkey, &keylen);
    if(result != 1 || keylen != Curve25519KeySize)
    {
        qWarning() << "Unable to get public key -" << result << "-" << keylen;
        return false;
    }

    return true;
}
