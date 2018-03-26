#include "eval.h"
using namespace Eval;

inline std::shared_ptr<ExprAST> EvalImpl::eval_block(std::vector<std::shared_ptr<ExprAST>>& Statement)
{
    std::shared_ptr<ExprAST> ret;
    for (auto i : Statement)
        ret = eval_one(i);
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
    auto Cond = eval_one(If->Cond);
    bool cond_bool = true;
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

    // Execute if-statement
    if (cond_bool)
        return eval_block(If->If_Statement);

    // Execute else-if
    if (If->ElseIf)
        return eval_if_else(std::static_pointer_cast<IfExprAST>(If->ElseIf));

    // Execute else-statement
    if (!If->Else_Statement.empty())
        return eval_block(If->Else_Statement);

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
    // Match the parameters list
    if (Caller->Args.size() != Func->Proto->Args.size())
    {
        ERR_INFO = "[eval_call_expr] Number od parameters mismatch. Expected " + std::to_string(Func->Proto->Args.size()) + " parameters.";
        eval_err(ERR_INFO);
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
    if (Op == "~") return _bit_not(_v);
    if (Op == "!") return _not(_v);
    return nullptr;
}

std::shared_ptr<ExprAST> EvalImpl::eval_binary_op_expr(std::shared_ptr<BinaryOpExprAST> expr)
{
    auto LHS = eval_one_bin_op_expr(expr->LHS);
    auto RHS = eval_one_bin_op_expr(expr->RHS);

    // null => 0
    if (!LHS) LHS = std::make_shared<IntegerValueExprAST>(0);
    if (!RHS) RHS = std::make_shared<IntegerValueExprAST>(0);
    return eval_bin_op_expr_helper(expr->Op, LHS, RHS);
}

// eval one of Binary Operator (LHS or RHS)
std::shared_ptr<ExprAST> EvalImpl::eval_one_bin_op_expr(std::shared_ptr<ExprAST> E)
{
    switch (E->SubType)
    {
        case Type::integer_expr: 
        case Type::float_expr: 
        case Type::string_expr:
        case Type::variable_expr:
            return E;
        case Type::unary_op_expr:
            return eval_unary_op_expr(std::static_pointer_cast<UnaryOpExprAST>(E));
        case Type::binary_op_expr:
            return eval_binary_op_expr(std::static_pointer_cast<BinaryOpExprAST>(E));
        case Type::call_expr:
            return eval_call_expr(std::static_pointer_cast<CallExprAST>(E));
        default: 
            break;
    }
    return nullptr;
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

    // LHS, RHS is Integer or Float or String
    if (Op == "+") return _add(LHS, RHS);
    if (Op == "-") return _sub(LHS, RHS);
    if (Op == "*") return _mul(LHS, RHS);
    if (Op == "/") return _div(LHS, RHS);
    if (Op == "%") return _mod(LHS, RHS);
    if (Op == "&&") return _and(LHS, RHS);
    if (Op == "||") return _or(LHS, RHS);
    if (Op == ">>") return _rshift(LHS, RHS);
    if (Op == "<<") return _lshift(LHS, RHS);
    if (Op == "&") return _bit_and(LHS, RHS);
    if (Op == "|") return _bit_or(LHS, RHS);
    if (Op == "^") return _bit_xor(LHS, RHS);
    
    eval_err("[eval_bin_op_expr_helper] Invalid parameters.");
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
    eval_err("[add] Invalid '+' expression.");
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
    eval_err("[sub] Invalid '-' expression.");
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
    eval_err("[mul] Invalid '*' expression.");
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
    eval_err("[div] Invalid '/' expression.");
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
    eval_err("[mod] Invalid '%' expression.");
    return nullptr;
}

/* '!' */
inline std::shared_ptr<ExprAST> EvalImpl::_not(const std::shared_ptr<ExprAST> RHS)
{
    return std::make_shared<IntegerValueExprAST>(!value_to_bool(RHS));
}

std::shared_ptr<ExprAST> EvalImpl::_and(const std::shared_ptr<ExprAST> LHS, const std::shared_ptr<ExprAST> RHS)
{
    return nullptr;
}

std::shared_ptr<ExprAST> EvalImpl::_or(const std::shared_ptr<ExprAST> LHS, const std::shared_ptr<ExprAST> RHS)
{
    return nullptr;
}

std::shared_ptr<ExprAST> EvalImpl::_rshift(const std::shared_ptr<ExprAST> LHS, const std::shared_ptr<ExprAST> RHS)
{
    return nullptr;
}

std::shared_ptr<ExprAST> EvalImpl::_lshift(const std::shared_ptr<ExprAST> LHS, const std::shared_ptr<ExprAST> RHS)
{
    return nullptr;
}

std::shared_ptr<ExprAST> EvalImpl::_bit_and(const std::shared_ptr<ExprAST> LHS, const std::shared_ptr<ExprAST> RHS)
{
    return nullptr;
}

std::shared_ptr<ExprAST> EvalImpl::_bit_or(const std::shared_ptr<ExprAST> LHS, const std::shared_ptr<ExprAST> RHS)
{
    return nullptr;
}

std::shared_ptr<ExprAST> EvalImpl::_bit_xor(const std::shared_ptr<ExprAST> LHS, const std::shared_ptr<ExprAST> RHS)
{
    return nullptr;
}

/* '~' */
std::shared_ptr<ExprAST> EvalImpl::_bit_not(const std::shared_ptr<ExprAST> RHS)
{
    return nullptr;
}
