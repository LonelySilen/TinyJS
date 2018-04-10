#include "eval.h"
using namespace Eval;

inline std::shared_ptr<ExprAST> EvalImpl::eval_block(std::vector<std::shared_ptr<ExprAST>>& Statement)
{
#ifdef elog
    log("in eval_block");
#endif
    if (Statement.empty())
        return std::make_shared<IntegerValueExprAST>(0);

    std::shared_ptr<ExprAST> ret;
    for (auto i : Statement)
    {
        EvalLineNumber = i->LineNumber;
        ret = eval_one(i);
        if (is_interrupt_control_flow(ret))
            break;
    }
    return ret;
}

std::shared_ptr<ExprAST> EvalImpl::eval_return(std::shared_ptr<ReturnExprAST> R)
{
#ifdef elog
    log("in eval_return");
#endif
    if (R->RetValue) // Check it is just return , or not.
    {
        auto ans = eval_one(R->RetValue);
        return ans;
    }
    return R;
}

inline std::shared_ptr<ExprAST> EvalImpl::eval_function_expr(std::shared_ptr<FunctionAST> F)
{
    // Register function in current scope
    CurScope->set(F->Proto->Name, F);
    return F;
}

std::shared_ptr<ExprAST> EvalImpl::eval_if_else(std::shared_ptr<IfExprAST> If)
{
#ifdef elog
    log("in eval_if_else");
#endif
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
                eval_err("[eval_if_else] Uncaught SyntaxError: Unexpected name " + Cond->get_ast_name() + ".");
        }
    }

    // Execute if-statement
    if (cond_bool)
    {
        auto R = eval_block(If->IfBlock->Statement);
        recover_prev_env();
        return R;
    }

    // Execute else-if
    if (If->ElseIf)
    {
        auto R = eval_if_else(ptr_to<IfExprAST>(If->ElseIf));
        recover_prev_env();
        return R;
    }

    // Execute else-statement
    if (If->ElseBlock && !If->ElseBlock->Statement.empty())
    {
        auto R = eval_block(If->ElseBlock->Statement);
        recover_prev_env();
        return R;
    }

    recover_prev_env();
    return If;
}

std::shared_ptr<ExprAST> EvalImpl::eval_for(std::shared_ptr<ForExprAST> For)
{
#ifdef elog
    log("in eval_for");
#endif
    enter_new_env();

    auto InitCond = eval_expression(For->Cond[0]);
    if (!For->Block->Statement.empty())
    {
        while (value_to_bool(eval_expression(For->Cond[1])))
        {
            if (For->Block)
            {
                auto R = eval_block(For->Block->Statement);
                if (is_interrupt_control_flow(R))
                {
                    if (isBreak(R))
                        break;
                    else if (isContinue(R))
                        continue;
                    else if (isReturn(R))
                    {
                        recover_prev_env();
                        return R;
                    }
                }
            }
            eval_expression(For->Cond[2]);
        }
    }

    recover_prev_env();
    return For;
}

std::shared_ptr<ExprAST> EvalImpl::eval_while(std::shared_ptr<WhileExprAST> While)
{
#ifdef elog
    log("in eval_while");
#endif
    enter_new_env();
    while (value_to_bool(eval_expression(While->Cond)))
    {
        if (While->Block)
        {
            auto R = eval_block(While->Block->Statement);
            if (is_interrupt_control_flow(R))
            {
                if (isBreak(R))
                    break;
                else if (isContinue(R))
                    continue;
                else if (isReturn(R))
                {
                    recover_prev_env();
                    return R;
                }
            }
        }
    }
    recover_prev_env();
    return While;
}

