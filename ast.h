#ifndef TINYJS_AST
#define TINYJS_AST

#include <string>
#include <vector>

namespace AST
{
    enum class Type
    {
        value_expr,
        variable_expr,
        binary_op_expr,
        call_expr,
        prototype_expr,
        function_expr,
    };

    class ExprAST
    {
        private:
            Type Sub_Type;

        public:
            ExprAST() = delete;
            ExprAST(Type Sub_Type) : Sub_Type(Sub_Type) {}
            virtual ~ExprAST() = default;

            Type get_type() { return Sub_Type; }
    };

    template <typename T>
    class ValueExprAST : public ExprAST
    {
        private:
            T Val;

        public:
            ValueExprAST(const T Val) : ExprAST(Type::value_expr), Val(Val) {}
            
            auto get_value() -> decltype(Val) { return Val; }
    };

    class VariableExprAST : public ExprAST
    {
        private:
            std::string Name;

        public:
            VariableExprAST(const std::string& Name) : ExprAST(Type::variable_expr), Name(Name) {}
            
            std::string get_value() { return Name; }
    };

    class BinaryOpExprAST : public ExprAST
    {
        private:
            std::string Op;
            std::unique_ptr<ExprAST> LHS, RHS;
        
        public:
            BinaryOpExprAST(const std::string& Op, std::unique_ptr<ExprAST> LHS, std::unique_ptr<ExprAST> RHS)
                : ExprAST(Type::binary_op_expr), Op(Op), LHS(std::move(LHS)), RHS(std::move(RHS)) {}

            // pair => (Op, (LHS, RHS))
            auto get_value() -> decltype(std::make_pair(Op, std::make_pair(std::move(LHS), std::move(RHS))))&& { return std::move(std::make_pair(Op, std::make_pair(std::move(LHS), std::move(RHS)))); }
    };

    class CallExprAST : public ExprAST
    {
        private:
            std::string Callee;
            std::vector<std::unique_ptr<ExprAST>> Args;

        public:
            CallExprAST(const std::string& Callee, std::vector<std::unique_ptr<ExprAST>> Args) : ExprAST(Type::call_expr), Callee(Callee), Args(std::move(Args)) {}

            // pair => (Callee, Args)
            auto get_value() -> decltype(std::make_pair(Callee, std::move(Args)))&& { return std::move(std::make_pair(Callee, std::move(Args))); }

    };

    class PrototypeAST : public ExprAST
    {
        private:
            std::string Name;
            std::vector<std::string> Args;

        public:
            PrototypeAST(const std::string& Name, const std::vector<std::string>& Args) : ExprAST(Type::prototype_expr), Name(Name), Args(Args) {}

            // pair => (Name, Args)
            auto get_value() -> decltype(std::make_pair(Name, std::move(Args)))&& { return std::move(std::make_pair(Name, std::move(Args))); }
    };

    class FunctionAST : public ExprAST
    {
        private:
            std::unique_ptr<PrototypeAST> Proto;
            std::vector<std::unique_ptr<ExprAST>> Body;

        public:
            FunctionAST(std::unique_ptr<PrototypeAST> Proto,
                        std::vector<std::unique_ptr<ExprAST>> Body)
            : ExprAST(Type::function_expr), Proto(std::move(Proto)), Body(std::move(Body)) {}

            // pair => (Proto, Body)
            auto get_value() -> decltype(std::make_pair(std::move(Proto), std::move(Body)))&& { return std::move(std::make_pair(std::move(Proto), std::move(Body))); }
    };
}

#endif