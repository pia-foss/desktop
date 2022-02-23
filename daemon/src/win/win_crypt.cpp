// Copyright (c) 2022 Private Internet Access, Inc.
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
#line SOURCE_FILE("win/win_crypt.cpp")

#include "win_crypt.h"
#include "win/win_util.h"
#include <wincrypt.h>

#pragma comment(lib, "Crypt32.lib")

struct WinCloseCertStore
{
    void operator()(HCERTSTORE handle){::CertCloseStore(handle, 0);}
};

struct WinCloseCryptMsg
{
    void operator()(HCRYPTMSG handle){::CryptMsgClose(handle);}
};

using WinCertStore = WinGenericHandle<HCERTSTORE, WinCloseCertStore>;
using WinCryptMsg = WinGenericHandle<HCRYPTMSG, WinCloseCryptMsg>;

std::wstring winGetSignerName(WinCertStore &certStore, WinCryptMsg &cryptMsg,
                              const QStringView &path, DWORD signerIndex);

std::set<std::wstring> winGetExecutableSigners(const QStringView &path)
{
    DWORD dwEncoding{0}, dwContentType{0}, dwFormatType{0};
    WinCertStore certStore;
    WinCryptMsg cryptMsg;

    // Get the message and store handles for this executable
    if(!::CryptQueryObject(CERT_QUERY_OBJECT_FILE,
                           path.utf16(),
                           CERT_QUERY_CONTENT_FLAG_PKCS7_SIGNED_EMBED,
                           CERT_QUERY_FORMAT_FLAG_BINARY,
                           0,
                           &dwEncoding,
                           &dwContentType,
                           &dwFormatType,
                           certStore.receive(),
                           cryptMsg.receive(),
                           nullptr))
    {
        qWarning() << "Failed to load signature information from" << path
            << SystemError{HERE};
        return {};
    }

    // Get the number of signers
    DWORD signers;
    DWORD signersSize{sizeof(signers)};
    if(!::CryptMsgGetParam(cryptMsg, CMSG_SIGNER_COUNT_PARAM, 0,
                           &signers, &signersSize) ||
       signersSize != sizeof(signers))
    {
        qWarning() << "Failed to load signer count for" << path
            << SystemError{HERE};
        return {};
    }

    qInfo() << "Executable has" << signers << "signatures -" << path;
    std::set<std::wstring> signerSubjects;
    for(DWORD i=0; i<signers; ++i)
    {
        std::wstring signerSubject = winGetSignerName(certStore, cryptMsg, path, i);
        if(!signerSubject.empty())
        {
            qInfo() << " -" << i << "-" << signerSubject;
            signerSubjects.insert(std::move(signerSubject));
        }
    }

    return signerSubjects;
}

std::wstring winGetSignerName(WinCertStore &certStore, WinCryptMsg &cryptMsg,
                              const QStringView &path, DWORD signerIndex)
{
    // Get the signer info
    DWORD signerInfoSize{0};
    if(!::CryptMsgGetParam(cryptMsg, CMSG_SIGNER_INFO_PARAM, signerIndex, nullptr, &signerInfoSize))
    {
        qWarning() << "Failed to get size of signer info for index"
            << signerIndex << "of" << path << SystemError{HERE};
        return {};
    }
    if(signerInfoSize == 0)
        return {};  // No signer info (check this to ensure data() is valid later)

    std::vector<unsigned char> signerInfo;
    signerInfo.resize(signerInfoSize);
    if(!::CryptMsgGetParam(cryptMsg, CMSG_SIGNER_INFO_PARAM, signerIndex, signerInfo.data(), &signerInfoSize))
    {
        qWarning() << "Failed to get signer info for index" << signerIndex
            << "of" << path << SystemError{HERE};
        return {};
    }
    signerInfo.resize(signerInfoSize);
    CMSG_SIGNER_INFO *pSignerInfo = reinterpret_cast<CMSG_SIGNER_INFO*>(signerInfo.data());

    // Find the certificate for this signature
    CERT_INFO signCertInfo{};
    signCertInfo.Issuer = pSignerInfo->Issuer;
    signCertInfo.SerialNumber = pSignerInfo->SerialNumber;

    const CERT_CONTEXT *pCertContext = ::CertFindCertificateInStore(certStore,
                                                                    X509_ASN_ENCODING|PKCS_7_ASN_ENCODING,
                                                                    0,
                                                                    CERT_FIND_SUBJECT_CERT,
                                                                    &signCertInfo,
                                                                    nullptr);
    if(!pCertContext)
    {
        qWarning() << "Failed to find certificate for signature"
            << signerIndex << "of" << path;
        return {};
    }

    // Get the size of the Subject name
    DWORD subjSize = ::CertGetNameString(pCertContext,
                                         CERT_NAME_SIMPLE_DISPLAY_TYPE, 0,
                                         nullptr, nullptr, 0);
    // subjSize includes the terminating null char; this function returns 1 if
    // the name isn't found (would just return a string with a single null char)
    if(subjSize <= 1)
    {
        qWarning() << "Failed to get subject name size for signature"
            << signerIndex << "of" << path;
        return {};
    }

    std::wstring name;
    name.resize(subjSize);
    subjSize = ::CertGetNameString(pCertContext, CERT_NAME_SIMPLE_DISPLAY_TYPE,
                                   0, nullptr, name.data(), name.size());
    if(subjSize <= 1)
    {
        qWarning() << "Failed to get subject name size for signature"
            << signerIndex << "of" << path;
        return {};
    }

    // Truncate off the terminating null from the API (wstring still has its own
    // terminating null char)
    name.resize(subjSize-1);
    return name;
}