std::shared_ptr<ExprAST> EvalImpl::eval_do_while(std::shared_ptr<DoWhileExprAST> DoWhile)
{
#ifdef elog
    log("in eval_do_while");
#endif
    enter_new_env();
    do {
        if (DoWhile->Block)
        {
            auto R = eval_block(DoWhile->Block->Statement);
            if (is_interrupt_control_flow(R))
            {
                if (isBreak(R))
                    break;
                else if (isContinue(R))
                    continue;
                else if (isReturn(R))
                {
                    recover_prev_env();
                    return R;
                }
            }
        }
    } while (value_to_bool(eval_expression(DoWhile->Cond)));

    recover_prev_env();
    return DoWhile;
}

std::shared_ptr<ExprAST> EvalImpl::eval_call_expr(std::shared_ptr<CallExprAST> Caller)
{
#ifdef elog
    log("in eval_call_expr");
#endif
    // If is built in function
    if (is_built_in(Caller->Callee))
        return exec_built_in(Caller);

    // Find function prototype
    auto F = find_name(Caller->Callee);
    if (!F)
    {
        ERR_INFO = "[eval_call_expr] ReferenceError: '" + Caller->Callee + "' is not defined. ";
        eval_err(ERR_INFO);
    }

    // Get the function definition
    auto Func = ptr_to<FunctionAST>(F);

    // Creat new function environment, Switch to sub scope
    enter_new_env();

    // Set parameters
    if (Caller->Args.size() >= Func->Proto->Args.size())
    {
        for (int i = 0; i < Func->Proto->Args.size(); ++i)
        {
            auto tmp_V = eval_expression(Caller->Args[i]);
            if (isVariable(tmp_V)) tmp_V = get_variable_value(tmp_V);
            CurScope->set(get_name(Func->Proto->Args[i]), tmp_V);
        }
    }
    else
    {
        for (int i = 0; i < Caller->Args.size(); i++)
        {
            auto tmp_V = eval_expression(Caller->Args[i]);
            if (isVariable(tmp_V)) tmp_V = get_variable_value(tmp_V);
            CurScope->set(get_name(Func->Proto->Args[i]), tmp_V);
        }

        for (int i = Caller->Args.size(); i < Func->Proto->Args.size(); i++)
            eval_expression(Func->Proto->Args[i]);
    }

    // Execute function body
    auto ret = eval_block(Func->Body->Statement);
    switch (ret->SubType)
    {
        case Type::return_expr:
        {
            auto R = eval_return(ptr_to<ReturnExprAST>(ret));
            if (isVariable(R) || isFunction(R))
                R = find_name(get_name(R));
            recover_prev_env(); // Exit curr environment
            return R;
        }
        case Type::break_expr:
            eval_err("Uncaught SyntaxError: Illegal break statement");
        case Type::continue_expr:
            eval_err("Uncaught SyntaxError: Illegal continue statement");
        default:
            break;
    }

    recover_prev_env(); // Exit curr environment
    return ret;
}

std::shared_ptr<ExprAST> EvalImpl::eval_unary_op_expr(std::shared_ptr<UnaryOpExprAST> expr)
{
#ifdef elog
    log("in eval_unary_op_expr");
#endif
    auto Op = expr->Op;
    auto E = eval_one(expr->Expression);
    auto _v = E;
    if (isVariable(E))
    {
        _v = get_variable_value(E);
        if (!_v)
        {
            ERR_INFO = "[eval_unary_op_expr] ReferenceError '" + get_name(E) + "' is not defined.";
            eval_err(ERR_INFO);
        }
    }

    if (Op == "-") return _mul(_v, std::make_shared<IntegerValueExprAST>(-1));
    if (Op == "+") return _mul(_v, std::make_shared<IntegerValueExprAST>(1));
    if (Op == "~") return _bit_not(_v);
    if (Op == "!") return _not(_v);

    eval_err("Uncaught SyntaxError: Unexpected token "+ Op);
    return nullptr;
}

