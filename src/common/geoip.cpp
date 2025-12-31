#include "common/geoip.h"
#include "log/log.h"
#ifndef _WIN32
#include <arpa/inet.h>
#else
#include <winsock2.h>
#include <ws2tcpip.h>
#endif



namespace clash
{
    namespace common
    {
        GeoIP& GeoIP::instance()
        {
            static GeoIP instance;
            return instance;
        }

        GeoIP::GeoIP()
        {
        }

        GeoIP::~GeoIP()
        {
            if (valid_)
            {
                MMDB_close(&mmdb_);
            }
        }

        bool GeoIP::init(const std::string& path)
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (valid_)
            {
                MMDB_close(&mmdb_);
                valid_ = false;
            }

            int status = MMDB_open(path.c_str(), MMDB_MODE_MMAP, &mmdb_);
            if (status != MMDB_SUCCESS)
            {
                LOG_ERROR("Failed to open GeoIP database: %s, error: %s", path.c_str(), MMDB_strerror(status));
                return false;
            }

            valid_ = true;
            LOG_INFO("GeoIP database loaded: %s", path.c_str());
            return true;
        }

        std::string GeoIP::lookup(const std::string& ip)
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!valid_) return "";

            int gai_error, mmdb_error;
            MMDB_lookup_result_s result = MMDB_lookup_string(&mmdb_, ip.c_str(), &gai_error, &mmdb_error);

            if (gai_error != 0)
            {
                // LOG_DEBUG("GeoIP lookup failed for %s: gai_error=%d", ip.c_str(), gai_error);
                return "";
            }

            if (mmdb_error != MMDB_SUCCESS)
            {
                // LOG_DEBUG("GeoIP lookup failed for %s: mmdb_error=%s", ip.c_str(), MMDB_strerror(mmdb_error));
                return "";
            }

            if (!result.found_entry)
            {
                return "";
            }

            MMDB_entry_data_s entry_data;
            int status = MMDB_get_value(&result.entry, &entry_data, "country", "iso_code", NULL);
            if (status != MMDB_SUCCESS)
            {
                return "";
            }

            if (entry_data.has_data && entry_data.type == MMDB_DATA_TYPE_UTF8_STRING)
            {
                return std::string(entry_data.utf8_string, entry_data.data_size);
            }

            return "";
        }
    }
}
