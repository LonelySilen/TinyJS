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
        EnvImpl() { reset(); }
        virtual ~EnvImpl() = default;

        T&& get(const std::string& key) { return Symbol.find(key) != Symbol.end() ? Symbol[key] : nullptr; }

        void set(const std::string& key, T&& value) { Symbol[key] = value; }
        void reset() { Symbol.clear(); }
    };
}

#endif