std::shared_ptr<ExprAST> EvalImpl::eval_binary_op_expr(std::shared_ptr<BinaryOpExprAST> expr)
{
#ifdef elog
    log("in eval_binary_op_expr");
#endif
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
#ifdef elog
    log("in eval_bin_op_expr_helper");
#endif
    /* RHS is a variable */
    auto rvalue = RHS;
    if (isVariable(RHS))
    {
        rvalue = get_variable_value(RHS);
        if (!rvalue)
        {
            ERR_INFO = "[eval_bin_op_expr_helper] ReferenceError: '" + get_name(RHS) + "' is not defined. ";
            eval_err(ERR_INFO);
        }
    }

    // Assign
    if (Op == "=")
    {
    #ifdef elog
        log("in _assign");
    #endif
        if (!isVariable(LHS))
            eval_err("[eval_bin_op_expr_helper] Expected a variable_expr before '=', rvalue is not a identifier. ");

        // Invoke assign(...) if need check type
        // assign(LHS, RHS);
        auto _var = ptr_to<VariableExprAST>(LHS);
        if (_var->DefineType == "var")
            get_top_scope()->set(_var->Name, rvalue);
        else if (_var->DefineType == "let")
            CurScope->set(_var->Name, rvalue);
        else /* auto set variable, local first. */
            set_name(_var->Name, rvalue);

        // print_value(rvalue); // debug
        return rvalue;
    }

    // LHS is a variable
    auto lvalue = LHS;
    if (isVariable(LHS))
    {
        lvalue = get_variable_value(LHS);
        if (!lvalue)
        {
            ERR_INFO = "[eval_bin_op_expr_helper] ReferenceError: '" + get_name(LHS) + "' is not defined. ";
            eval_err(ERR_INFO);
        }
    }
    // LHS, RHS is Integer or Float or String or Expression
    if (Op.length() == 1)
    {
        switch (Op[0])
        {
            case ',':
                eval_expression(lvalue);
                return eval_expression(rvalue);
            case '+':  return _add(lvalue, rvalue);
            case '-':  return _sub(lvalue, rvalue);
            case '*':  return _mul(lvalue, rvalue);
            case '/':  return _div(lvalue, rvalue);
            case '>':  return _greater(lvalue, rvalue);
            case '<':  return _less(lvalue, rvalue);
            case '%':  return _mod(lvalue, rvalue);
            case '&':  return _bit_and(lvalue, rvalue);
            case '|':  return _bit_or(lvalue, rvalue);
            case '^':  return _bit_xor(lvalue, rvalue);
            default: break;
        }
    }
    else if (Op == ">=") return _not_less(lvalue, rvalue);
    else if (Op == "<=") return _not_more(lvalue, rvalue);
    else if (Op == "==") return _equal(lvalue, rvalue);
    else if (Op == "&&") return _and(lvalue, rvalue);
    else if (Op == "||") return _or(lvalue, rvalue);
    else if (Op == ">>") return _bit_rshift(lvalue, rvalue);
    else if (Op == "<<") return _bit_lshift(lvalue, rvalue);


    
    ERR_INFO = "[eval_bin_op_expr_helper] '" + Op + "' is invalid operator.";
    eval_err(ERR_INFO);
    return nullptr;
}


