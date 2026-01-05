#include "common/ssl_utils.h"
#include "log/log.h"

#ifdef _WIN32
#include <windows.h>
#include <wincrypt.h>
#endif

namespace clash
{
    namespace common
    {
        void load_system_certificates(SSL_CTX* ctx)
        {
#ifdef _WIN32
            X509_STORE* store = SSL_CTX_get_cert_store(ctx);
            if (!store) return;

            // Windows 系统证书存储区名称
            // ROOT: 受信任的根证书颁发机构
            // CA: 中级证书颁发机构
            const char* store_names[] = { "ROOT", "CA" };
            int count = 0;

            for (const char* store_name : store_names)
            {
                HCERTSTORE hStore = CertOpenSystemStoreA(0, store_name);
                if (hStore)
                {
                    PCCERT_CONTEXT pContext = NULL;
                    while ((pContext = CertEnumCertificatesInStore(hStore, pContext)) != NULL)
                    {
                        // 转换 Windows 证书为 OpenSSL X509
                        const unsigned char* encoded_cert = pContext->pbCertEncoded;
                        // d2i_X509 会移动指针，所以传入副本
                        X509* x509 = d2i_X509(NULL, &encoded_cert, pContext->cbCertEncoded);
                        if (x509)
                        {
                            if (X509_STORE_add_cert(store, x509) == 1) {
                                count++;
                            }
                            X509_free(x509);
                        }
                    }
                    CertCloseStore(hStore, 0);
                }
            }
            LOG_INFO("Loaded %d Windows system certificates", count);
#else
            // Linux/macOS 使用默认路径
            if (SSL_CTX_set_default_verify_paths(ctx) != 1) {
                LOG_WARN("Failed to load default verify paths");
            } else {
                LOG_INFO("Loaded default system certificates");
            }
#endif
        }
    }
}