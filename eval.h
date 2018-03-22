#ifndef TINYJS_EVAL
#define TINYJS_EVAL
#include "ast.h"
#include "env.h"
#include "log.h"

namespace Eval
{
    using namespace AST;
    using namespace Env;

    template <typename T>
    class EvalImpl : public Env::EnvImpl<typename T::value_type>
    {
    using EnvImpl = EnvImpl<typename T::value_type>;
    
    private:
        T Expression; // T := vector<unique_ptr<ExprAST>>

    public:
        EvalImpl() = delete;
        EvalImpl(T Expression) : Expression(std::move(Expression)) {}
        ~EvalImpl() = default;
        
        EvalImpl(const EvalImpl&) = delete;
        const EvalImpl& operator =(const EvalImpl&) = delete;
        EvalImpl(EvalImpl&&) = delete;
        const EvalImpl& operator =(EvalImpl&&) = delete;

        void eval()
        {
            // EnvImpl::get("hi");
            // std::cout << EvalImpl::test() << std::endl;
            // std::cout << "eval\n" << Expression.size() << std::endl;
            for (auto& i : Expression)
            {
                action(i);
                // EnvImpl::set(static_cast<AST::StringValueExprAST*>(expr.get())->Val);
            }
            std::cout << std::static_pointer_cast<StringValueExprAST>(EnvImpl::get("a"))->Val << std::endl;
            std::cout << std::static_pointer_cast<StringValueExprAST>(EnvImpl::get("b"))->Val << std::endl;
        }

        void action(std::shared_ptr<ExprAST> expr)
        {
            switch (expr->SubType)
            {
                case Type::function_expr:
                    break;
                case Type::binary_op_expr:
                    eval_binary_op_expr(std::static_pointer_cast<BinaryOpExprAST>(expr));
                    break;
                default:
                    break;
            }
        }

        std::shared_ptr<ExprAST> eval_binary_op_expr(std::shared_ptr<BinaryOpExprAST> expr)
        {
            std::shared_ptr<ExprAST> LHS, RHS;
            switch (expr->LHS->SubType)
            {
                case Type::integer_expr: 
                case Type::float_expr: 
                case Type::string_expr:
                case Type::variable_expr:
                    LHS = expr->LHS;
                    break;
                case Type::binary_op_expr:
                    LHS = eval_binary_op_expr(std::static_pointer_cast<BinaryOpExprAST>(expr->LHS));
                    break;
                default: 
                    break;
            }

            switch (expr->RHS->SubType)
            {
                case Type::integer_expr: 
                case Type::float_expr: 
                case Type::string_expr:
                case Type::variable_expr:
                    RHS = expr->RHS;
                    break;
                case Type::binary_op_expr:
                    RHS = eval_binary_op_expr(std::static_pointer_cast<BinaryOpExprAST>(expr->RHS));
                    break;
                default: 
                    break;
            }

            return bin_op_expr_helper(expr->Op, LHS, RHS);
        }

        std::shared_ptr<ExprAST> bin_op_expr_helper(const std::string& Op, std::shared_ptr<ExprAST> LHS, std::shared_ptr<ExprAST> RHS)
        {
            if (Op == "=")
            {
                if (LHS->SubType != Type::variable_expr)
                    eval_err("[bin_op_expr_helper] Expected a variable_expr, rvalue is not a identifier", LHS);
                EnvImpl::set(std::static_pointer_cast<StringValueExprAST>(LHS)->Val, RHS);
                return RHS;
            }
            else if (Op == "+")
                return add(LHS, RHS);
            else
            {
                log_err("[bin_op_expr_helper] Invalid paramters.");
            }
            return nullptr;
        }

        std::shared_ptr<ExprAST> add(const std::shared_ptr<ExprAST> LHS, const std::shared_ptr<ExprAST> RHS)
        {
            // 1+1=2
            if (LHS->SubType == Type::integer_expr && RHS->SubType == Type::integer_expr)
                return std::make_shared<IntegerValueExprAST>(std::static_pointer_cast<IntegerValueExprAST>(LHS)->Val + std::static_pointer_cast<IntegerValueExprAST>(RHS)->Val);
            // 1+1.0=2.0
            if (LHS->SubType == Type::integer_expr && RHS->SubType == Type::float_expr)
                return std::make_shared<FloatValueExprAST>(std::static_pointer_cast<IntegerValueExprAST>(LHS)->Val + std::static_pointer_cast<FloatValueExprAST>(RHS)->Val);
            // 1.0+1=2.0
            if (LHS->SubType == Type::float_expr && RHS->SubType == Type::integer_expr) 
                return std::make_shared<FloatValueExprAST>(std::static_pointer_cast<FloatValueExprAST>(LHS)->Val + std::static_pointer_cast<IntegerValueExprAST>(RHS)->Val);
            // 1.0+1.0=2.0
            if (LHS->SubType == Type::float_expr && RHS->SubType == Type::float_expr)
                return std::make_shared<FloatValueExprAST>(std::static_pointer_cast<FloatValueExprAST>(LHS)->Val + std::static_pointer_cast<FloatValueExprAST>(RHS)->Val);
            // "1"+"1"="11"
            if (LHS->SubType == Type::string_expr && RHS->SubType == Type::string_expr)
                return std::make_shared<StringValueExprAST>(std::static_pointer_cast<StringValueExprAST>(LHS)->Val + std::static_pointer_cast<StringValueExprAST>(RHS)->Val);
            // 1+"1"="11"
            if (LHS->SubType == Type::integer_expr && RHS->SubType == Type::string_expr)
                return std::make_shared<StringValueExprAST>(std::to_string(std::static_pointer_cast<IntegerValueExprAST>(LHS)->Val) + std::static_pointer_cast<StringValueExprAST>(RHS)->Val);
            // "1"+1="11"
            if (LHS->SubType == Type::string_expr && RHS->SubType == Type::integer_expr)
                return std::make_shared<StringValueExprAST>(std::static_pointer_cast<StringValueExprAST>(LHS)->Val + std::to_string(std::static_pointer_cast<IntegerValueExprAST>(RHS)->Val));
            // 1.0+"1"="1.01"
            if (LHS->SubType == Type::float_expr && RHS->SubType == Type::string_expr)
                return std::make_shared<StringValueExprAST>(std::to_string(std::static_pointer_cast<FloatValueExprAST>(LHS)->Val) + std::static_pointer_cast<StringValueExprAST>(RHS)->Val);
            // "1"+1.1="11.1"
            if (LHS->SubType == Type::string_expr && RHS->SubType == Type::float_expr)
                return std::make_shared<StringValueExprAST>(std::static_pointer_cast<StringValueExprAST>(LHS)->Val + std::to_string(std::static_pointer_cast<FloatValueExprAST>(RHS)->Val));
            return nullptr;
        }

        void eval_err(const std::string& loginfo, std::shared_ptr<ExprAST> ast)
        {
            std::cerr << loginfo << std::endl;
            ast->print_ast();
            exit(1);
        }

    };
}

#endif