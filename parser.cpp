#include "parser.h"
using namespace Parser;

// expression
//   ::= primary binoprhs
//
std::unique_ptr<ExprAST> ParserImpl::parser_experssion()
{
#ifdef LOG
    log("in parser_experssion");
#endif
    auto LHS = parser_primary();
    if (!LHS)
        return nullptr;
    return parser_binaryOpExpr(0, std::move(LHS));
}

// primary
//   ::= identifierexpr
//   ::= parenexpr
//   ::= numberexpr
std::unique_ptr<ExprAST> ParserImpl::parser_primary()
{
#ifdef LOG
    log("in parser_primary");
#endif
    switch (CurToken.tk_type)
    {
        case Lexer::Type::tok_identifier:
            return parser_identifier();
        case Lexer::Type::tok_integer: case Lexer::Type::tok_float: case Lexer::Type::tok_string:
            return parser_value();
        case Lexer::Type::tok_single_char:
            if (CurToken.tk_string == "(")
                return parser_parenExpr();
            parser_log_err("[parser_primary] Unknown token.");
        default:
            parser_log_err("[parser_primary] Unknown token.");
    }
    return nullptr;
}

// binoprhs
//   ::= ('+' primary)*
std::unique_ptr<ExprAST> ParserImpl::parser_binaryOpExpr(int expr_prec, std::unique_ptr<ExprAST> LHS)
{
#ifdef LOG
    log("in parser_binaryOpExpr");
#endif

    while (true)
    {
        int tok_prec = get_tok_prec(CurToken.tk_string);
        if (expr_prec > tok_prec)
            return LHS;

        auto bin_op = CurToken.tk_string;
        get_next_token(); // eat BinOp

        auto RHS = parser_primary();
        if (!RHS)
            return nullptr;

        int next_prec = get_tok_prec(CurToken.tk_string);
        if (tok_prec < next_prec)
        {
            RHS = parser_binaryOpExpr(tok_prec + 1, std::move(RHS));
            if (!RHS)
                return nullptr;
        }
        LHS = std::make_unique<BinaryOpExprAST>(bin_op, std::move(LHS), std::move(RHS));
    }
}

std::unique_ptr<ExprAST> ParserImpl::parser_value()
{
#ifdef LOG
    log("in parser_value");
#endif
    long long v1;
    double v2;
    std::string v3;
    bool is_v1, is_v2, is_v3;
    is_v1 = is_v2 = is_v3 = false;
    switch (CurToken.tk_type)
    {
        case Lexer::Type::tok_integer:
            v1 = atoi(CurToken.tk_string.c_str());
            is_v1 = true;
            break;
        case Lexer::Type::tok_float:
            v2 = atof(CurToken.tk_string.c_str());
            is_v2 = true;
            break;
        case Lexer::Type::tok_string:
            v3 = CurToken.tk_string;
            is_v3 = true;
            break;
        default:
            parser_log_err("[parser_value] Not a ValueExprAST!");
    }

    get_next_token(); // eat Number
    if (is_v1)
        return std::make_unique<ValueExprAST<decltype(v1)>>(std::move(v1));

    if (is_v2)
        return std::make_unique<ValueExprAST<decltype(v2)>>(std::move(v2));
    
    if (is_v3)
        return std::make_unique<ValueExprAST<decltype(v3)>>(std::move(v3));

    return nullptr;
}

// identifierexpr
//   ::= identifier
//   ::= identifier '(' expression* ')'
std::unique_ptr<ExprAST> ParserImpl::parser_identifier()
{
#ifdef LOG
    log("in parser_identifier");
#endif
    std::string IdName = CurToken.tk_string;
    get_next_token();
    if (CurToken.tk_string != "(")
        return std::make_unique<VariableExprAST>(IdName);

    get_next_token(); // eat '('
    std::vector<std::unique_ptr<ExprAST>> Args;
    if (CurToken.tk_string != ")")
    {
        while (true)
        {
            if (auto Arg = parser_experssion())
                Args.push_back(std::move(Arg));
            else
                parser_log_err("[parser_identifier] Parser Arg Err.");

            if (CurToken.tk_string == ")")
                break;

            if (CurToken.tk_string != ",")
                parser_log_err("[parser_identifier] Unexpected an ','!");
            get_next_token();
        }
    }
    get_next_token(); // eat ')'
    return std::make_unique<CallExprAST>(IdName, std::move(Args));
}

// parenexpr ::= '(' expression ')'
std::unique_ptr<ExprAST> ParserImpl::parser_parenExpr()
{
#ifdef LOG
    log("in parser_parenExpr");
#endif
    get_next_token(); // eat '('
    
    auto V = parser_experssion();
    if (!V)
        return nullptr;

    if (CurToken.tk_string != ")")
        parser_log_err("[parser_parenExpr] Expected an ')'!");

    get_next_token(); // eat ')'
    return V;
}

// PrototypeExpr ::= 'function' identifier '(' expression ')'
std::unique_ptr<PrototypeAST> ParserImpl::parser_prototype()
{
#ifdef LOG
    log("in parser_prototype");
#endif
    std::string FnName;
    if (CurToken.tk_type == Lexer::Type::tok_identifier)
    {
        FnName = CurToken.tk_string;
        get_next_token(); // eat function name
    }

    if (CurToken.tk_string != "(")
        parser_log_err("[parser_prototype] Expected an '('.");
    get_next_token(); // eat '('

    // param list
    std::vector<std::string> Args;
    if (CurToken.tk_string != ")")
    {
        while (true)
        {
            if (CurToken.tk_type == Lexer::Type::tok_identifier)
            {
                Args.push_back(CurToken.tk_string);
                get_next_token();
            }
            else
                parser_log_err("[parser_prototype] Unexpected an token.");

            if (CurToken.tk_string == ")")
                break;

            if (CurToken.tk_string != ",")
                parser_log_err("[parser_prototype] Expected an ','!");
            get_next_token();
        }
    }
    get_next_token(); // eat ')'
    return std::make_unique<PrototypeAST>(FnName, std::move(Args));
}

// functionexpr = 'function' identifier parenexpr
std::unique_ptr<FunctionAST> ParserImpl::parser_function()
{
#ifdef LOG
    log("in parser_function");
#endif
    get_next_token(); // eat 'funtion'
    auto Proto = parser_prototype();
    if (!Proto)
        return nullptr;

    if (CurToken.tk_string != "{")
        parser_log_err("[parser_function] Expected an '{'.");
    get_next_token(); // eat '{'

    std::vector<std::unique_ptr<ExprAST>> Body;
    if (CurToken.tk_string != "}")
    {
        while (true)
        {
            if (auto E = parser_line())
            {
                Body.push_back(std::move(E));
                if (CurToken.tk_string != ";")
                    parser_log_err("[parser_function] Expected an ';'.");
                get_next_token(); // eat ';'
            }

            if (CurToken.tk_string == "}")
                break;
        }
    }

    get_next_token(); // eat '}'
    return std::make_unique<FunctionAST>(std::move(Proto), std::move(Body));
}