#include "eval.h"
using namespace Eval;

inline std::shared_ptr<ExprAST> EvalImpl::eval_block(std::vector<std::shared_ptr<ExprAST>>& Statement)
{
    std::shared_ptr<ExprAST> ret;
    for (auto i : Statement)
    {
        EvalLineNumber = i->LineNumber;
        ret = eval_one(i);
        switch (ret->SubType)
        {
            case Type::return_expr: case Type::break_expr: case Type::continue_expr:
                return ret;
            default:
                break;
        }
    }
    return ret;
}

inline std::shared_ptr<ExprAST> EvalImpl::eval_function_expr(std::shared_ptr<FunctionAST> F)
{
    // Register function in current scope
    CurScope->set(F->Proto->Name, F);
    return F;
}

std::shared_ptr<ExprAST> EvalImpl::eval_if_else(std::shared_ptr<IfExprAST> If)
{
    enter_new_env();

    auto Cond = eval_one(If->Cond);
    bool cond_bool = true;
    if (!Cond) 
        cond_bool = false;
    else
    {
        switch (Cond->SubType)
        {
            case Type::integer_expr:
            case Type::float_expr:
            case Type::string_expr:
            case Type::variable_expr:
                cond_bool = value_to_bool(Cond);
                break;
            default:
                Cond->print_ast();
                eval_err("[eval_if_else] Syntax Error.");
        }
    }

    // Execute if-statement
    if (cond_bool)
    {
        auto R = eval_block(If->IfBlock->Statement);
        can_break_control_flow(R);
        recover_prev_env();
        return R;
    }

    // Execute else-if
    if (If->ElseIf)
    {
        auto R = eval_if_else(std::static_pointer_cast<IfExprAST>(If->ElseIf));
        recover_prev_env();
        return R;
    }

    // Execute else-statement
    if (!If->ElseBlock->Statement.empty())
    {
        auto R = eval_block(If->ElseBlock->Statement);
        can_break_control_flow(R);
        recover_prev_env();
        return R;
    }

    recover_prev_env();
    return nullptr;
}

std::shared_ptr<ExprAST> EvalImpl::eval_for(std::shared_ptr<ForExprAST> For)
{
    enter_new_env();

    auto InitCond = eval_expression(For->Cond[0]);
    if (!For->Block->Statement.empty())
    {
        while (value_to_bool(eval_expression(For->Cond[1])))
        {
            eval_block(For->Block->Statement);
            eval_expression(For->Cond[2]);
        }
    }

    recover_prev_env();
    return nullptr;
}

std::shared_ptr<ExprAST> EvalImpl::eval_while(std::shared_ptr<WhileExprAST> While)
{
    return nullptr;
}

std::shared_ptr<ExprAST> EvalImpl::eval_do_while(std::shared_ptr<DoWhileExprAST> DoWhile)
{
    return nullptr;
}

std::shared_ptr<ExprAST> EvalImpl::eval_call_expr(std::shared_ptr<CallExprAST> Caller)
{
    // If is built in function
    if (is_built_in(Caller->Callee))
        return exec_built_in(Caller);

    // Switch to sub scope
    CurScope = find_name_belong_scope(Caller->Callee);
    auto F = CurScope->get(Caller->Callee);
    if (!F)
    {
        ERR_INFO = "[eval_call_expr] ReferenceError: '" + Caller->Callee + "' is not defined. ";
        eval_err(ERR_INFO);
    }

    // Get the function definition
    auto Func = std::static_pointer_cast<FunctionAST>(F);

    // Creat new function environment
    enter_new_env();

    // Set parameters
    if (Caller->Args.size() >= Func->Proto->Args.size())
    {
        for (int i = 0; i < Func->Proto->Args.size(); ++i)
            CurScope->set(get_name(Func->Proto->Args[i]), eval_expression(Caller->Args[i]));        
    }
    else
    {
        for (int i = 0; i < Caller->Args.size(); i++)
            CurScope->set(get_name(Func->Proto->Args[i]), eval_expression(Caller->Args[i])); 

        for (int i = Caller->Args.size(); i < Func->Proto->Args.size(); i++)
            eval_expression(Func->Proto->Args[i]);
    }

    // Execute function body
    auto ret = eval_block(Func->Body->Statement);
    switch (ret->SubType)
    {
        case Type::return_expr:
        {
            auto res = std::static_pointer_cast<ReturnExprAST>(ret);
            if (res->RetValue) // Check it is just return , or not.
            {
                auto ans = eval_one(res->RetValue);
                recover_prev_env(); // Exit curr environment
                return ans;
            }
            recover_prev_env(); // Exit curr environment
            return nullptr;
        }
        case Type::break_expr:
            eval_err("Uncaught SyntaxError: Illegal break statement");
        case Type::continue_expr:
            eval_err("Uncaught SyntaxError: Illegal continue statement");
        default:
            break;
    }

    recover_prev_env(); // Exit curr environment
    return nullptr;
}

