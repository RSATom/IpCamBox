#pragma once

#include <gio/gio.h>

#include <memory>


namespace DeviceBox
{

G_BEGIN_DECLS

#define TYPE_CERTIFICATE_PROVIDER certificate_provider_get_type()
G_DECLARE_FINAL_TYPE(
    CertificateProvider, certificate_provider, , CERTIFICATE_PROVIDER,
    GTlsInteraction)

CertificateProvider* certificate_provider_new(GTlsCertificate* certificate);

G_END_DECLS

struct CertificateProviderUnref
{
    void operator() (CertificateProvider* provider)
        { g_object_unref(provider); }
};

typedef
    std::unique_ptr<
        CertificateProvider,
        CertificateProviderUnref> CertificateProviderPtr;

}
