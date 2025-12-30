#pragma once

#include "rule/rule_interface.h"

namespace clash
{
    namespace rule
    {
        class GeoIP : public Rule
        {
        public:
            GeoIP(std::string country_code, std::string adapter, bool no_resolve = false);

            constant::RuleType type() const override;
            bool match(const constant::Metadata& metadata) const override;
            std::string adapter() const override;
            std::string payload() const override;

        private:
            std::string country_code_;
            std::string adapter_;
            bool no_resolve_;
        };
    }
}