std::shared_ptr<ExprAST> EvalImpl::eval_unary_op_expr(std::shared_ptr<UnaryOpExprAST> expr)
{
    auto Op = expr->Op;
    auto E = eval_one(expr->Expression);
    std::shared_ptr<ExprAST> _v = E;
    if (E->SubType == Type::variable_expr)
    {
        auto _e = std::static_pointer_cast<VariableExprAST>(E);
        _v = get_variable_value(_e);
        if (!_v)
        {
            ERR_INFO = "[eval_unary_op_expr] ReferenceError '" + _e->Name + "' is not defined.";
            eval_err(ERR_INFO);
        }
    }

    if (Op == "-") return _mul(_v, std::make_shared<IntegerValueExprAST>(-1));
    if (Op == "+") return _mul(_v, std::make_shared<IntegerValueExprAST>(1));
    if (Op == "~") return _bit_not(_v);
    if (Op == "!") return _not(_v);
    return nullptr;
}

std::shared_ptr<ExprAST> EvalImpl::eval_binary_op_expr(std::shared_ptr<BinaryOpExprAST> expr)
{
    auto LHS = eval_expression(expr->LHS);
    auto RHS = eval_expression(expr->RHS);

    // null => 0
    if (!LHS) LHS = std::make_shared<IntegerValueExprAST>(0);
    if (!RHS) RHS = std::make_shared<IntegerValueExprAST>(0);
    return eval_bin_op_expr_helper(expr->Op, LHS, RHS);
}


// Calculation of evaluation
std::shared_ptr<ExprAST> EvalImpl::eval_bin_op_expr_helper(const std::string& Op, std::shared_ptr<ExprAST> LHS, std::shared_ptr<ExprAST> RHS)
{
    if (Op == "=")
    {
        if (LHS->SubType != Type::variable_expr)
            eval_err("[eval_bin_op_expr_helper] Expected a variable_expr before '=', rvalue is not a identifier. ");

        // Invoke assign(...) if need check type
        // assign(LHS, RHS);
        auto _var = std::static_pointer_cast<VariableExprAST>(LHS);
        if (_var->DefineType == "var")
            get_top_scope()->set(_var->Name, RHS);
        else if (_var->DefineType == "let")
            CurScope->set(_var->Name, RHS);
        else /* auto set variable, local first. */
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
            ERR_INFO = "[eval_bin_op_expr_helper] ReferenceError: '" + _l->Name + "' is not defined. ";
            eval_err(ERR_INFO);
        }
    }
    
    if (RHS->SubType == Type::variable_expr)
    {
        auto _r = std::static_pointer_cast<VariableExprAST>(RHS);
        RHS = find_name(_r->Name);
        if (!RHS)
        {
            ERR_INFO = "[eval_bin_op_expr_helper] ReferenceError: '" + _r->Name + "' is not defined. ";
            eval_err(ERR_INFO);
        }
    }

    // LHS, RHS is Integer or Float or String or Expression
    if (Op == ",")
    {
        eval_expression(LHS);
        return eval_expression(RHS);
    }
    if (Op == "+")  return _add(LHS, RHS);
    if (Op == "-")  return _sub(LHS, RHS);
    if (Op == "*")  return _mul(LHS, RHS);
    if (Op == "/")  return _div(LHS, RHS);
    if (Op == ">")  return _greater(LHS, RHS);
    if (Op == "<")  return _less(LHS, RHS);
    if (Op == ">=") return _not_more(LHS, RHS);
    if (Op == "<=") return _not_less(LHS, RHS);
    if (Op == "==") return _equal(LHS, RHS);
    if (Op == "%")  return _mod(LHS, RHS);
    if (Op == "&&") return _and(LHS, RHS);
    if (Op == "||") return _or(LHS, RHS);
    if (Op == ">>") return _bit_rshift(LHS, RHS);
    if (Op == "<<") return _bit_lshift(LHS, RHS);
    if (Op == "&")  return _bit_and(LHS, RHS);
    if (Op == "|")  return _bit_or(LHS, RHS);
    if (Op == "^")  return _bit_xor(LHS, RHS);
    
    ERR_INFO = "[eval_bin_op_expr_helper] '" + Op + "' is invalid operator.";
    eval_err(ERR_INFO);
    return nullptr;
}


