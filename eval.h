#ifndef TINYJS_EVAL
#define TINYJS_EVAL

#include <cmath>
#include <string>
#include "env.h"
#include "ast.h"
#include "log.h"
#include "built_in.h"

// #define elog

namespace Eval
{
    using namespace AST;
    using namespace Env;
    using namespace BuiltIn;

    class EvalImpl : public BuiltInImpl
    {
    using IntType = long long;
    using T = std::vector<std::shared_ptr<ExprAST>>;
    using EnvImpl = EnvImpl<T::value_type>;
    
    private:
        std::shared_ptr<EnvImpl> Scope;
        std::shared_ptr<EnvImpl> CurScope;
        const std::string TopScope = "__top_expression";
        T Expression; // T := vector<unique_ptr<ExprAST>>
        unsigned long long EvalLineNumber;
        std::string ERR_INFO;

    public:
        EvalImpl() = delete;
        EvalImpl(T Expression) : Expression(std::move(Expression)) 
        {
            BuiltInImpl();
            if (!Scope)
            {
                CurScope.reset(new EnvImpl(TopScope));
                Scope = CurScope;
                EvalLineNumber = 1;
                ERR_INFO = "";
            }
        }
        ~EvalImpl() = default;
        
        EvalImpl(const EvalImpl&) = delete;
        const EvalImpl& operator =(const EvalImpl&) = delete;
        EvalImpl(EvalImpl&&) = delete;
        const EvalImpl& operator =(EvalImpl&&) = delete;

        /* Attention !!! Wait for rewrite !!! */
        std::shared_ptr<ExprAST> exec_built_in(std::shared_ptr<ExprAST> Func)
        {
            auto F = ptr_to<CallExprAST>(Func);
            auto Name = F->Callee;
            if (Name == "print")
            {
                for (int i = 0; i < F->Args.size(); i++)
                {
                    auto arg = F->Args[i];
                    print_value(eval_expression(arg));
                }
            }
            return F;
        }

        /* -- Scope -- */
        std::shared_ptr<EnvImpl> get_top_scope()
        {
            auto _CurScope = CurScope;
            while (_CurScope->Parent)
                _CurScope = _CurScope->Parent;
            return _CurScope;
        }

        void enter_new_env()
        { CurScope.reset(new EnvImpl(CurScope)); }
        void enter_new_env(const std::string& Name)
        { CurScope.reset(new EnvImpl(Name, CurScope)); }

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

        bool is_top_scope()
        { return CurScope->Parent ? false : true; }
        /* ++ Scope ++ */

        bool is_interrupt_control_flow(std::shared_ptr<ExprAST> E)
        {
            switch (E->SubType)
            {
                case Type::break_expr: case Type::continue_expr: case Type::return_expr:
                    if (is_top_scope())
                        eval_err("Uncaught SyntaxError: Illegal " + E->get_ast_name() + " statement");
                    return true;
                default:
                    return false;
            }
        }

        /* -- Name -- */
        // Find exist name
        // if (!find_name()) => Check name is exist?
        std::shared_ptr<ExprAST> find_name(const std::string& Name)
        { return find_name_belong_scope(Name)->get(Name); }

        // Set a variable or function
        void set_name(const std::string& Name, std::shared_ptr<ExprAST> Value)
        { find_name_belong_scope(Name)->set(Name, Value); }

        // get name
        std::string get_name(std::shared_ptr<ExprAST> V)
        {
            switch (V->SubType)
            {
                case Type::variable_expr:
                    return ptr_to<VariableExprAST>(V)->Name;
                case Type::binary_op_expr:
                    return get_name(ptr_to<BinaryOpExprAST>(V)->LHS);
                case Type::call_expr:
                    return ptr_to<CallExprAST>(V)->Callee;
                case Type::function_expr:
                    return ptr_to<FunctionAST>(V)->Proto->Name;
                default:
                    return "";
            }
        }
        /* ++ Name ++ */

