#include "CertificateProvider.h"

#include <CxxPtr/GlibPtr.h>


namespace DeviceBox
{

struct _CertificateProvider
{
    GTlsInteraction parentInstance;

    GTlsCertificatePtr certificatePtr;
};

G_DEFINE_TYPE(CertificateProvider, certificate_provider, G_TYPE_TLS_INTERACTION)

static GTlsInteractionResult
request_certificate(GTlsInteraction* interaction,
                    GTlsConnection* connection,
                    GTlsCertificateRequestFlags flags,
                    GCancellable* cancellable,
                    GError** error);

CertificateProvider*
certificate_provider_new(GTlsCertificate* certificate)
{
    if(!certificate)
        return nullptr;

    CertificateProvider* instance =
        (CertificateProvider*)g_object_new(TYPE_CERTIFICATE_PROVIDER, NULL);

    if(instance) {
        g_object_ref(certificate);
        instance->certificatePtr.reset(certificate);
    }

    return instance;
}

static void
certificate_provider_class_init(CertificateProviderClass* klass)
{
    GTlsInteractionClass* parent_klass = G_TLS_INTERACTION_CLASS(klass);

    parent_klass->request_certificate = request_certificate;
}

static void
certificate_provider_init(CertificateProvider* self)
{
}

static GTlsInteractionResult
request_certificate(GTlsInteraction* interaction,
                    GTlsConnection* connection,
                    GTlsCertificateRequestFlags /*flags*/,
                    GCancellable* cancellable,
                    GError** error)
{
    CertificateProvider* self =
        _CERTIFICATE_PROVIDER(interaction);

    g_tls_connection_set_certificate(connection, self->certificatePtr.get());

    return G_TLS_INTERACTION_HANDLED;
}

}
