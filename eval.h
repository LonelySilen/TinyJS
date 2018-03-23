#ifndef TINYJS_EVAL
#define TINYJS_EVAL

#include <cmath>
#include <string>
#include <unordered_map>
#include "ast.h"
#include "env.h"
#include "log.h"

namespace Eval
{
    using namespace AST;
    using namespace Env;

    template <typename T>
    class EvalImpl
    {
    using EnvImpl = EnvImpl<typename T::value_type>;
    
    private:
        std::shared_ptr<EnvImpl> Scope;
        std::shared_ptr<EnvImpl> CurScope;
        const std::string TopScope = "__top_expression";
        T Expression; // T := vector<unique_ptr<ExprAST>>

    public:
        EvalImpl() = delete;
        EvalImpl(T Expression) : Expression(std::move(Expression)) 
        {
            if (!Scope)
            {
                CurScope.reset(new EnvImpl(TopScope));
                Scope = CurScope;
            }
        }
        ~EvalImpl() = default;
        
        EvalImpl(const EvalImpl&) = delete;
        const EvalImpl& operator =(const EvalImpl&) = delete;
        EvalImpl(EvalImpl&&) = delete;
        const EvalImpl& operator =(EvalImpl&&) = delete;

        void enter_new_env()
        {
            CurScope.reset(new EnvImpl(CurScope));
        }

        void enter_new_env(const std::string& Name)
        {
            CurScope.reset(new EnvImpl(Name, CurScope));
        }

        void recover_prev_env()
        {
            if (CurScope->Parent)
                CurScope = CurScope->Parent;
        }

        std::shared_ptr<EnvImpl> find_name_belong_scope(const std::string& Name)
        {
            auto val = CurScope->get(Name);
            auto _CurScope = CurScope;
            // Search for parent scope
            while (!val && _CurScope->Parent)
            {
                _CurScope = _CurScope->Parent;
                val = _CurScope->get(Name);
            }
            if (!val) return CurScope;
            return _CurScope;
        }

        // Find exist name
        // if (!find_name()) => Check name is exist?
        std::shared_ptr<ExprAST> find_name(const std::string& Name)
        {
            return find_name_belong_scope(Name)->get(Name);
        }

        bool set_name(const std::string& Name, std::shared_ptr<ExprAST> Value)
        {
            find_name_belong_scope(Name)->set(Name, Value);
            return true;
        }

        void print_value(const std::string& Name)
        {
            auto V = CurScope->get(Name);
            if (!V)
            {
                std::cout << "Undefined '" << Name << "'." << std::endl;
                return; 
            }
            switch (V->SubType)
            {
                case Type::integer_expr:
                    std::cout << std::static_pointer_cast<IntegerValueExprAST>(V)->Val << std::endl;
                    break;
                case Type::float_expr:
                    std::cout << std::static_pointer_cast<FloatValueExprAST>(V)->Val << std::endl;
                    break;
                case Type::string_expr:
                    std::cout << std::static_pointer_cast<StringValueExprAST>(V)->Val << std::endl;
                    break;
                default:
                    break;
            }
        }

        void eval()
        {
            for (auto& i : Expression)
                eval_one(i);

            print_value("a");
            print_value("d");
        }

        std::shared_ptr<ExprAST> eval_one(std::shared_ptr<ExprAST> expr)
        {
            switch (expr->SubType)
            {
                case Type::integer_expr: 
                case Type::float_expr: 
                case Type::string_expr:
                case Type::variable_expr:
                    return expr;
                case Type::function_expr:
                    return eval_function_expr(std::static_pointer_cast<FunctionAST>(expr));
                case Type::binary_op_expr:
                    return eval_binary_op_expr(std::static_pointer_cast<BinaryOpExprAST>(expr));
                case Type::call_expr:
                    return eval_call_expr(std::static_pointer_cast<CallExprAST>(expr));
                default:
                    expr->print_ast();
                    eval_err("Illegal statement");
            }
            return nullptr;
        }

        std::shared_ptr<ExprAST> eval_function_expr(std::shared_ptr<FunctionAST> F)
        {
            // Register function in current scope
            CurScope->set(F->Proto->Name, F);
            return F;
        }