std::shared_ptr<ExprAST> EvalImpl::_add(const std::shared_ptr<ExprAST> LHS, const std::shared_ptr<ExprAST> RHS)
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

    LHS->print_ast(); RHS->print_ast();
    eval_err("[_add] Invalid '+' expression.");
    return nullptr;
}

std::shared_ptr<ExprAST> EvalImpl::_sub(const std::shared_ptr<ExprAST> LHS, const std::shared_ptr<ExprAST> RHS)
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

    LHS->print_ast(); RHS->print_ast();
    eval_err("[_sub] Invalid '-' expression.");
    return nullptr;
}

std::shared_ptr<ExprAST> EvalImpl::_mul(const std::shared_ptr<ExprAST> LHS, const std::shared_ptr<ExprAST> RHS)
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

    LHS->print_ast(); RHS->print_ast();
    eval_err("[_mul] Invalid '*' expression.");
    return nullptr;
}

std::shared_ptr<ExprAST> EvalImpl::_div(const std::shared_ptr<ExprAST> LHS, const std::shared_ptr<ExprAST> RHS)
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

    LHS->print_ast(); RHS->print_ast();
    eval_err("[_div] Invalid '/' expression.");
    return nullptr;
}

std::shared_ptr<ExprAST> EvalImpl::_mod(const std::shared_ptr<ExprAST> LHS, const std::shared_ptr<ExprAST> RHS)
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

    LHS->print_ast(); RHS->print_ast();
    eval_err("[_mod] Invalid \'%\' expression.");
    return nullptr;
}

/* '!' */
inline std::shared_ptr<ExprAST> EvalImpl::_not(const std::shared_ptr<ExprAST> RHS)
{
    return std::make_shared<IntegerValueExprAST>(!value_to_bool(RHS));
}

/* >  <  >=  <=  == */
std::shared_ptr<ExprAST> EvalImpl::_greater(const std::shared_ptr<ExprAST> LHS, const std::shared_ptr<ExprAST> RHS)
{
    if (LHS->SubType == Type::integer_expr && RHS->SubType == Type::integer_expr)
        return std::make_shared<IntegerValueExprAST>(get_value<IntegerValueExprAST>(LHS) > get_value<IntegerValueExprAST>(RHS));

    if (LHS->SubType == Type::float_expr && RHS->SubType == Type::integer_expr)
        return std::make_shared<IntegerValueExprAST>(get_value<FloatValueExprAST>(LHS) > get_value<IntegerValueExprAST>(RHS));

    if (LHS->SubType == Type::integer_expr && RHS->SubType == Type::float_expr)
        return std::make_shared<IntegerValueExprAST>(get_value<IntegerValueExprAST>(LHS) > get_value<FloatValueExprAST>(RHS));

    if (LHS->SubType == Type::float_expr && RHS->SubType == Type::float_expr)
        return std::make_shared<IntegerValueExprAST>(get_value<FloatValueExprAST>(LHS) > get_value<FloatValueExprAST>(RHS));

    LHS->print_ast(); RHS->print_ast();
    eval_err("[_greater] Invalid '>' expression.");
    return nullptr;
}

