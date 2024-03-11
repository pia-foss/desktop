// Copyright (c) 2024 Private Internet Access, Inc.
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

#include <common/src/common.h>
#include <common/src/openssl.h>
#include <common/src/builtin/path.h>
#include "src/testresource.h"
#include "daemon/src/daemon.h"
#include "daemon/src/wireguardbackend.h"
#include "daemon/src/wireguardmethod.h"
#include <QtTest>
#include <QDir>

namespace
{
    nullable_t<StateModel> defaultState;
    nullable_t<Environment> defaultEnv;

    // Sample regions list payload and signature.
    const QByteArray samplePayload1 = TestResource::load(QStringLiteral(":/openssl/payload1/payload"));
    const QByteArray validSig1B64 = TestResource::load(QStringLiteral(":/openssl/payload1/sig"));
    const QByteArray validSig1 = QByteArray::fromBase64(validSig1B64);

    // Another sample regions list payload and signature
    const QByteArray samplePayload2 = TestResource::load(QStringLiteral(":/openssl/payload2/payload"));
    const QByteArray validSig2B64 = TestResource::load(QStringLiteral(":/openssl/payload2/sig"));
    const QByteArray validSig2 = QByteArray::fromBase64(validSig2B64);

    // This garbage "signature" is just 256 random bytes.
    const QByteArray garbageSigB64 = QByteArrayLiteral(
        "vlJX4cqmIcSgyN01eHLFL/6U3UbgfeAJ9kn9cg+FFPMod1KymJFee9DwrOeo\n"
        "qWBL+BVcT4YOlGNFJ14LldWNskBS1IrOGv5ma4MCiTe4q0/b7dKA7mzWHKXn\n"
        "Av5jKsMNYH+8C2+8hsDTVpwBpi1IpXiYW6iAjDD+C0XYJ6315z8Aidmu/OMt\n"
        "jsTQtt6Tco+mNDpwJAtxybtZXfs9iyFza/9sjRAxVF2oQag3JdrjIr2HfN8o\n"
        "5wphNhoH8GramAjHt2XhhlPFrqIDsGyCoCHwqxxchwm4jcp0LAsjq6HaBvki\n"
        "OUSWUDNRV2SzBLd/08lTTyT//Cj94fCHA0vPy4eQIg==\n"
    );
    const QByteArray garbageSig = QByteArray::fromBase64(garbageSigB64);

    // This is a different GPG key used to verify that we reject a valid
    // signature using an incorrect key.
    const QByteArray bogusPubKey = QByteArrayLiteral(
        "-----BEGIN PUBLIC KEY-----\n"
        "MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAxWk8AXMIftJ0nUq315ZV\n"
        "RWaF04ezI5Wjme/xwqp6nLkVmPwbzqhpozCeZczlR/eXsYkNhBR6Y0iTyC7VEUZd\n"
        "QVa1GIXH9As1X7K1FVCwwiXyQ1liesyIiZRVu51cJytYcSiGMKH1uoS8LPexsGHb\n"
        "yY3pXtlCm00XEHHKfC99thFYqnoJNs5bKzy2/KQCYmRFH+ana8ppRjlsIKwfLF+4\n"
        "EJ9Pz3tYjNAaxpzGA8QEmXFHOsDLVj9D5KI6E2cyi7qivh9nl1HRbS2hPAOdn7yJ\n"
        "BVafTCfg32DKKowN5nswoUwJPDJSPPmlIfsZR9gAv95AklSWSjteM4UZLySst4zH\n"
        "uQIDAQAB\n"
        "-----END PUBLIC KEY-----"
    );
    // This is a valid signature on samplePayload2 using the bogus key.
    const QByteArray bogusSig2B64 = QByteArrayLiteral(
        "I5LlnMhHHt55SALzpvVUzWLjLLr2cmoCa98bRx3YNo7AodkONPoTbbwIj6W/k2qE\n"
        "yvlVStmuWsynImuVYAU2L8tWBE1kELn90060V99bRywAa9C6yaQc4Y2O76S8o0w6\n"
        "xnSqCh0KpigkIoG8Lp+eG05C3MWzzv5zgzpEh2iuw4XUbg+0Tn6pHnEzBDbtN0Nw\n"
        "EKs8UqIXuGQNq0c+zMrhw2N3DBi2YNltXi1+DuNEeoFuEGVUlCqy753LTpufCzjS\n"
        "ojKCe3vnThBbl2hNNcHyH3HjDghmg5VodrU1z4dlLWjwH0FCLmDjtOg7XA3flJM5\n"
        "SNC9OjpNsosvaqBn8uj38Q==\n"
    );
    const QByteArray bogusSig2 = QByteArray::fromBase64(bogusSig2B64);

