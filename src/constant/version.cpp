#include "constant/version.h"

namespace clash
{
    namespace constant
    {

        std::string Version = "V1.1.0";

        // 如果定义了 BUILD_TIME 宏，则使用该宏的值，否则使用默认值
#ifdef BUILD_TIME
        std::string BuildTime = BUILD_TIME;
#else
        std::string BuildTime = "unknown time";
#endif

    } // namespace constant
} // namespace clash