        // Type conversion: integer float string => bool
        bool value_to_bool(std::shared_ptr<ExprAST> V)
        {
        #ifdef elog
            log("in value_to_bool");
        #endif
            switch (V->SubType)
            {
                case Type::integer_expr:
                    if (ptr_to<IntegerValueExprAST>(V)->Val)
                        return true;
                    return false;
                case Type::float_expr:
                    if (ptr_to<FloatValueExprAST>(V)->Val)
                        return true;
                    return false;
                case Type::string_expr:
                    if (ptr_to<StringValueExprAST>(V)->Val.length())
                        return true;
                    return false;
                case Type::variable_expr:
                {
                    auto v = ptr_to<VariableExprAST>(V);
                    auto _v = find_name(v->Name);
                    if (!_v)
                    {
                        ERR_INFO = "[value_to_bool] ReferenceError: " + v->Name + " is not defined.";
                        eval_err(ERR_INFO);
                    }
                    return value_to_bool(_v);
                }
                default:
                    return false;
            }
            return false;
        }

        // Point transform
        template <typename T>
        inline std::shared_ptr<T> ptr_to(std::shared_ptr<ExprAST> P)
        { return std::static_pointer_cast<T>(P); }

        /* -- Value -- */
        void print_value(std::shared_ptr<ExprAST> V)
        {
            switch (V->SubType)
            {
                case Type::integer_expr:
                    std::cout << get_value<IntegerValueExprAST>(V) << std::endl;
                    break;
                case Type::float_expr:
                    std::cout << get_value<FloatValueExprAST>(V) << std::endl;
                    break;
                case Type::string_expr:
                    std::cout << get_value<StringValueExprAST>(V) << std::endl;
                    break;
                case Type::variable_expr:
                {
                    auto _v = ptr_to<VariableExprAST>(V);
                    auto _var = get_variable_value(_v);
                    if (!_var)
                    {
                        std::cout << "[warnning] Variable '"<< _v->Name << "' = undefined." << std::endl;
                        return;
                    }

                    std::cout << "Variable '" << _v->Name << "' = ";
                    print_value(_var);
                    break;
                }
                default:
                    std::cout << "[print_value] ExprAST Undefined." << std::endl;
                    V->print_ast();
                    break;
            }
        }

        // get Integer or Float or String
        template <typename T>
        typename T::value_type get_value(std::shared_ptr<ExprAST> V)
        {
            return ptr_to<T>(V)->Val;
        }

        std::shared_ptr<ExprAST> get_variable_value(std::shared_ptr<ExprAST> V)
        {
            auto v = find_name(ptr_to<VariableExprAST>(V)->Name);
            if (!v) return nullptr;
            return v;
        }
        /* ++ Value ++ */

        std::shared_ptr<ExprAST> eval_function_expr(std::shared_ptr<FunctionAST> F);
        std::shared_ptr<ExprAST> eval_return(std::shared_ptr<ReturnExprAST> R);
        std::shared_ptr<ExprAST> eval_if_else(std::shared_ptr<IfExprAST> If);
        std::shared_ptr<ExprAST> eval_for(std::shared_ptr<ForExprAST> For);
        std::shared_ptr<ExprAST> eval_while(std::shared_ptr<WhileExprAST> While);
        std::shared_ptr<ExprAST> eval_do_while(std::shared_ptr<DoWhileExprAST> DoWhile);
        std::shared_ptr<ExprAST> eval_call_expr(std::shared_ptr<CallExprAST> Caller);
        std::shared_ptr<ExprAST> eval_unary_op_expr(std::shared_ptr<UnaryOpExprAST> expr);
        /* Binary op expr */
        std::shared_ptr<ExprAST> eval_binary_op_expr(std::shared_ptr<BinaryOpExprAST> expr);
        std::shared_ptr<ExprAST> eval_one_bin_op_expr(std::shared_ptr<ExprAST> E);
        std::shared_ptr<ExprAST> eval_bin_op_expr_helper(const std::string& Op, std::shared_ptr<ExprAST> LHS, std::shared_ptr<ExprAST> RHS);
        /* Block */
        std::shared_ptr<ExprAST> eval_block(std::vector<std::shared_ptr<ExprAST>>& Statement);

        void eval()
        {
            for (auto& i : Expression)
            {
                EvalLineNumber = i->LineNumber;
                eval_one(i);
            }
        }

        // API (Interpreter)
        std::shared_ptr<ExprAST> eval_one(std::shared_ptr<ExprAST> expr)
        {
            switch (expr->SubType)
            {
                case Type::function_expr:
                    return eval_function_expr(ptr_to<FunctionAST>(expr));
                default:
                    return eval_expression(expr);
            }
            return nullptr;
        }

