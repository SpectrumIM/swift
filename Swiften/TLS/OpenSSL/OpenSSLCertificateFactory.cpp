/*
 * Copyright (c) 2018 Isode Limited.
 * All rights reserved.
 * See the COPYING file for more information.
 */

#include <Swiften/TLS/OpenSSL/OpenSSLCertificateFactory.h>

#include <openssl/pem.h>
#include <openssl/err.h>
#include <limits>

namespace Swift {

OpenSSLCertificateFactory::OpenSSLCertificateFactory() {
}

OpenSSLCertificateFactory::~OpenSSLCertificateFactory() {
}

Certificate* OpenSSLCertificateFactory::createCertificateFromDER(const ByteArray& der) {
    return new OpenSSLCertificate(der);
}

std::vector<std::shared_ptr<Certificate>> OpenSSLCertificateFactory::createCertificateChain(const ByteArray& data) {
    std::vector<std::shared_ptr<Certificate>> certificateChain;

    if (data.size() > std::numeric_limits<int>::max()) {
        return certificateChain;
    }

    auto bio = std::shared_ptr<BIO>(BIO_new(BIO_s_mem()), BIO_free);
    BIO_write(bio.get(), vecptr(data), int(data.size()));

    // Attempt parsing data as PEM
    X509* openSSLCert = nullptr;
    auto x509certFromPEM = PEM_read_bio_X509(bio.get(), &openSSLCert, nullptr, nullptr);
    if (x509certFromPEM && openSSLCert) {
        std::shared_ptr<X509> x509Cert(openSSLCert, X509_free);
        certificateChain.emplace_back(std::make_shared<OpenSSLCertificate>(x509Cert));
        openSSLCert = nullptr;
        while ((x509certFromPEM = PEM_read_bio_X509(bio.get(), &openSSLCert, nullptr, nullptr)) != nullptr) {
            std::shared_ptr<X509> x509Cert(openSSLCert, X509_free);
            certificateChain.emplace_back(std::make_shared<OpenSSLCertificate>(x509Cert));
            openSSLCert = nullptr;
        }
    }

    // Clear any (expected) errors which resulted from PEM parsing
    // If we don't do this, any existing TLS context will detect these
    // spurious errors and fail to work
    ERR_clear_error();

    return certificateChain;
}

}