    // Real certificate and serial issued by the PIA RSA-4096 CA
    const QByteArray usCaliforniaCert = TestResource::load(QStringLiteral(":/openssl/45021bfe382ef1190fea63e5bb5b51ce.pem"));
    const QString usCaliforniaSerial = QStringLiteral("45021bfe382ef1190fea63e5bb5b51ce");

    // This is an invalid certificate; it's the cert above with the common name
    // crudely replaced with a different value.  This is used to verify that we
    // do correctly check that the certificate is validly signed.
    const QByteArray hackedCert = TestResource::load(QStringLiteral(":/openssl/hacked-de778d49fe291354c7fef5032e9b61ac.pem"));
    const QString hackedCertSerial = QStringLiteral("de778d49fe291354c7fef5032e9b61ac");

    // A valid certificate chain from a well-known CA (not the PIA RSA-4096 CA).
    // Used to ensure that PrivateCA does _not_ accept certificates issued by
    // roots in the OS certificate store.
    const QByteArray testchainCert = TestResource::load(QStringLiteral(":/openssl/cert-chain-test/privateinternetaccess-com.pem"));
    const QByteArray testchainIntermediateCert = TestResource::load(QStringLiteral(":/openssl/cert-chain-test/privateinternetaccess-com(1).pem"));
    const QByteArray testchainRootCert = TestResource::load(QStringLiteral(":/openssl/cert-chain-test/privateinternetaccess-com(2).pem"));
    const QString testchainName = QStringLiteral("us-california.privateinternetaccess.com");
}

