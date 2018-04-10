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
//   ::= unaryexpr
std::shared_ptr<ExprAST> ParserImpl::parser_primary()
{
#ifdef LOG
    log("in parser_primary");
#endif
    switch (CurToken.tk_type)
    {
        case Lexer::Type::tok_identifier: return parser_identifier();
        case Lexer::Type::tok_integer: case Lexer::Type::tok_float: case Lexer::Type::tok_string:
            return parser_value();

        case Lexer::Type::tok_op:
        case Lexer::Type::tok_single_char:
        {
            switch (CurToken.tk_string[0])
            {
                case '-': case '+': case '!': case '~':
                    return parser_unaryOpExpr();
                case '(':
                    return parser_parenExpr();
                default:
                    break;
            }
        }
            
        default:
            parser_err("[parser_primary] Unknown token.");
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
            parser_err("[parser_value] Not ValueExprAST!");
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
std::shared_ptr<ExprAST> ParserImpl::parser_identifier(const std::string& DefineType)
{
#ifdef LOG
    log("in parser_identifier");
#endif
    std::string IdName = CurToken.tk_string;
    get_next_token(); // eat identifier
    auto Op = CurToken.tk_string;

    // Call
    if (Op[0] == '(')
    {
        auto Args = parser_parameter_list("(", ")", "parser_identifier", ",");
        return std::make_shared<CallExprAST>(IdName, Args);
    }
    return std::make_shared<VariableExprAST>(DefineType, IdName);
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
    if (CurToken.tk_string == "let" || CurToken.tk_string == "var")
    {
        auto DefineType = CurToken.tk_string;
        get_next_token(); // eat declare symbol
        auto LHS = parser_identifier(DefineType);
        if (CurToken.tk_string == ";")
            return LHS;

        auto BinOp = CurToken.tk_string;
        get_next_token(); // eat BinOp

        auto RHS = parser_experssion();
        return std::make_shared<BinaryOpExprAST>(BinOp, LHS, RHS);
    }
    return parser_experssion();
}

// objectexpr
//   ::= '{' key:value, key:value, ... '}'
std::shared_ptr<ExprAST> ParserImpl::parser_object()
{
    return nullptr;
}

// parenexpr ::= '(' expression ')'
std::shared_ptr<ExprAST> ParserImpl::parser_parenExpr()
{
#ifdef LOG
    log("in parser_parenExpr");
#endif
    if (CurToken.tk_string != "(")
        parser_err("[parser_parenExpr] Expected '('!");
    get_next_token(); // eat '('
    
    auto V = parser_experssion();
    if (!V)
        return nullptr;

    if (CurToken.tk_string != ")")
        parser_err("[parser_parenExpr] Expected ')'!");

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

    auto Args = parser_parameter_list("(", ")", "parser_prototype", ",");
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

    auto Body = parser_block("function");
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

    return std::make_shared<ReturnExprAST>(parser_experssion());
}

// breakexpr ::= 'break'
inline std::shared_ptr<ExprAST> ParserImpl::parser_break()
{
#ifdef LOG
    log("in parser_break");
#endif
    get_next_token();
    return std::make_shared<BreakExprAST>();
}

// continuexpr ::= 'continue'
inline std::shared_ptr<ExprAST> ParserImpl::parser_continue()
{
#ifdef LOG
    log("in parser_continue");
#endif
    get_next_token();
    return std::make_shared<ContinueExprAST>();
}


// ifexpr
//   ::= 'if' '(' expression ')' ';'
//   ::= 'if' '(' expression ')' expression ';'
//   ::= 'if' '(' expression ')' blockexpr
//   ::= 'if' '(' expression ')' blockexpr 'else' expression ';'
//   ::= 'if' '(' expression ')' blockexpr 'else' statement
//   ::= 'if' '(' expression ')' blockexpr 'else' ifexpr
std::shared_ptr<ExprAST> ParserImpl::parser_if()
{
#ifdef LOG
    log("in parser_if");
#endif
    get_next_token(); // eat "if"
    auto Cond = parser_parenExpr();
    if (!Cond)
        parser_err("[parser_if] Expected cond expression.");

    // if (cond) ;
    if (CurToken.tk_string == ";")
    {
        get_next_token(); // eat ';'
        return std::make_shared<IfExprAST>(Cond);
    }

    auto IfBlock = parser_block("if");

    // log(CurToken.tk_string);
    // ... else ...
    if (CurToken.tk_string != "else")
        return std::make_shared<IfExprAST>(Cond, IfBlock);
    get_next_token(); // eat "else"
    
    // ... else if ...
    if (CurToken.tk_string == "if")
    {
        auto ElseIf = parser_if();
        return std::make_shared<IfExprAST>(Cond, IfBlock, ElseIf);            
    }

    auto ElseBlock = parser_block("if");

    return std::make_shared<IfExprAST>(Cond, IfBlock, ElseBlock);
}

// whileexpr
//   ::= 'while' '(' expression ')' blockexpr
std::shared_ptr<ExprAST> ParserImpl::parser_while()
{
#ifdef LOG
    log("in parser_while");
#endif
    get_next_token(); // eat 'while'
    auto Cond = parser_parenExpr();
    if (CurToken.tk_string == ";")
    {
        get_next_token();
        return std::make_shared<WhileExprAST>(Cond);
    }
    auto Block = parser_block();
    return std::make_shared<WhileExprAST>(Cond, Block);
}

// dowhileexpr
//   ::= 'do' blockexpr 'while' '(' expression ')' ';'
std::shared_ptr<ExprAST> ParserImpl::parser_do_while()
{
#ifdef LOG
    log("in parser_do_while");
#endif
    get_next_token(); // eat 'do'
    auto Block = parser_block();
    get_next_token(); // eat 'while'
    auto Cond = parser_parenExpr();
    get_next_token(); // eat ';'
    return std::make_shared<DoWhileExprAST>(Block, Cond);
}

std::shared_ptr<ExprAST> ParserImpl::parser_for()
{
#ifdef LOG
    log("in parser_for");
#endif
    get_next_token(); // eat 'for'
    get_next_token(); // eat '('
    std::vector<std::shared_ptr<ExprAST>> Cond;
    Cond.push_back(parser_variable_define());
    get_next_token(); // eat ';'
    Cond.push_back(parser_experssion());
    get_next_token(); // eat ';'
    Cond.push_back(parser_experssion());
    get_next_token(); // eat ')'

    if (Cond.size() != 3)
        parser_err("[parser_for] 'for' need 3 params.");

    // for (cond) ;
    if (CurToken.tk_string == ";")
    {
        get_next_token(); // eat ';'
        return std::make_shared<ForExprAST>(Cond);
    }
    return std::make_shared<ForExprAST>(Cond, parser_block("for"));
}

// paramexpr
//  ::= '(' expression, ... ')'
std::vector<std::shared_ptr<ExprAST>> ParserImpl::parser_parameter_list(const std::string& _start, const std::string& _end, const std::string& err_func_name, const std::string& separater)
{
#ifdef LOG
    log("in parser_parameter_list");
#endif
    del_op(","); // remove ',' from operator
    if (CurToken.tk_string != _start)
        parser_err("[" + err_func_name + "] Expected '" + _start + "'.");
    get_next_token(); // eat _start

    std::vector<std::shared_ptr<ExprAST>> Params;
    if (CurToken.tk_string != _end)
    {
        while (true)
        {
            if (auto P = parser_experssion())
                Params.push_back(P);
            else
                parser_err("[" + err_func_name + "] Parser Arguments Err.");

            if (CurToken.tk_string == _end)
                break;

            if (CurToken.tk_string != separater)
                parser_err("[" + err_func_name + "] Unexpected '" + separater + "'!");
            get_next_token();
        }
    }
    get_next_token(); // eat _end
    set_op(",", 1);
    return Params;
}


// blockexpr
//  ::= '{' statement '}'
std::shared_ptr<BlockExprAST> ParserImpl::parser_block(const std::string& err_block_name)
{
#ifdef LOG
    log("in parser_block");
#endif
    // Just a line
    if (CurToken.tk_string != "{")
        return std::make_shared<BlockExprAST>(parser_one());
    get_next_token(); // eat "{"
    std::vector<std::shared_ptr<ExprAST>> Statement;
    if (CurToken.tk_string != "}")
    {
        while (true)
        {
            if (auto E = parser_one())
            {
                E->LineNumber = LineNumber; // set line number for debug
                Statement.push_back(E);
            }

            if (CurToken.tk_string == "}")
                break;
        }
    }
    get_next_token(); // eat "}"
    return std::make_shared<BlockExprAST>(Statement);
}
