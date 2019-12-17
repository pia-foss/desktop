// Copyright (c) 2019 London Trust Media Incorporated
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

#include <QDir>
#include <QLibrary>
#include <QSslSocket>

#include <cctype>

// Check that a module path matches a certain library name, taking into
// account possible suffixes etc.
template<typename Char>
static bool matchesLibrary(const Char* path, const char* name)
{
    // Skip to last path segment.
    const Char* file = path;
    for (const Char* end = file; *end; ++end)
    {
        if (*end == QDir::separator())
            file = end + 1;
    }
    // Check if the next characters match the supplied name.
    for (size_t len = 0; ; len++)
    {
        // Check if the whole supplied name has matched.
        if (!name[len])
        {
            // Check if we're at a word boundary.
            if (!file[len] || file[len] == '.' || file[len] == '-')
                return true;
            break;
        }
        // Check if we've exhausted the path.
        if (!file[len])
            return false;
        // The supplied name is always ASCII.
        if (file[len] >= 256)
            break;
#ifdef Q_OS_WIN
        // Windows paths are case insensitive.
        if (std::tolower(static_cast<unsigned char>(file[len])) != static_cast<unsigned char>(name[len]))
            break;
#else
        // Pretend all other paths are case sensitive.
        if (static_cast<unsigned char>(file[len]) != static_cast<unsigned char>(name[len]))
            break;
#endif
    }
    return false;
}


#if defined(Q_OS_WIN)

#include <Windows.h>
#include <Psapi.h>

static QLibrary* findOpenSSLLibrary()
{
    HANDLE hProcess = GetCurrentProcess();
    HMODULE hModules[1024];
    DWORD size;

    if (EnumProcessModules(hProcess, hModules, sizeof(hModules), &size))
    {
        for (DWORD i = 0; i < (size / sizeof(HMODULE)); i++)
        {
            wchar_t name[MAX_PATH];
            name[GetModuleFileNameExW(hProcess, hModules[i], name, MAX_PATH)] = 0;
            if (*name && matchesLibrary(name, "libeay32.dll"))
                return new QLibrary(QString::fromWCharArray(name));
        }
    }
    qError() << "Unable to locate libeay32.dll";
    return nullptr;
}

#elif defined(Q_OS_MACOS)

#include <mach-o/dyld.h>

static QLibrary* findOpenSSLLibrary()
{
    const uint32_t count = _dyld_image_count();
    for (uint32_t i = 0; i < count; i++)
    {
        if (const char* name = _dyld_get_image_name(i))
        {
            if (matchesLibrary(name, "libcrypto"))
                return new QLibrary(name);
        }
    }
    qError() << "Unable to locate libcrypto.dylib";
    return nullptr;
}

#elif defined(Q_OS_LINUX)

#include <link.h>
#include <dlfcn.h>

static QLibrary* findOpenSSLLibrary()
{
    void* self = dlopen(nullptr, RTLD_LAZY);
    link_map* map;
    if (!dlinfo(self, RTLD_DI_LINKMAP, &map))
    {
        while (map)
        {
            if (matchesLibrary(map->l_name, "libcrypto"))
                return new QLibrary(map->l_name);
            map = map->l_next;
        }
    }
    qError() << "Unable to locate libcrypto.so";
    return nullptr;
}

#endif


struct EVP_MD_CTX;
struct EVP_PKEY_CTX;
struct EVP_MD;
struct ENGINE;
struct BIO;
struct EVP_PKEY;
typedef int (*pem_password_cb)(char* buf, int size, int rwflag, void* u);


static void (*ERR_print_errors_cb)(int (*cb)(const char* str, size_t len, void* u), void* u) = nullptr;

static BIO* (*BIO_new_mem_buf)(const void *buf, int len) = nullptr;
static void (*BIO_free)(BIO* bp) = nullptr;

static EVP_PKEY* (*PEM_read_bio_PUBKEY)(BIO* bp, EVP_PKEY** x, pem_password_cb* cb, void* u);

static EVP_MD_CTX* (*EVP_MD_CTX_create)() = nullptr;
static void (*EVP_MD_CTX_destroy)(EVP_MD_CTX* ctx) = nullptr;

static const EVP_MD* (*EVP_sha1)() = nullptr;
static const EVP_MD* (*EVP_sha256)() = nullptr;

static void (*EVP_PKEY_free)(EVP_PKEY* pkey) = nullptr;

static int (*EVP_DigestVerifyInit)(EVP_MD_CTX* ctx, EVP_PKEY_CTX** pctx, const EVP_MD* type, ENGINE* e, EVP_PKEY* pkey) = nullptr;
static int (*EVP_DigestUpdate)(EVP_MD_CTX* ctx, const void* d, size_t cnt) = nullptr;
static int (*EVP_DigestVerifyFinal)(EVP_MD_CTX* ctx, const unsigned char* sig, size_t siglen) = nullptr;


static bool checkOpenSSL()
{
    static bool attempted = false, successful = false;

    if (successful)
        return true;
    if (attempted)
        return false;

    attempted = true;

    // This triggers Qt to load OpenSSL dynamically.
    if (!QSslSocket::supportsSsl())
        return false;

    if (QLibrary* openssl = findOpenSSLLibrary())
    {
        if (!openssl->load())
            return false;

#define TRY_RESOLVE_OPENSSL_FUNCTION(name) \
        (name = reinterpret_cast<decltype(name)>(openssl->resolve(#name)))
#define RESOLVE_OPENSSL_FUNCTION(name) \
        if (!TRY_RESOLVE_OPENSSL_FUNCTION(name)) { qError() << "Unable to resolve symbol" << #name; return false; } else ((void)0)

        RESOLVE_OPENSSL_FUNCTION(ERR_print_errors_cb);

        RESOLVE_OPENSSL_FUNCTION(BIO_new_mem_buf);
        RESOLVE_OPENSSL_FUNCTION(BIO_free);

        RESOLVE_OPENSSL_FUNCTION(PEM_read_bio_PUBKEY);

        RESOLVE_OPENSSL_FUNCTION(EVP_MD_CTX_create);
        RESOLVE_OPENSSL_FUNCTION(EVP_MD_CTX_destroy);

        RESOLVE_OPENSSL_FUNCTION(EVP_sha1);
        RESOLVE_OPENSSL_FUNCTION(EVP_sha256);

        RESOLVE_OPENSSL_FUNCTION(EVP_PKEY_free);

        RESOLVE_OPENSSL_FUNCTION(EVP_DigestVerifyInit);
        RESOLVE_OPENSSL_FUNCTION(EVP_DigestUpdate);
        RESOLVE_OPENSSL_FUNCTION(EVP_DigestVerifyFinal);

#undef RESOLVE_OPENSSL_FUNCTION
#undef TRY_RESOLVE_OPENSSL_FUNCTION

        successful = true;

        return true;
    }
    return false;
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
    const bool opensslError = true;

    if (!checkOpenSSL()) return opensslError;

    auto md = getMD(hashAlgorithm);
    if (!md) return false;

    EVP_PKEY* pkey = createPublicKeyFromPem(publicKeyPem);
    if (!pkey) return false;
    AT_SCOPE_EXIT(EVP_PKEY_free(pkey));

    auto ctx = EVP_MD_CTX_create();
    if (!ctx) return false;
    AT_SCOPE_EXIT(EVP_MD_CTX_destroy(ctx));

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