std::shared_ptr<ExprAST> EvalImpl::_less(const std::shared_ptr<ExprAST> LHS, const std::shared_ptr<ExprAST> RHS)
{
    if (LHS->SubType == Type::integer_expr && RHS->SubType == Type::integer_expr)
        return std::make_shared<IntegerValueExprAST>(get_value<IntegerValueExprAST>(LHS) < get_value<IntegerValueExprAST>(RHS));

    if (LHS->SubType == Type::float_expr && RHS->SubType == Type::integer_expr)
        return std::make_shared<IntegerValueExprAST>(get_value<FloatValueExprAST>(LHS) < get_value<IntegerValueExprAST>(RHS));

    if (LHS->SubType == Type::integer_expr && RHS->SubType == Type::float_expr)
        return std::make_shared<IntegerValueExprAST>(get_value<IntegerValueExprAST>(LHS) < get_value<FloatValueExprAST>(RHS));

    if (LHS->SubType == Type::float_expr && RHS->SubType == Type::float_expr)
        return std::make_shared<IntegerValueExprAST>(get_value<FloatValueExprAST>(LHS) < get_value<FloatValueExprAST>(RHS));

    LHS->print_ast(); RHS->print_ast();
    eval_err("[_less] Invalid '<' expression.");
    return nullptr;
}

std::shared_ptr<ExprAST> EvalImpl::_not_more(const std::shared_ptr<ExprAST> LHS, const std::shared_ptr<ExprAST> RHS)
{
    if (LHS->SubType == Type::integer_expr && RHS->SubType == Type::integer_expr)
        return std::make_shared<IntegerValueExprAST>(get_value<IntegerValueExprAST>(LHS) <= get_value<IntegerValueExprAST>(RHS));

    if (LHS->SubType == Type::float_expr && RHS->SubType == Type::integer_expr)
        return std::make_shared<IntegerValueExprAST>(get_value<FloatValueExprAST>(LHS) <= get_value<IntegerValueExprAST>(RHS));

    if (LHS->SubType == Type::integer_expr && RHS->SubType == Type::float_expr)
        return std::make_shared<IntegerValueExprAST>(get_value<IntegerValueExprAST>(LHS) <= get_value<FloatValueExprAST>(RHS));

    if (LHS->SubType == Type::float_expr && RHS->SubType == Type::float_expr)
        return std::make_shared<IntegerValueExprAST>(get_value<FloatValueExprAST>(LHS) <= get_value<FloatValueExprAST>(RHS));

    LHS->print_ast(); RHS->print_ast();
    eval_err("[_not_more] Invalid '<=' expression.");
    return nullptr;
}

std::shared_ptr<ExprAST> EvalImpl::_not_less(const std::shared_ptr<ExprAST> LHS, const std::shared_ptr<ExprAST> RHS)
{
    if (LHS->SubType == Type::integer_expr && RHS->SubType == Type::integer_expr)
        return std::make_shared<IntegerValueExprAST>(get_value<IntegerValueExprAST>(LHS) >= get_value<IntegerValueExprAST>(RHS));

    if (LHS->SubType == Type::float_expr && RHS->SubType == Type::integer_expr)
        return std::make_shared<IntegerValueExprAST>(get_value<FloatValueExprAST>(LHS) >= get_value<IntegerValueExprAST>(RHS));

    if (LHS->SubType == Type::integer_expr && RHS->SubType == Type::float_expr)
        return std::make_shared<IntegerValueExprAST>(get_value<IntegerValueExprAST>(LHS) >= get_value<FloatValueExprAST>(RHS));

    if (LHS->SubType == Type::float_expr && RHS->SubType == Type::float_expr)
        return std::make_shared<IntegerValueExprAST>(get_value<FloatValueExprAST>(LHS) >= get_value<FloatValueExprAST>(RHS));

    LHS->print_ast(); RHS->print_ast();
    eval_err("[_not_less] Invalid '>=' expression.");
    return nullptr;
}

std::shared_ptr<ExprAST> EvalImpl::_equal(const std::shared_ptr<ExprAST> LHS, const std::shared_ptr<ExprAST> RHS)
{
    if (LHS->SubType == Type::integer_expr && RHS->SubType == Type::integer_expr)
        return std::make_shared<IntegerValueExprAST>(get_value<IntegerValueExprAST>(LHS) == get_value<IntegerValueExprAST>(RHS));

    if (LHS->SubType == Type::float_expr && RHS->SubType == Type::integer_expr)
        return std::make_shared<IntegerValueExprAST>(get_value<FloatValueExprAST>(LHS) == get_value<IntegerValueExprAST>(RHS));

    if (LHS->SubType == Type::integer_expr && RHS->SubType == Type::float_expr)
        return std::make_shared<IntegerValueExprAST>(get_value<IntegerValueExprAST>(LHS) == get_value<FloatValueExprAST>(RHS));

    if (LHS->SubType == Type::float_expr && RHS->SubType == Type::float_expr)
        return std::make_shared<IntegerValueExprAST>(get_value<FloatValueExprAST>(LHS) == get_value<FloatValueExprAST>(RHS));

    if (LHS->SubType == Type::string_expr && RHS->SubType == Type::string_expr)
        return std::make_shared<IntegerValueExprAST>(get_value<StringValueExprAST>(LHS) == get_value<StringValueExprAST>(RHS)); 

    LHS->print_ast(); RHS->print_ast();
    eval_err("[_equal] Invalid '==' expression.");
    return nullptr;
}



