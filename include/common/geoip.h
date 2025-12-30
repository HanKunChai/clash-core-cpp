#pragma once

#include <string>
#include <mutex>
#include <maxminddb.h>

namespace clash
{
    namespace common
    {
        class GeoIP
        {
        public:
            static GeoIP& instance();

            bool init(const std::string& path);
            std::string lookup(const std::string& ip);
            
            // Prevent copy
            GeoIP(const GeoIP&) = delete;
            GeoIP& operator=(const GeoIP&) = delete;

        private:
            GeoIP();
            ~GeoIP();

            MMDB_s mmdb_;
            bool valid_ = false;
            std::mutex mutex_;
        };
    }
}
