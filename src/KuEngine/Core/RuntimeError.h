// KuEngine Runtime 错误分类：区分环境不可用与实现/资源故障。
#pragma once

#include <stdexcept>
#include <string>

namespace ku {

class RuntimeUnavailableError : public std::runtime_error {
public:
    explicit RuntimeUnavailableError(const std::string& message)
        : std::runtime_error(message)
    {
    }
};

} // namespace ku
