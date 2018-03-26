#include "parser.h"
using namespace Parser;

// expression
//   ::= primary binoprhs
//
std::shared_ptr<ExprAST> ParserImpl::parser_experssion()
{
#ifdef LOG
    log("in parser_experssion");
#endif
    auto LHS = parser_primary();
    if (!LHS)
        return nullptr;
    return parser_binaryOpExpr(0, LHS);
}

// primary
//   ::= identifierexpr
//   ::= parenexpr
//   ::= numberexpr
//   ::= returnexpr
std::shared_ptr<ExprAST> ParserImpl::parser_primary()
{
#ifdef LOG
    log("in parser_primary");
#endif
    switch (CurToken.tk_type)
    {
        case Lexer::Type::tok_if:
            return parser_if();
        case Lexer::Type::tok_while:
            return parser_while();
        case Lexer::Type::tok_for:
            return parser_for();
        case Lexer::Type::tok_do_while:
            return parser_do_while();
        case Lexer::Type::tok_variable_declare:
            return parser_variable_define();
        case Lexer::Type::tok_identifier:
            return parser_identifier();
        case Lexer::Type::tok_integer: case Lexer::Type::tok_float: case Lexer::Type::tok_string:
            return parser_value();
        case Lexer::Type::tok_return:
            return parser_return();
        case Lexer::Type::tok_single_char:
            if (CurToken.tk_string == "(")
                return parser_parenExpr();
            if (CurToken.tk_string == "-" || CurToken.tk_string == "+" || CurToken.tk_string == "!" || CurToken.tk_string == "~")
                return parser_unaryOpExpr();
            parser_log_err("[parser_primary] Unknown token.");
        default:
            parser_log_err("[parser_primary] Unknown token.");
    }
    return nullptr;
}

// unaryexpr
//   ::= '+' expression
//   ::= '-' expression
//   ::= '!' expression
//   ::= '~' expression
std::shared_ptr<ExprAST> ParserImpl::parser_unaryOpExpr()
{
#ifdef LOG
    log("in parser_unaryOpExpr");
#endif
    auto Op = CurToken.tk_string;
    get_next_token(); // eat Op
    auto E = parser_primary();
    return std::make_shared<UnaryOpExprAST>(Op, E);
}

// binoprhs
//   ::= ('+' primary)*
std::shared_ptr<ExprAST> ParserImpl::parser_binaryOpExpr(int expr_prec, std::shared_ptr<ExprAST> LHS)
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
            RHS = parser_binaryOpExpr(tok_prec + 1, RHS);
            if (!RHS)
                return nullptr;
        }
        LHS = std::make_shared<BinaryOpExprAST>(bin_op, LHS, RHS);
    }
}

// value_expr
//  ::= double
//  ::= long long
//  ::= string
std::shared_ptr<ExprAST> ParserImpl::parser_value()
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
        return std::make_shared<IntegerValueExprAST>(v1);

    if (is_v2)
        return std::make_shared<FloatValueExprAST>(v2);
    
    if (is_v3)
        return std::make_shared<StringValueExprAST>(v3);

    return nullptr;
}

