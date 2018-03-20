#ifndef TINYJS_ENV
#define TINYJS_ENV

#include <string>
#include <unordered_map>

namespace Env
{
    template <typename T>
    class EnvImpl
    {
    private:
        std::unordered_map<std::string, T> Symbol;

    public:
        EnvImpl() { Symbol.clear(); }
        virtual ~EnvImpl() = default;
    };
}

#endif