        std::shared_ptr<ExprAST> eval_expression(std::shared_ptr<ExprAST> E)
        {
            switch (E->SubType)
            {
                case Type::return_expr: case Type::break_expr: case Type::continue_expr:
                case Type::integer_expr: case Type::float_expr: case Type::string_expr:
                case Type::variable_expr:
                    return E;
                case Type::if_else_expr:
                    return eval_if_else(ptr_to<IfExprAST>(E));
                case Type::for_expr:
                    return eval_for(ptr_to<ForExprAST>(E));
                case Type::while_expr:
                    return eval_while(ptr_to<WhileExprAST>(E));
                case Type::do_while_expr:
                    return eval_do_while(ptr_to<DoWhileExprAST>(E));
                case Type::unary_op_expr:
                    return eval_unary_op_expr(ptr_to<UnaryOpExprAST>(E));
                case Type::binary_op_expr:
                    return eval_binary_op_expr(ptr_to<BinaryOpExprAST>(E));
                case Type::call_expr:
                    return eval_call_expr(ptr_to<CallExprAST>(E));
                default:
                    E->print_ast();
                    eval_err("Illegal statement");
            }
            return nullptr;
        }

        void eval_err(const std::string& loginfo)
        {
            std::cerr << "[Eval Error] in line: " << EvalLineNumber << std::endl;
            std::cerr << loginfo << std::endl;
            exit(1);
        }

        // If need check type to assign
        std::shared_ptr<ExprAST> assign(const std::shared_ptr<VariableExprAST> LHS, const std::shared_ptr<ExprAST> RHS)
        { return nullptr; }

        std::shared_ptr<ExprAST> _add(const std::shared_ptr<ExprAST> LHS, const std::shared_ptr<ExprAST> RHS);
        std::shared_ptr<ExprAST> _sub(const std::shared_ptr<ExprAST> LHS, const std::shared_ptr<ExprAST> RHS);
        std::shared_ptr<ExprAST> _mul(const std::shared_ptr<ExprAST> LHS, const std::shared_ptr<ExprAST> RHS);
        std::shared_ptr<ExprAST> _div(const std::shared_ptr<ExprAST> LHS, const std::shared_ptr<ExprAST> RHS);
        std::shared_ptr<ExprAST> _mod(const std::shared_ptr<ExprAST> LHS, const std::shared_ptr<ExprAST> RHS);
        std::shared_ptr<ExprAST> _not(const std::shared_ptr<ExprAST> RHS); /* '!' */

        std::shared_ptr<ExprAST> _greater(const std::shared_ptr<ExprAST> LHS, const std::shared_ptr<ExprAST> RHS);
        std::shared_ptr<ExprAST> _less(const std::shared_ptr<ExprAST> LHS, const std::shared_ptr<ExprAST> RHS);
        std::shared_ptr<ExprAST> _not_more(const std::shared_ptr<ExprAST> LHS, const std::shared_ptr<ExprAST> RHS);
        std::shared_ptr<ExprAST> _not_less(const std::shared_ptr<ExprAST> LHS, const std::shared_ptr<ExprAST> RHS);
        std::shared_ptr<ExprAST> _equal(const std::shared_ptr<ExprAST> LHS, const std::shared_ptr<ExprAST> RHS);

        std::shared_ptr<ExprAST> _and(const std::shared_ptr<ExprAST> LHS, const std::shared_ptr<ExprAST> RHS);
        std::shared_ptr<ExprAST> _or(const std::shared_ptr<ExprAST> LHS, const std::shared_ptr<ExprAST> RHS);
        std::shared_ptr<ExprAST> _bit_rshift(const std::shared_ptr<ExprAST> LHS, const std::shared_ptr<ExprAST> RHS);
        std::shared_ptr<ExprAST> _bit_lshift(const std::shared_ptr<ExprAST> LHS, const std::shared_ptr<ExprAST> RHS);
        std::shared_ptr<ExprAST> _bit_and(const std::shared_ptr<ExprAST> LHS, const std::shared_ptr<ExprAST> RHS);
        std::shared_ptr<ExprAST> _bit_or(const std::shared_ptr<ExprAST> LHS, const std::shared_ptr<ExprAST> RHS);
        std::shared_ptr<ExprAST> _bit_xor(const std::shared_ptr<ExprAST> LHS, const std::shared_ptr<ExprAST> RHS);
        std::shared_ptr<ExprAST> _bit_not(const std::shared_ptr<ExprAST> RHS); /* '~' */

    };
}

#endif