std::shared_ptr<ExprAST> EvalImpl::_add(const std::shared_ptr<ExprAST> LHS, const std::shared_ptr<ExprAST> RHS)
{
#ifdef elog
    log("in _add");
#endif
    /* Number */
    // 1+1=2
    if (isInt(LHS) && isInt(RHS))
        return std::make_shared<IntegerValueExprAST>(get_value<IntegerValueExprAST>(LHS) + get_value<IntegerValueExprAST>(RHS));
    // 1+1.0=2.0
    if (isInt(LHS) && isFloat(RHS))
        return std::make_shared<FloatValueExprAST>(get_value<IntegerValueExprAST>(LHS) + get_value<FloatValueExprAST>(RHS));
    // 1.0+1=2.0
    if (isFloat(LHS) && isInt(RHS)) 
        return std::make_shared<FloatValueExprAST>(get_value<FloatValueExprAST>(LHS) + get_value<IntegerValueExprAST>(RHS));
    // 1.0+1.0=2.0
    if (isFloat(LHS) && isFloat(RHS))
        return std::make_shared<FloatValueExprAST>(get_value<FloatValueExprAST>(LHS) + get_value<FloatValueExprAST>(RHS));
    /* String */
    // "1"+"1"="11"
    if (isString(LHS) && isString(RHS))
        return std::make_shared<StringValueExprAST>(get_value<StringValueExprAST>(LHS) + get_value<StringValueExprAST>(RHS));
    // 1+"1"="11"
    if (isInt(LHS) && isString(RHS))
        return std::make_shared<StringValueExprAST>(std::to_string(get_value<IntegerValueExprAST>(LHS)) + get_value<StringValueExprAST>(RHS));
    // "1"+1="11"
    if (isString(LHS) && isInt(RHS))
        return std::make_shared<StringValueExprAST>(get_value<StringValueExprAST>(LHS) + std::to_string(get_value<IntegerValueExprAST>(RHS)));
    // 1.0+"1"="1.01"
    if (isFloat(LHS) && isString(RHS))
        return std::make_shared<StringValueExprAST>(std::to_string(get_value<FloatValueExprAST>(LHS)) + get_value<StringValueExprAST>(RHS));
    // "1"+1.1="11.1"
    if (isString(LHS) && isFloat(RHS))
        return std::make_shared<StringValueExprAST>(get_value<StringValueExprAST>(LHS) + std::to_string(get_value<FloatValueExprAST>(RHS)));

    LHS->print_ast(); RHS->print_ast();
    eval_err("[_add] Invalid '+' expression.");
    return nullptr;
}

std::shared_ptr<ExprAST> EvalImpl::_sub(const std::shared_ptr<ExprAST> LHS, const std::shared_ptr<ExprAST> RHS)
{
    // 1-1=0
    if (isInt(LHS) && isInt(RHS))
        return std::make_shared<IntegerValueExprAST>(get_value<IntegerValueExprAST>(LHS) - get_value<IntegerValueExprAST>(RHS));
    // 1-1.0=0.0
    if (isInt(LHS) && isFloat(RHS))
        return std::make_shared<FloatValueExprAST>(get_value<IntegerValueExprAST>(LHS) - get_value<FloatValueExprAST>(RHS));
    // 1.0-1=0.0
    if (isFloat(LHS) && isInt(RHS)) 
        return std::make_shared<FloatValueExprAST>(get_value<FloatValueExprAST>(LHS) - get_value<IntegerValueExprAST>(RHS));
    // 1.0-1.0=0.0
    if (isFloat(LHS) && isFloat(RHS))
        return std::make_shared<FloatValueExprAST>(get_value<FloatValueExprAST>(LHS) - get_value<FloatValueExprAST>(RHS));

    LHS->print_ast(); RHS->print_ast();
    eval_err("[_sub] Invalid '-' expression.");
    return nullptr;
}

std::shared_ptr<ExprAST> EvalImpl::_mul(const std::shared_ptr<ExprAST> LHS, const std::shared_ptr<ExprAST> RHS)
{
    // 1*1=1
    if (isInt(LHS) && isInt(RHS))
        return std::make_shared<IntegerValueExprAST>(get_value<IntegerValueExprAST>(LHS) * get_value<IntegerValueExprAST>(RHS));
    // 1*1.0=1.0
    if (isInt(LHS) && isFloat(RHS))
        return std::make_shared<FloatValueExprAST>(get_value<IntegerValueExprAST>(LHS) * get_value<FloatValueExprAST>(RHS));
    // 1.0*1=1.0
    if (isFloat(LHS) && isInt(RHS)) 
        return std::make_shared<FloatValueExprAST>(get_value<FloatValueExprAST>(LHS) * get_value<IntegerValueExprAST>(RHS));
    // 1.0*1.0=1.0
    if (isFloat(LHS) && isFloat(RHS))
        return std::make_shared<FloatValueExprAST>(get_value<FloatValueExprAST>(LHS) * get_value<FloatValueExprAST>(RHS));

    LHS->print_ast(); RHS->print_ast();
    eval_err("[_mul] Invalid '*' expression.");
    return nullptr;
}