/* & | << >> ^ ~ */
inline std::shared_ptr<ExprAST> EvalImpl::_and(const std::shared_ptr<ExprAST> LHS, const std::shared_ptr<ExprAST> RHS)
{
    if (value_to_bool(LHS))
        return RHS;
    return LHS;
}

inline std::shared_ptr<ExprAST> EvalImpl::_or(const std::shared_ptr<ExprAST> LHS, const std::shared_ptr<ExprAST> RHS)
{
    if (value_to_bool(LHS))
        return LHS;
    return RHS;
}

std::shared_ptr<ExprAST> EvalImpl::_bit_rshift(const std::shared_ptr<ExprAST> LHS, const std::shared_ptr<ExprAST> RHS)
{
    if (LHS->SubType == Type::integer_expr && RHS->SubType == Type::integer_expr)
        return std::make_shared<IntegerValueExprAST>(get_value<IntegerValueExprAST>(LHS) >> get_value<IntegerValueExprAST>(RHS));

    if (LHS->SubType == Type::float_expr && RHS->SubType == Type::integer_expr)
        return std::make_shared<IntegerValueExprAST>((IntType)(get_value<FloatValueExprAST>(LHS)) >> get_value<IntegerValueExprAST>(RHS));

    if (LHS->SubType == Type::integer_expr && RHS->SubType == Type::float_expr)
        return std::make_shared<IntegerValueExprAST>(get_value<IntegerValueExprAST>(LHS) >> (IntType)(get_value<FloatValueExprAST>(RHS)));

    if (LHS->SubType == Type::float_expr && RHS->SubType == Type::float_expr)
        return std::make_shared<IntegerValueExprAST>((IntType)get_value<FloatValueExprAST>(LHS) >> (IntType)(get_value<FloatValueExprAST>(RHS)));

    LHS->print_ast(); RHS->print_ast();
    eval_err("[_bit_rshift] Invalid '>>' expression.");
    return nullptr;
}

std::shared_ptr<ExprAST> EvalImpl::_bit_lshift(const std::shared_ptr<ExprAST> LHS, const std::shared_ptr<ExprAST> RHS)
{
    if (LHS->SubType == Type::integer_expr && RHS->SubType == Type::integer_expr)
        return std::make_shared<IntegerValueExprAST>(get_value<IntegerValueExprAST>(LHS) << get_value<IntegerValueExprAST>(RHS));

    if (LHS->SubType == Type::float_expr && RHS->SubType == Type::integer_expr)
        return std::make_shared<IntegerValueExprAST>((IntType)(get_value<FloatValueExprAST>(LHS)) << get_value<IntegerValueExprAST>(RHS));

    if (LHS->SubType == Type::integer_expr && RHS->SubType == Type::float_expr)
        return std::make_shared<IntegerValueExprAST>(get_value<IntegerValueExprAST>(LHS) << (IntType)(get_value<FloatValueExprAST>(RHS)));

    if (LHS->SubType == Type::float_expr && RHS->SubType == Type::float_expr)
        return std::make_shared<IntegerValueExprAST>((IntType)get_value<FloatValueExprAST>(LHS) << (IntType)(get_value<FloatValueExprAST>(RHS)));

    LHS->print_ast(); RHS->print_ast();
    eval_err("[_bit_lshift] Invalid '<<' expression.");
    return nullptr;
}

