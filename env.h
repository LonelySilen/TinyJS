#ifndef TINYJS_ENV
#define TINYJS_ENV

#include <string>
#include <stack>
#include <unordered_map>
#include "log.h"

namespace Env
{
    template <typename T>
    class EnvImpl
    {
    private:
        std::stack<T> EnvStack;
        std::unordered_map<std::string, T> Symbol;

    public:
        EnvImpl() { reset(); }
        virtual ~EnvImpl() = default;

        T get(const std::string& key) { return Symbol.find(key) != Symbol.end() ? Symbol[key] : nullptr; }
        T get() { 
            auto t = EnvStack.top();
            EnvStack.pop();
            return std::move(t); 
        }

        void set(const std::string& key, T value) { Symbol[key] = value; }
        void set(const std::string& key) { Symbol[key] = 0; EnvStack.push(key); }
        void set(long long value) { EnvStack.push(std::make_unique<long long>(value)); }
        void set(double value) { EnvStack.push(std::make_unique<double>(value)); }
        void reset() { Symbol.clear(); while (!EnvStack.empty()) EnvStack.pop(); }
    };
}

#endif