        std::shared_ptr<ExprAST> eval_call_expr(std::shared_ptr<CallExprAST> Caller)
        {
            // Switch to sub scope
            auto C = find_name(Caller->Callee);
            if (!C)
            {
                std::string err_info = "[eval_call_expr] Undefined symbol '" + Caller->Callee + "'.";
                eval_err(err_info);
            }
            CurScope = find_name_belong_scope(Caller->Callee);

            // Get the function definition
            auto Func = std::static_pointer_cast<FunctionAST>(CurScope->get(Caller->Callee));
            // Match the parameters list
            if (Caller->Args.size() != Func->Proto->Args.size())
            {
                std::string err_info = "[eval_call_expr] Number od parameters mismatch. Expected " + std::to_string(Func->Proto->Args.size()) + " parameters.";
                eval_err(err_info);
            }

            // Creat new function environment
            enter_new_env();

            // Set parameters
            for (int i = 0; i < Caller->Args.size(); ++i)
                CurScope->set(Func->Proto->Args[i], eval_one(Caller->Args[i]));

            // Execute function body
            for (auto i : Func->Body)
            {
                // If is return expression
                if (i->SubType == Type::return_expr)
                {
                    auto ret = std::static_pointer_cast<ReturnExprAST>(i);
                    if (ret->RetValue) // Check it is just return , or not.
                    {
                        auto res = eval_one(ret->RetValue);
                        recover_prev_env(); // Exit curr environment
                        return res;
                    }
                    recover_prev_env(); // Exit curr environment
                    return nullptr;
                }
                // Otherwise execute it
                eval_one(i);
            }

            recover_prev_env(); // Exit curr environment
            return nullptr;
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
                case Type::call_expr:
                    LHS = eval_call_expr(std::static_pointer_cast<CallExprAST>(expr->LHS));
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
                case Type::call_expr:
                    RHS = eval_call_expr(std::static_pointer_cast<CallExprAST>(expr->RHS));
                    break;
                default: 
                    break;
            }

            return eval_bin_op_expr_helper(expr->Op, LHS, RHS);
        }

        std::shared_ptr<ExprAST> eval_bin_op_expr_helper(const std::string& Op, std::shared_ptr<ExprAST> LHS, std::shared_ptr<ExprAST> RHS)
        {
            if (Op == "=")
            {
                if (LHS->SubType != Type::variable_expr)
                    eval_err("[eval_bin_op_expr_helper] Expected a variable_expr before '=', rvalue is not a identifier");

                // Invoke assign(...) if need check type
                // assign(LHS, RHS);
                auto _var = std::static_pointer_cast<VariableExprAST>(LHS);
                set_name(_var->Name, RHS);
                return RHS;
            }
            /* It's a variable */
            if (LHS->SubType == Type::variable_expr)
            {
                auto _l = std::static_pointer_cast<VariableExprAST>(LHS);
                LHS = find_name(_l->Name);
                
                if (!LHS)
                {
                    std::string err_info = "[eval_bin_op_expr_helper] Undefined symbol '" + _l->Name + "'.";
                    eval_err(err_info);
                }
            }
            
            if (RHS->SubType == Type::variable_expr)
            {
                auto _r = std::static_pointer_cast<VariableExprAST>(RHS);
                RHS = find_name(_r->Name);
                if (!RHS)
                {
                    std::string err_info = "[eval_bin_op_expr_helper] Undefined symbol '" + _r->Name + "'.";
                    eval_err(err_info);
                }
            }

            // LHS, RHS is Integer or Float or String
            if (Op == "+") return add(LHS, RHS);
            if (Op == "-") return sub(LHS, RHS);
            if (Op == "*") return mul(LHS, RHS);
            if (Op == "/") return div(LHS, RHS);
            if (Op == "%") return mod(LHS, RHS);
            
            log_err("[eval_bin_op_expr_helper] Invalid parameters.");
            return nullptr;
        }

        // If need check type to assign
        std::shared_ptr<ExprAST> assign(const std::shared_ptr<VariableExprAST> LHS, const std::shared_ptr<ExprAST> RHS)
        { return nullptr; }

        std::shared_ptr<ExprAST> add(const std::shared_ptr<ExprAST> LHS, const std::shared_ptr<ExprAST> RHS)
        {
            /* Number */
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
            /* String */
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

            eval_err("[add] Invalid '+' expression.");
            return nullptr;
        }

        std::shared_ptr<ExprAST> sub(const std::shared_ptr<ExprAST> LHS, const std::shared_ptr<ExprAST> RHS)
        {
            // 1-1=0
            if (LHS->SubType == Type::integer_expr && RHS->SubType == Type::integer_expr)
                return std::make_shared<IntegerValueExprAST>(std::static_pointer_cast<IntegerValueExprAST>(LHS)->Val - std::static_pointer_cast<IntegerValueExprAST>(RHS)->Val);
            // 1-1.0=0.0
            if (LHS->SubType == Type::integer_expr && RHS->SubType == Type::float_expr)
                return std::make_shared<FloatValueExprAST>(std::static_pointer_cast<IntegerValueExprAST>(LHS)->Val - std::static_pointer_cast<FloatValueExprAST>(RHS)->Val);
            // 1.0-1=0.0
            if (LHS->SubType == Type::float_expr && RHS->SubType == Type::integer_expr) 
                return std::make_shared<FloatValueExprAST>(std::static_pointer_cast<FloatValueExprAST>(LHS)->Val - std::static_pointer_cast<IntegerValueExprAST>(RHS)->Val);
            // 1.0-1.0=0.0
            if (LHS->SubType == Type::float_expr && RHS->SubType == Type::float_expr)
                return std::make_shared<FloatValueExprAST>(std::static_pointer_cast<FloatValueExprAST>(LHS)->Val - std::static_pointer_cast<FloatValueExprAST>(RHS)->Val);

            eval_err("[sub] Invalid '-' expression.");
            return nullptr;
        }