// identifierexpr
//   ::= identifier
//   ::= identifier '(' expression* ')'
// return => VariableExprAST | CallExprAST
std::shared_ptr<ExprAST> ParserImpl::parser_identifier()
{
#ifdef LOG
    log("in parser_identifier");
#endif
    std::string IdName = CurToken.tk_string;
    get_next_token();
    if (CurToken.tk_string != "(")
        return std::make_shared<VariableExprAST>(IdName);

    get_next_token(); // eat '('
    std::vector<std::shared_ptr<ExprAST>> Args;
    if (CurToken.tk_string != ")")
    {
        while (true)
        {
            if (auto Arg = parser_experssion())
                Args.push_back(Arg);
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
    return std::make_shared<CallExprAST>(IdName, Args);
}

// variablexpr 
//   ::= 'var' identifier ';'
//   ::= 'var' identifier binoprhs
//   ::= 'let' identifier ';'
//   ::= 'let' identifier binoprhs
std::shared_ptr<ExprAST> ParserImpl::parser_variable_define()
{
#ifdef LOG
    log("in parser_variable_define");
#endif
    auto DefineType = CurToken.tk_string;
    get_next_token(); // eat declare symbol
    auto LHS = parser_identifier();
    if (!LHS || LHS->SubType != AST::Type::variable_expr)
        parser_log_err("[parser_variable_define] Expected a variable_expr.");

    std::static_pointer_cast<VariableExprAST>(LHS)->DefineType = DefineType;
    return parser_binaryOpExpr(0, LHS);
}

// parenexpr ::= '(' expression ')'
std::shared_ptr<ExprAST> ParserImpl::parser_parenExpr()
{
#ifdef LOG
    log("in parser_parenExpr");
#endif
    if (CurToken.tk_string != "(")
        parser_log_err("[parser_parenExpr] Expected an '('!");
    get_next_token(); // eat '('
    
    auto V = parser_experssion();
    if (!V)
        return nullptr;

    if (CurToken.tk_string != ")")
        parser_log_err("[parser_parenExpr] Expected an ')'!");

    get_next_token(); // eat ')'
    return V;
}

// PrototypeExpr ::= 'function' identifier? '(' expression ')'
std::shared_ptr<PrototypeAST> ParserImpl::parser_prototype()
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
    return std::make_shared<PrototypeAST>(FnName, Args);
}

// functionexpr ::= 'function' identifier parenexpr
std::shared_ptr<FunctionAST> ParserImpl::parser_function()
{
#ifdef LOG
    log("in parser_function");
#endif
    get_next_token(); // eat 'funtion'
    auto Proto = parser_prototype();
    if (!Proto)
        return nullptr;

    auto Body = parser_block("[parser_function]");
    return std::make_shared<FunctionAST>(Proto, Body);
}

// returnexpr ::= 'return' expression?
std::shared_ptr<ExprAST> ParserImpl::parser_return()
{
#ifdef LOG
    log("in parser_return");
#endif
    get_next_token(); // eat 'return'
    if (CurToken.tk_string == ";") // just return
        return std::make_shared<ReturnExprAST>();

    auto E = parser_experssion();
    return std::make_shared<ReturnExprAST>(E);
}

// ifexpr
//   ::= 'if' '(' expression ')' ';'
//   ::= 'if' '(' expression ')' expression ';'
//   ::= 'if' '(' expression ')' '{' statement '}'
//   ::= 'if' '(' expression ')' '{' statement '}' 'else' expression ';'
//   ::= 'if' '(' expression ')' '{' statement '}' 'else' statement
//   ::= 'if' '(' expression ')' '{' statement '}' 'else' ifexpr
std::shared_ptr<ExprAST> ParserImpl::parser_if()
{
#ifdef LOG
    log("in parser_if");
#endif
    get_next_token(); // eat "if"
    auto Cond = parser_parenExpr();

    // if (cond) ;
    if (CurToken.tk_string == ";")
    {
        get_next_token(); // eat ';'
        return std::make_shared<IfExprAST>(Cond);
    }

    std::vector<std::shared_ptr<ExprAST>> IfStatement;
    if (CurToken.tk_string != "{")
        IfStatement.push_back(parser_one()); // if (cond) expression; ...
    else
        IfStatement = parser_block("[parser_if]"); // if (cond) { statement } ...

    // log(CurToken.tk_string);
    // ... else ...
    if (CurToken.tk_string != "else")
        return std::make_shared<IfExprAST>(Cond, IfStatement);
    get_next_token(); // eat "else"
    
    decltype(IfStatement) Else_Statement;
    // ... else if ...
    if (CurToken.tk_string == "if")
    {
        auto ElseIf = parser_if();
        return std::make_shared<IfExprAST>(Cond, IfStatement, ElseIf);            
    }

    if (CurToken.tk_string != "{")
        Else_Statement.push_back(parser_one()); // ... else expression;
    else
        Else_Statement = parser_block("[parser_if]"); // ... else { statement }

    return std::make_shared<IfExprAST>(Cond, IfStatement, Else_Statement);
}

std::shared_ptr<ExprAST> ParserImpl::parser_while()
{
#ifdef LOG
    log("in parser_while");
#endif
    return nullptr;
}

std::shared_ptr<ExprAST> ParserImpl::parser_do_while()
{
#ifdef LOG
    log("in parser_do_while");
#endif
    return nullptr;
}

std::shared_ptr<ExprAST> ParserImpl::parser_for()
{
#ifdef LOG
    log("in parser_for");
#endif
    return nullptr;
}

std::vector<std::shared_ptr<ExprAST>> ParserImpl::parser_block(const std::string& err_func_name)
{
#ifdef LOG
    log("in parser_block");
#endif
    if (CurToken.tk_string != "{")
        parser_log_err("[parser_block] Not a block.");
    get_next_token(); // eat "{"
    std::vector<std::shared_ptr<ExprAST>> Statement;
    if (CurToken.tk_string != "}")
    {
        while (true)
        {
            if (auto E = parser_one())
                Statement.push_back(E);

            if (CurToken.tk_string == "}")
                break;
        }
    }
    get_next_token(); // eat "}"
    return Statement;
}
