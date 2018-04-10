#ifndef TINYJS_BUILTIN
#define TINYJS_BUILTIN

#include <unordered_map>

namespace BuiltIn
{
    class BuiltInImpl
    {
    public:
        std::unordered_map<std::string, int> BuiltIn;
        BuiltInImpl()
        {
            BuiltIn["print"] = 1;
        }


        bool is_built_in(const std::string& Name)
        {
            return BuiltIn.find(Name) != BuiltIn.end() ? true : false;
        }
        
    };
    
}

#endif