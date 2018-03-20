#ifndef TINYJS_AST
#define TINYJS_AST

#include <string>
#include <vector>
#include <memory>

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
            ExprAST(Type Sub_Type) : Sub_Type(Sub_Type) { }
            virtual ~ExprAST() = default;

            Type get_type() { return Sub_Type; }
    };

    template <typename T>
    class ValueExprAST : public ExprAST
    {
        private:
            T Val;

        public:
            ValueExprAST(const T Val) : ExprAST(Type::value_expr), Val(Val) { }
            
            auto get_value() -> decltype(Val) { return Val; }
    };

    class VariableExprAST : public ExprAST
    {
        private:
            std::string Name;

        public:
            VariableExprAST(const std::string& Name) : ExprAST(Type::variable_expr), Name(Name) { }
            
            std::string get_value() { return Name; }
    };

    class BinaryOpExprAST : public ExprAST
    {
        private:
            std::string Op;
            std::unique_ptr<ExprAST> LHS, RHS;
            std::shared_ptr<decltype(std::make_pair(LHS, RHS))> LR_Ptr; /* ret value */
        
        public:
            BinaryOpExprAST(const std::string& Op, std::unique_ptr<ExprAST> LHS, std::unique_ptr<ExprAST> RHS)
                : ExprAST(Type::binary_op_expr), 
                Op(Op), LHS(std::move(LHS)), RHS(std::move(RHS)),
                LR_Ptr(std::make_shared<decltype(std::make_pair(LHS, RHS))>(std::make_pair(std::move(LHS), std::move(RHS)))) { }

            // pair => (Op, (LHS, RHS))
            auto get_value() -> decltype(std::make_pair(Op, LR_Ptr)) { return std::make_pair(Op, LR_Ptr); }
    };

    class CallExprAST : public ExprAST
    {
        private:
            std::string Callee;
            std::vector<std::unique_ptr<ExprAST>> Args;
            std::shared_ptr<decltype(Args)> Args_Ptr; /* ret value */

        public:
            CallExprAST(const std::string& Callee, std::vector<std::unique_ptr<ExprAST>> Args) : 
                ExprAST(Type::call_expr), 
                Callee(Callee), Args(std::move(Args)), 
                Args_Ptr(std::make_shared<decltype(Args)>(std::move(Args))) { }

            // pair => (Callee, Args)
            auto get_value() -> decltype(std::make_pair(Callee, Args_Ptr)) { return std::make_pair(Callee, Args_Ptr); }

    };

    class PrototypeAST : public ExprAST
    {
        private:
            std::string Name;
            std::vector<std::string> Args;
            std::shared_ptr<decltype(Args)> Args_Ptr; /* ret value */

        public:
            PrototypeAST(const std::string& Name, std::vector<std::string> Args) : ExprAST(Type::prototype_expr), Name(Name), Args(Args), 
                Args_Ptr(std::make_shared<decltype(Args)>(std::move(Args))) { }

            // pair => (Name, Args)
            auto get_value() -> decltype(std::make_pair(Name, Args_Ptr)) { return std::make_pair(Name, Args_Ptr); }
    };

    class FunctionAST : public ExprAST
    {
        private:
            std::unique_ptr<PrototypeAST> Proto;
            std::vector<std::unique_ptr<ExprAST>> Body;
            std::shared_ptr<decltype(Body)> Body_Ptr; /* ret value */

        public:
            FunctionAST(std::unique_ptr<PrototypeAST> Proto, std::vector<std::unique_ptr<ExprAST>> Body) : 
                ExprAST(Type::function_expr), 
                Proto(std::move(Proto)), Body(std::move(Body)),
                Body_Ptr(std::make_shared<decltype(Body)>(std::move(Body))) { }

            // pair => (Proto, Body)
            auto get_value() -> decltype(std::make_pair(Proto, Body_Ptr)) { return std::make_pair(std::move(Proto), Body_Ptr); }
    };
}

#endif