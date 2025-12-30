#include "rule/geoip.h"
#include "common/geoip.h"
#include "log/log.h"

namespace clash
{
    namespace rule
    {
        GeoIP::GeoIP(std::string country_code, std::string adapter, bool no_resolve)
            : country_code_(std::move(country_code)), adapter_(std::move(adapter)), no_resolve_(no_resolve)
        {
        }

        constant::RuleType GeoIP::type() const
        {
            return constant::RuleType::GEOIP;
        }

        bool GeoIP::match(const constant::Metadata& metadata) const
        {
            if (metadata.dstIP.empty())
            {
                return false;
            }

            std::string code = common::GeoIP::instance().lookup(metadata.dstIP);
            return code == country_code_;
        }

        std::string GeoIP::adapter() const
        {
            return adapter_;
        }

        std::string GeoIP::payload() const
        {
            return country_code_;
        }
    }
}