std::shared_ptr<ExprAST> EvalImpl::_bit_and(const std::shared_ptr<ExprAST> LHS, const std::shared_ptr<ExprAST> RHS)
{
    if (LHS->SubType == Type::integer_expr && RHS->SubType == Type::integer_expr)
        return std::make_shared<IntegerValueExprAST>(get_value<IntegerValueExprAST>(LHS) & get_value<IntegerValueExprAST>(RHS));

    if (LHS->SubType == Type::float_expr && RHS->SubType == Type::integer_expr)
        return std::make_shared<IntegerValueExprAST>((IntType)(get_value<FloatValueExprAST>(LHS)) & get_value<IntegerValueExprAST>(RHS));

    if (LHS->SubType == Type::integer_expr && RHS->SubType == Type::float_expr)
        return std::make_shared<IntegerValueExprAST>(get_value<IntegerValueExprAST>(LHS) & (IntType)(get_value<FloatValueExprAST>(RHS)));

    if (LHS->SubType == Type::float_expr && RHS->SubType == Type::float_expr)
        return std::make_shared<IntegerValueExprAST>((IntType)get_value<FloatValueExprAST>(LHS) & (IntType)(get_value<FloatValueExprAST>(RHS)));

    LHS->print_ast(); RHS->print_ast();
    eval_err("[_bit_and] Invalid '&' expression.");
    return nullptr;
}

std::shared_ptr<ExprAST> EvalImpl::_bit_or(const std::shared_ptr<ExprAST> LHS, const std::shared_ptr<ExprAST> RHS)
{
    if (LHS->SubType == Type::integer_expr && RHS->SubType == Type::integer_expr)
        return std::make_shared<IntegerValueExprAST>(get_value<IntegerValueExprAST>(LHS) | get_value<IntegerValueExprAST>(RHS));

    if (LHS->SubType == Type::float_expr && RHS->SubType == Type::integer_expr)
        return std::make_shared<IntegerValueExprAST>((IntType)(get_value<FloatValueExprAST>(LHS)) | get_value<IntegerValueExprAST>(RHS));

    if (LHS->SubType == Type::integer_expr && RHS->SubType == Type::float_expr)
        return std::make_shared<IntegerValueExprAST>(get_value<IntegerValueExprAST>(LHS) | (IntType)(get_value<FloatValueExprAST>(RHS)));

    if (LHS->SubType == Type::float_expr && RHS->SubType == Type::float_expr)
        return std::make_shared<IntegerValueExprAST>((IntType)get_value<FloatValueExprAST>(LHS) | (IntType)(get_value<FloatValueExprAST>(RHS)));

    LHS->print_ast(); RHS->print_ast();
    eval_err("[_bit_or] Invalid '|' expression.");
    return nullptr;
}

std::shared_ptr<ExprAST> EvalImpl::_bit_xor(const std::shared_ptr<ExprAST> LHS, const std::shared_ptr<ExprAST> RHS)
{
    if (LHS->SubType == Type::integer_expr && RHS->SubType == Type::integer_expr)
        return std::make_shared<IntegerValueExprAST>(get_value<IntegerValueExprAST>(LHS) ^ get_value<IntegerValueExprAST>(RHS));

    if (LHS->SubType == Type::float_expr && RHS->SubType == Type::integer_expr)
        return std::make_shared<IntegerValueExprAST>((IntType)(get_value<FloatValueExprAST>(LHS)) ^ get_value<IntegerValueExprAST>(RHS));

    if (LHS->SubType == Type::integer_expr && RHS->SubType == Type::float_expr)
        return std::make_shared<IntegerValueExprAST>(get_value<IntegerValueExprAST>(LHS) ^ (IntType)(get_value<FloatValueExprAST>(RHS)));

    if (LHS->SubType == Type::float_expr && RHS->SubType == Type::float_expr)
        return std::make_shared<IntegerValueExprAST>((IntType)get_value<FloatValueExprAST>(LHS) ^ (IntType)(get_value<FloatValueExprAST>(RHS)));

    LHS->print_ast(); RHS->print_ast();
    eval_err("[_bit_xor] Invalid '^' expression.");
    return nullptr;
}

/* '~' */
std::shared_ptr<ExprAST> EvalImpl::_bit_not(const std::shared_ptr<ExprAST> RHS)
{
    if (RHS->SubType == Type::integer_expr)
        return std::make_shared<IntegerValueExprAST>(~get_value<IntegerValueExprAST>(RHS));

    if (RHS->SubType == Type::float_expr)
        return std::make_shared<IntegerValueExprAST>(~((IntType)get_value<FloatValueExprAST>(RHS)));

    RHS->print_ast();
    eval_err("[_bit_not] Invalid '~' expression.");
    return nullptr;
}