std::shared_ptr<ExprAST> EvalImpl::_div(const std::shared_ptr<ExprAST> LHS, const std::shared_ptr<ExprAST> RHS)
{
    // 1/1=1
    if (isInt(LHS) && isInt(RHS))
        return std::make_shared<IntegerValueExprAST>(get_value<IntegerValueExprAST>(LHS) / get_value<IntegerValueExprAST>(RHS));
    // 1/1.0=1.0
    if (isInt(LHS) && isFloat(RHS))
        return std::make_shared<FloatValueExprAST>(get_value<IntegerValueExprAST>(LHS) / get_value<FloatValueExprAST>(RHS));
    // 1.0/1=1.0
    if (isFloat(LHS) && isInt(RHS)) 
        return std::make_shared<FloatValueExprAST>(get_value<FloatValueExprAST>(LHS) / get_value<IntegerValueExprAST>(RHS));
    // 1.0/1.0=1.0
    if (isFloat(LHS) && isFloat(RHS))
        return std::make_shared<FloatValueExprAST>(get_value<FloatValueExprAST>(LHS) / get_value<FloatValueExprAST>(RHS));

    LHS->print_ast(); RHS->print_ast();
    eval_err("[_div] Invalid '/' expression.");
    return nullptr;
}

std::shared_ptr<ExprAST> EvalImpl::_mod(const std::shared_ptr<ExprAST> LHS, const std::shared_ptr<ExprAST> RHS)
{
    // 1%1=0
    if (isInt(LHS) && isInt(RHS))
        return std::make_shared<IntegerValueExprAST>(get_value<IntegerValueExprAST>(LHS) % get_value<IntegerValueExprAST>(RHS));
    // 1%1.0=0.0
    if (isInt(LHS) && isFloat(RHS))
        return std::make_shared<FloatValueExprAST>(fmod(get_value<IntegerValueExprAST>(LHS), get_value<FloatValueExprAST>(RHS)));
    // 1.0%1=0.0
    if (isFloat(LHS) && isInt(RHS)) 
        return std::make_shared<FloatValueExprAST>(fmod(get_value<FloatValueExprAST>(LHS), get_value<IntegerValueExprAST>(RHS)));
    // 1.0%1.0=0.0
    if (isFloat(LHS) && isFloat(RHS))
        return std::make_shared<FloatValueExprAST>(fmod(get_value<FloatValueExprAST>(LHS), get_value<FloatValueExprAST>(RHS)));

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
    if (isInt(LHS) && isInt(RHS))
        return std::make_shared<IntegerValueExprAST>(get_value<IntegerValueExprAST>(LHS) > get_value<IntegerValueExprAST>(RHS) ? 1 : 0);

    if (isFloat(LHS) && isInt(RHS))
        return std::make_shared<IntegerValueExprAST>(get_value<FloatValueExprAST>(LHS) > get_value<IntegerValueExprAST>(RHS) ? 1 : 0);

    if (isInt(LHS) && isFloat(RHS))
        return std::make_shared<IntegerValueExprAST>(get_value<IntegerValueExprAST>(LHS) > get_value<FloatValueExprAST>(RHS) ? 1 : 0);

    if (isFloat(LHS) && isFloat(RHS))
        return std::make_shared<IntegerValueExprAST>(get_value<FloatValueExprAST>(LHS) > get_value<FloatValueExprAST>(RHS) ? 1 : 0);

    LHS->print_ast(); RHS->print_ast();
    eval_err("[_greater] Invalid '>' expression.");
    return nullptr;
}

