#ifndef TINYJS_ENV
#define TINYJS_ENV

#include <string>
#include <vector>
#include <unordered_map>

namespace Env
{
    template <typename T>
    class EnvImpl
    {
    private:
        std::unordered_map<std::string, T> Symbol;

    public:
        std::string Name;
        std::shared_ptr<EnvImpl<T>> Parent; // Parent Scope

        EnvImpl() : Name("") { }
        EnvImpl(const std::string& Name) : Name(Name) { }
        EnvImpl(std::shared_ptr<EnvImpl<T>> Parent) : Name(""), Parent(Parent) { }
        EnvImpl(const std::string& Name, std::shared_ptr<EnvImpl<T>> Parent) : Name(Name), Parent(Parent) { }
        virtual ~EnvImpl() = default;

        T get(const std::string& key) { return Symbol.find(key) != Symbol.end() ? Symbol[key] : nullptr; }

        void set(const std::string& key, T value) { Symbol[key] = value; }
        void set(const std::string& key) { Symbol[key] = 0; }
        void reset() { Symbol.clear(); }
    };
}

#endif