        std::shared_ptr<ExprAST> mul(const std::shared_ptr<ExprAST> LHS, const std::shared_ptr<ExprAST> RHS)
        {
            // 1*1=1
            if (LHS->SubType == Type::integer_expr && RHS->SubType == Type::integer_expr)
                return std::make_shared<IntegerValueExprAST>(std::static_pointer_cast<IntegerValueExprAST>(LHS)->Val * std::static_pointer_cast<IntegerValueExprAST>(RHS)->Val);
            // 1*1.0=1.0
            if (LHS->SubType == Type::integer_expr && RHS->SubType == Type::float_expr)
                return std::make_shared<FloatValueExprAST>(std::static_pointer_cast<IntegerValueExprAST>(LHS)->Val * std::static_pointer_cast<FloatValueExprAST>(RHS)->Val);
            // 1.0*1=1.0
            if (LHS->SubType == Type::float_expr && RHS->SubType == Type::integer_expr) 
                return std::make_shared<FloatValueExprAST>(std::static_pointer_cast<FloatValueExprAST>(LHS)->Val * std::static_pointer_cast<IntegerValueExprAST>(RHS)->Val);
            // 1.0*1.0=1.0
            if (LHS->SubType == Type::float_expr && RHS->SubType == Type::float_expr)
                return std::make_shared<FloatValueExprAST>(std::static_pointer_cast<FloatValueExprAST>(LHS)->Val * std::static_pointer_cast<FloatValueExprAST>(RHS)->Val);

            eval_err("[mul] Invalid '*' expression.");
            return nullptr;
        }

        std::shared_ptr<ExprAST> div(const std::shared_ptr<ExprAST> LHS, const std::shared_ptr<ExprAST> RHS)
        {
            // 1/1=1
            if (LHS->SubType == Type::integer_expr && RHS->SubType == Type::integer_expr)
                return std::make_shared<IntegerValueExprAST>(std::static_pointer_cast<IntegerValueExprAST>(LHS)->Val / std::static_pointer_cast<IntegerValueExprAST>(RHS)->Val);
            // 1/1.0=1.0
            if (LHS->SubType == Type::integer_expr && RHS->SubType == Type::float_expr)
                return std::make_shared<FloatValueExprAST>(std::static_pointer_cast<IntegerValueExprAST>(LHS)->Val / std::static_pointer_cast<FloatValueExprAST>(RHS)->Val);
            // 1.0/1=1.0
            if (LHS->SubType == Type::float_expr && RHS->SubType == Type::integer_expr) 
                return std::make_shared<FloatValueExprAST>(std::static_pointer_cast<FloatValueExprAST>(LHS)->Val / std::static_pointer_cast<IntegerValueExprAST>(RHS)->Val);
            // 1.0/1.0=1.0
            if (LHS->SubType == Type::float_expr && RHS->SubType == Type::float_expr)
                return std::make_shared<FloatValueExprAST>(std::static_pointer_cast<FloatValueExprAST>(LHS)->Val / std::static_pointer_cast<FloatValueExprAST>(RHS)->Val);

            eval_err("[div] Invalid '/' expression.");
            return nullptr;
        }

        std::shared_ptr<ExprAST> mod(const std::shared_ptr<ExprAST> LHS, const std::shared_ptr<ExprAST> RHS)
        {
            // 1%1=0
            if (LHS->SubType == Type::integer_expr && RHS->SubType == Type::integer_expr)
                return std::make_shared<IntegerValueExprAST>(std::static_pointer_cast<IntegerValueExprAST>(LHS)->Val % std::static_pointer_cast<IntegerValueExprAST>(RHS)->Val);
            // 1%1.0=0.0
            if (LHS->SubType == Type::integer_expr && RHS->SubType == Type::float_expr)
                return std::make_shared<FloatValueExprAST>(fmod(std::static_pointer_cast<IntegerValueExprAST>(LHS)->Val, std::static_pointer_cast<FloatValueExprAST>(RHS)->Val));
            // 1.0%1=0.0
            if (LHS->SubType == Type::float_expr && RHS->SubType == Type::integer_expr) 
                return std::make_shared<FloatValueExprAST>(fmod(std::static_pointer_cast<FloatValueExprAST>(LHS)->Val, std::static_pointer_cast<IntegerValueExprAST>(RHS)->Val));
            // 1.0%1.0=0.0
            if (LHS->SubType == Type::float_expr && RHS->SubType == Type::float_expr)
                return std::make_shared<FloatValueExprAST>(fmod(std::static_pointer_cast<FloatValueExprAST>(LHS)->Val, std::static_pointer_cast<FloatValueExprAST>(RHS)->Val));

            eval_err("[mod] Invalid '%' expression.");
            return nullptr;
        }

        void eval_err(const std::string& loginfo)
        {
            std::cerr << loginfo << std::endl;
            exit(1);
        }

    };
}

#endif