std::shared_ptr<ExprAST> EvalImpl::_less(const std::shared_ptr<ExprAST> LHS, const std::shared_ptr<ExprAST> RHS)
{
#ifdef elog
    log("in _less");
#endif
    if (isInt(LHS) && isInt(RHS))
        return std::make_shared<IntegerValueExprAST>(get_value<IntegerValueExprAST>(LHS) < get_value<IntegerValueExprAST>(RHS) ? 1 : 0);

    if (isFloat(LHS) && isInt(RHS))
        return std::make_shared<IntegerValueExprAST>(get_value<FloatValueExprAST>(LHS) < get_value<IntegerValueExprAST>(RHS) ? 1 : 0);

    if (isInt(LHS) && isFloat(RHS))
        return std::make_shared<IntegerValueExprAST>(get_value<IntegerValueExprAST>(LHS) < get_value<FloatValueExprAST>(RHS) ? 1 : 0);

    if (isFloat(LHS) && isFloat(RHS))
        return std::make_shared<IntegerValueExprAST>(get_value<FloatValueExprAST>(LHS) < get_value<FloatValueExprAST>(RHS) ? 1 : 0);

    LHS->print_ast(); RHS->print_ast();
    eval_err("[_less] Invalid '<' expression.");
    return nullptr;
}

std::shared_ptr<ExprAST> EvalImpl::_not_more(const std::shared_ptr<ExprAST> LHS, const std::shared_ptr<ExprAST> RHS)
{
    if (isInt(LHS) && isInt(RHS))
        return std::make_shared<IntegerValueExprAST>(get_value<IntegerValueExprAST>(LHS) <= get_value<IntegerValueExprAST>(RHS) ? 1 : 0);

    if (isFloat(LHS) && isInt(RHS))
        return std::make_shared<IntegerValueExprAST>(get_value<FloatValueExprAST>(LHS) <= get_value<IntegerValueExprAST>(RHS) ? 1 : 0);

    if (isInt(LHS) && isFloat(RHS))
        return std::make_shared<IntegerValueExprAST>(get_value<IntegerValueExprAST>(LHS) <= get_value<FloatValueExprAST>(RHS) ? 1 : 0);

    if (isFloat(LHS) && isFloat(RHS))
        return std::make_shared<IntegerValueExprAST>(get_value<FloatValueExprAST>(LHS) <= get_value<FloatValueExprAST>(RHS) ? 1 : 0);

    LHS->print_ast(); RHS->print_ast();
    eval_err("[_not_more] Invalid '<=' expression.");
    return nullptr;
}

std::shared_ptr<ExprAST> EvalImpl::_not_less(const std::shared_ptr<ExprAST> LHS, const std::shared_ptr<ExprAST> RHS)
{
    if (isInt(LHS) && isInt(RHS))
        return std::make_shared<IntegerValueExprAST>(get_value<IntegerValueExprAST>(LHS) >= get_value<IntegerValueExprAST>(RHS) ? 1 : 0);

    if (isFloat(LHS) && isInt(RHS))
        return std::make_shared<IntegerValueExprAST>(get_value<FloatValueExprAST>(LHS) >= get_value<IntegerValueExprAST>(RHS) ? 1 : 0);

    if (isInt(LHS) && isFloat(RHS))
        return std::make_shared<IntegerValueExprAST>(get_value<IntegerValueExprAST>(LHS) >= get_value<FloatValueExprAST>(RHS) ? 1 : 0);

    if (isFloat(LHS) && isFloat(RHS))
        return std::make_shared<IntegerValueExprAST>(get_value<FloatValueExprAST>(LHS) >= get_value<FloatValueExprAST>(RHS) ? 1 : 0);

    LHS->print_ast(); RHS->print_ast();
    eval_err("[_not_less] Invalid '>=' expression.");
    return nullptr;
}

