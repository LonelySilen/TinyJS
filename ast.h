#ifndef TINYJS_AST
#define TINYJS_AST

#include <string>
#include <vector>
#include <memory>
#include <map>
#include <iostream>

namespace AST
{
    enum class Type
    {
        /* value_expr */
        integer_expr,
        float_expr,
        string_expr,
        /* syntax */
        variable_expr,
        binary_op_expr,
        call_expr,
        prototype_expr,
        function_expr,
    };

    static std::map<Type, std::string> ASTName {
        { Type::integer_expr   , "integer_expr"   },
        { Type::float_expr     , "float_expr"     },
        { Type::string_expr    , "string_expr"    },
        { Type::variable_expr  , "variable_expr"  },
        { Type::binary_op_expr , "binary_op_expr" },
        { Type::call_expr      , "call_expr"      },
        { Type::prototype_expr , "prototype_expr" },
        { Type::function_expr  , "function_expr"  },
    };

    class ExprAST
    {
        public:
            Type SubType;

            ExprAST() = delete;
            ExprAST(Type SubType) : SubType(SubType) { }
            virtual ~ExprAST() = default;

            void print_ast() 
            {
                std::cout << "ASTName {" << std::endl;
                std::cout << "  " << ASTName[SubType] << std::endl;
                std::cout << "}" << std::endl;
            }
    };

    class IntegerValueExprAST : public ExprAST
    {
        public:
            long long Val;
            IntegerValueExprAST(long long Val) : ExprAST(Type::integer_expr), Val(Val) {}
        
    };

    class FloatValueExprAST : public ExprAST
    {
        public:
            double Val;
            FloatValueExprAST(double Val) : ExprAST(Type::float_expr), Val(Val) { }
        
    };

    class StringValueExprAST : public ExprAST
    {
        public:
            std::string Val;
            StringValueExprAST(const std::string& Val) :  ExprAST(Type::string_expr), Val(Val) { }
        
    };

    class VariableExprAST : public ExprAST
    {
        public:
            std::string Name;
            VariableExprAST(const std::string& Name) : ExprAST(Type::variable_expr), Name(Name) { }
            
    };

    class BinaryOpExprAST : public ExprAST
    {
        public:
            std::string Op;
            std::shared_ptr<ExprAST> LHS, RHS;
            BinaryOpExprAST(const std::string& Op, std::shared_ptr<ExprAST> LHS, std::shared_ptr<ExprAST> RHS)
                : ExprAST(Type::binary_op_expr), 
                Op(Op), LHS(LHS), RHS(RHS) { }

    };

    class CallExprAST : public ExprAST
    {
        public:
            std::string Callee;
            std::vector<std::shared_ptr<ExprAST>> Args;
            CallExprAST(const std::string& tCallee, std::vector<std::shared_ptr<ExprAST>> tArgs) : 
                ExprAST(Type::call_expr), 
                Callee(tCallee), Args(tArgs) { }

    };

    class PrototypeAST : public ExprAST
    {
        public:
            std::string Name;
            std::vector<std::string> Args;
            PrototypeAST(const std::string& tName, std::vector<std::string> tArgs) : ExprAST(Type::prototype_expr), Name(tName), Args(tArgs) { }
    };

    class FunctionAST : public ExprAST
    {
        public:
            std::shared_ptr<PrototypeAST> Proto;
            std::vector<std::shared_ptr<ExprAST>> Body;
            FunctionAST(std::shared_ptr<PrototypeAST> tProto, std::vector<std::shared_ptr<ExprAST>> tBody) : 
                ExprAST(Type::function_expr), 
                Proto(tProto), Body(tBody) { }

            int test() { return 1; }
    };

}

#endif