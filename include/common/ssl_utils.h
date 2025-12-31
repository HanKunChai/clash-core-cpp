#pragma once

#include <openssl/ssl.h>

namespace clash
{
    namespace common
    {
        /**
         * @brief 加载系统 SSL 证书到 SSL_CTX
         * 
         * 在 Windows 上，从系统证书存储区加载根证书。
         * 在 Linux/macOS 上，加载默认路径的证书。
         * 
         * @param ctx OpenSSL SSL_CTX 指针
         */
        void load_system_certificates(SSL_CTX* ctx);
    }
}