std::shared_ptr<ExprAST> EvalImpl::_equal(const std::shared_ptr<ExprAST> LHS, const std::shared_ptr<ExprAST> RHS)
{
    if (isInt(LHS) && isInt(RHS))
        return std::make_shared<IntegerValueExprAST>(get_value<IntegerValueExprAST>(LHS) == get_value<IntegerValueExprAST>(RHS) ? 1 : 0);

    if (isFloat(LHS) && isInt(RHS))
        return std::make_shared<IntegerValueExprAST>(get_value<FloatValueExprAST>(LHS) == get_value<IntegerValueExprAST>(RHS) ? 1 : 0);

    if (isInt(LHS) && isFloat(RHS))
        return std::make_shared<IntegerValueExprAST>(get_value<IntegerValueExprAST>(LHS) == get_value<FloatValueExprAST>(RHS) ? 1 : 0);

    if (isFloat(LHS) && isFloat(RHS))
        return std::make_shared<IntegerValueExprAST>(get_value<FloatValueExprAST>(LHS) == get_value<FloatValueExprAST>(RHS) ? 1 : 0);

    if (isString(LHS) && isString(RHS))
        return std::make_shared<IntegerValueExprAST>(get_value<StringValueExprAST>(LHS) == get_value<StringValueExprAST>(RHS) ? 1 : 0); 

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
    if (isInt(LHS) && isInt(RHS))
        return std::make_shared<IntegerValueExprAST>(get_value<IntegerValueExprAST>(LHS) >> get_value<IntegerValueExprAST>(RHS));

    if (isFloat(LHS) && isInt(RHS))
        return std::make_shared<IntegerValueExprAST>((IntType)(get_value<FloatValueExprAST>(LHS)) >> get_value<IntegerValueExprAST>(RHS));

    if (isInt(LHS) && isFloat(RHS))
        return std::make_shared<IntegerValueExprAST>(get_value<IntegerValueExprAST>(LHS) >> (IntType)(get_value<FloatValueExprAST>(RHS)));

    if (isFloat(LHS) && isFloat(RHS))
        return std::make_shared<IntegerValueExprAST>((IntType)get_value<FloatValueExprAST>(LHS) >> (IntType)(get_value<FloatValueExprAST>(RHS)));

    LHS->print_ast(); RHS->print_ast();
    eval_err("[_bit_rshift] Invalid '>>' expression.");
    return nullptr;
}

std::shared_ptr<ExprAST> EvalImpl::_bit_lshift(const std::shared_ptr<ExprAST> LHS, const std::shared_ptr<ExprAST> RHS)
{
    if (isInt(LHS) && isInt(RHS))
        return std::make_shared<IntegerValueExprAST>(get_value<IntegerValueExprAST>(LHS) << get_value<IntegerValueExprAST>(RHS));

    if (isFloat(LHS) && isInt(RHS))
        return std::make_shared<IntegerValueExprAST>((IntType)(get_value<FloatValueExprAST>(LHS)) << get_value<IntegerValueExprAST>(RHS));

    if (isInt(LHS) && isFloat(RHS))
        return std::make_shared<IntegerValueExprAST>(get_value<IntegerValueExprAST>(LHS) << (IntType)(get_value<FloatValueExprAST>(RHS)));

    if (isFloat(LHS) && isFloat(RHS))
        return std::make_shared<IntegerValueExprAST>((IntType)get_value<FloatValueExprAST>(LHS) << (IntType)(get_value<FloatValueExprAST>(RHS)));

    LHS->print_ast(); RHS->print_ast();
    eval_err("[_bit_lshift] Invalid '<<' expression.");
    return nullptr;
}