// Test the cryptographic operations that we implement using libssl / libcrypto.
class tst_openssl: public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        // We need to initialize the paths, so we can load openssl libs
        Path::initializePreApp();
        Path::initializePostApp();
        defaultState.emplace();
        defaultEnv.emplace(*defaultState);
    }

    void testGenCurve25519KeyPair()
    {
        wg_key pubKey, privKey;

        qDebug () << "App Dir Path: " << QCoreApplication::applicationDirPath();
        qDebug () << "Base dir: " <<Path::BaseDir;

        QVERIFY(genCurve25519KeyPair(pubKey, privKey));

        QCOMPARE(wgKeyToB64(pubKey).length(), 44);
        QCOMPARE(wgKeyToB64(privKey).length(), 44);
    }

    // Test that valid GPG signatures are accepted
    void testValidSignatures()
    {
        QVERIFY(verifySignature(Environment::defaultRegionsListPublicKey, validSig1, samplePayload1));
        QVERIFY(verifySignature(Environment::defaultRegionsListPublicKey, validSig2, samplePayload2));
    }

    // Test that mismatched GPG signatures are rejected - these signatures were
    // created using the correct key, but are not valid for the payload given
    void testMismatchedSignatures()
    {
        QTest::ignoreMessage(QtMsgType::QtWarningMsg, QRegularExpression{R"(^\d+:error:04091068:rsa routines:int_rsa_verify:bad signature:crypto[\/\\]+rsa[\/\\]+rsa_sign\.c:220:\n$)"});
        QVERIFY(!verifySignature(Environment::defaultRegionsListPublicKey, validSig2, samplePayload1));
        QTest::ignoreMessage(QtMsgType::QtWarningMsg, QRegularExpression{R"(^\d+:error:04091068:rsa routines:int_rsa_verify:bad signature:crypto[\/\\]+rsa[\/\\]+rsa_sign\.c:220:\n$)"});
        QVERIFY(!verifySignature(Environment::defaultRegionsListPublicKey, validSig1, samplePayload2));
    }

    // Test that the garbage signature is rejected
    void testGarbageSignature()
    {
        QTest::ignoreMessage(QtMsgType::QtWarningMsg, QRegularExpression{R"(^\d+:error:0407008A:rsa routines:RSA_padding_check_PKCS1_type_1:invalid padding:crypto[\/\\]+rsa[\/\\]+rsa_pk1.c:\d+:\n$)"});
        QVERIFY(!verifySignature(Environment::defaultRegionsListPublicKey, garbageSig, samplePayload1));
    }

    // Test that a valid signature using the wrong key is rejected.
    void testIncorrectKey()
    {
        // Verify that the signature with the bogus key was correctly generated
        QVERIFY(verifySignature(bogusPubKey, bogusSig2, samplePayload2));
        // That bogus signature should be rejected when expecting the real
        // public key
        QTest::ignoreMessage(QtMsgType::QtWarningMsg, QRegularExpression{R"(^\d+:error:04067072:rsa routines:rsa_ossl_public_decrypt:padding check failed:crypto[\/\\]+rsa[\/\\]+rsa_ossl.c:588:\n$)"});
        QVERIFY(!verifySignature(Environment::defaultRegionsListPublicKey, bogusSig2, samplePayload2));
    }

    // Verify a real certificate with the real PIA RSA-4096 CA certificate
    void testPiaCACertificate()
    {
        auto pCA = defaultEnv->getRsa4096CA();
        QVERIFY(pCA);

        QList<QSslCertificate> certChain;
        certChain.push_back(QSslCertificate{usCaliforniaCert});
        QVERIFY(pCA->verifyHttpsCertificate(certChain, usCaliforniaSerial, true));

        // The certificate should be rejected for any other name
        QTest::ignoreMessage(QtMsgType::QtWarningMsg, R"(Cert hostname validation failed with result 0 for "de778d49fe291354c7fef5032e9b61ac")");
        QVERIFY(!pCA->verifyHttpsCertificate(certChain, hackedCertSerial, true));
    }

    // Verify that a hacked cert is rejected - its signature is not valid.
    // (This cert purports to be signed by the PIA root, and has the expected
    // CN, but it was altered so the signature is invalid.)
    void testHackedPiaCACertificate()
    {
        auto pCA = defaultEnv->getRsa4096CA();
        QVERIFY(pCA);

        QList<QSslCertificate> certChain;
        certChain.push_back(QSslCertificate{hackedCert});
        QTest::ignoreMessage(QtMsgType::QtWarningMsg, R"(Cert validation failed with result 0 for "de778d49fe291354c7fef5032e9b61ac")");
        QTest::ignoreMessage(QtMsgType::QtWarningMsg, R"(Validation error 7 at depth 0 - "certificate signature failure")");
        QVERIFY(!pCA->verifyHttpsCertificate(certChain, hackedCertSerial, true));
    }

    // Verify that a valid cert from another root is not accepted when
    // specifying the PIA root CA.
    void testOtherCACertificate()
    {
        auto pPiaCA = defaultEnv->getRsa4096CA();
        QVERIFY(pPiaCA);

        PrivateCA otherCA{testchainRootCert};

        QList<QSslCertificate> certChain;
        certChain.push_back(QSslCertificate{testchainCert});
        certChain.push_back(QSslCertificate{testchainIntermediateCert});

        // The cert is not accepted by the PIA root
        QTest::ignoreMessage(QtMsgType::QtWarningMsg, R"(Cert validation failed with result 0 for "us-california.privateinternetaccess.com")");
        QTest::ignoreMessage(QtMsgType::QtWarningMsg, R"(Validation error 20 at depth 1 - "unable to get local issuer certificate")");
        QVERIFY(!pPiaCA->verifyHttpsCertificate(certChain, testchainName, true));
        // The cert chain is accepted by the correct root
        QVERIFY(otherCA.verifyHttpsCertificate(certChain, testchainName, true));

        // Including the root itself in the cert chain doesn't affect the result
        certChain.push_back(QSslCertificate{testchainRootCert});
        QTest::ignoreMessage(QtMsgType::QtWarningMsg, R"(Cert validation failed with result 0 for "us-california.privateinternetaccess.com")");
        QTest::ignoreMessage(QtMsgType::QtWarningMsg, R"(Validation error 19 at depth 2 - "self signed certificate in certificate chain")");
        QVERIFY(!pPiaCA->verifyHttpsCertificate(certChain, testchainName, true));
        QVERIFY(otherCA.verifyHttpsCertificate(certChain, testchainName, true));
    }
};

QTEST_GUILESS_MAIN(tst_openssl)
#include TEST_MOC