std::shared_ptr<ExprAST> EvalImpl::_bit_and(const std::shared_ptr<ExprAST> LHS, const std::shared_ptr<ExprAST> RHS)
{
    if (isInt(LHS) && isInt(RHS))
        return std::make_shared<IntegerValueExprAST>(get_value<IntegerValueExprAST>(LHS) & get_value<IntegerValueExprAST>(RHS));

    if (isFloat(LHS) && isInt(RHS))
        return std::make_shared<IntegerValueExprAST>((IntType)(get_value<FloatValueExprAST>(LHS)) & get_value<IntegerValueExprAST>(RHS));

    if (isInt(LHS) && isFloat(RHS))
        return std::make_shared<IntegerValueExprAST>(get_value<IntegerValueExprAST>(LHS) & (IntType)(get_value<FloatValueExprAST>(RHS)));

    if (isFloat(LHS) && isFloat(RHS))
        return std::make_shared<IntegerValueExprAST>((IntType)get_value<FloatValueExprAST>(LHS) & (IntType)(get_value<FloatValueExprAST>(RHS)));

    LHS->print_ast(); RHS->print_ast();
    eval_err("[_bit_and] Invalid '&' expression.");
    return nullptr;
}

std::shared_ptr<ExprAST> EvalImpl::_bit_or(const std::shared_ptr<ExprAST> LHS, const std::shared_ptr<ExprAST> RHS)
{
    if (isInt(LHS) && isInt(RHS))
        return std::make_shared<IntegerValueExprAST>(get_value<IntegerValueExprAST>(LHS) | get_value<IntegerValueExprAST>(RHS));

    if (isFloat(LHS) && isInt(RHS))
        return std::make_shared<IntegerValueExprAST>((IntType)(get_value<FloatValueExprAST>(LHS)) | get_value<IntegerValueExprAST>(RHS));

    if (isInt(LHS) && isFloat(RHS))
        return std::make_shared<IntegerValueExprAST>(get_value<IntegerValueExprAST>(LHS) | (IntType)(get_value<FloatValueExprAST>(RHS)));

    if (isFloat(LHS) && isFloat(RHS))
        return std::make_shared<IntegerValueExprAST>((IntType)get_value<FloatValueExprAST>(LHS) | (IntType)(get_value<FloatValueExprAST>(RHS)));

    LHS->print_ast(); RHS->print_ast();
    eval_err("[_bit_or] Invalid '|' expression.");
    return nullptr;
}

std::shared_ptr<ExprAST> EvalImpl::_bit_xor(const std::shared_ptr<ExprAST> LHS, const std::shared_ptr<ExprAST> RHS)
{
    if (isInt(LHS) && isInt(RHS))
        return std::make_shared<IntegerValueExprAST>(get_value<IntegerValueExprAST>(LHS) ^ get_value<IntegerValueExprAST>(RHS));

    if (isFloat(LHS) && isInt(RHS))
        return std::make_shared<IntegerValueExprAST>((IntType)(get_value<FloatValueExprAST>(LHS)) ^ get_value<IntegerValueExprAST>(RHS));

    if (isInt(LHS) && isFloat(RHS))
        return std::make_shared<IntegerValueExprAST>(get_value<IntegerValueExprAST>(LHS) ^ (IntType)(get_value<FloatValueExprAST>(RHS)));

    if (isFloat(LHS) && isFloat(RHS))
        return std::make_shared<IntegerValueExprAST>((IntType)get_value<FloatValueExprAST>(LHS) ^ (IntType)(get_value<FloatValueExprAST>(RHS)));

    LHS->print_ast(); RHS->print_ast();
    eval_err("[_bit_xor] Invalid '^' expression.");
    return nullptr;
}

/* '~' */
std::shared_ptr<ExprAST> EvalImpl::_bit_not(const std::shared_ptr<ExprAST> RHS)
{
    if (isInt(RHS))
        return std::make_shared<IntegerValueExprAST>(~get_value<IntegerValueExprAST>(RHS));

    if (isFloat(RHS))
        return std::make_shared<IntegerValueExprAST>(~((IntType)get_value<FloatValueExprAST>(RHS)));

    RHS->print_ast();
    eval_err("[_bit_not] Invalid '~' expression.");
    return nullptr;
}
