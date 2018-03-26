#ifndef TINYJS_PARSER
#define TINYJS_PARSER

#include "lex.h"
#include "ast.h"
#include "log.h"
#include <unordered_map>
#include <memory>
#include <cstdlib>
#include <iostream>

// #define LOG

namespace Parser
{
    using namespace Lexer;
    using namespace AST;

    class ParserImpl : public Lexer::LexerImpl
    {
    private:
        std::vector<std::shared_ptr<ExprAST>> ParserResult;
        std::unordered_map<std::string, int> BinOpPrecedence;
        int get_tok_prec(const std::string& op) { return BinOpPrecedence.find(op) != BinOpPrecedence.end() ? BinOpPrecedence[op] : -1; }
    
    public:
        ParserImpl() : ParserImpl(nullptr) { }
        ParserImpl(std::streambuf* sptr) : LexerImpl(sptr) { parser_init(); }
        ~ParserImpl() = default;

        // Copy Constructor
        ParserImpl(const ParserImpl&) = delete;
        // Assign Constructor
        const ParserImpl& operator =(const ParserImpl&) = delete;
        // Move Constructor
        ParserImpl(ParserImpl&&) = delete;
        const ParserImpl& operator =(ParserImpl&&) = delete;

        std::shared_ptr<ExprAST> parser_experssion();
        std::shared_ptr<ExprAST> parser_unaryOpExpr();
        std::shared_ptr<ExprAST> parser_binaryOpExpr(int expr_prce, std::shared_ptr<ExprAST> LHS);
        std::shared_ptr<ExprAST> parser_primary();
        std::shared_ptr<ExprAST> parser_value();
        std::shared_ptr<ExprAST> parser_identifier();
        std::shared_ptr<ExprAST> parser_parenExpr();
        std::shared_ptr<FunctionAST> parser_function();
        std::shared_ptr<PrototypeAST> parser_prototype();
        std::shared_ptr<ExprAST> parser_return();
        std::shared_ptr<ExprAST> parser_variable_define();
        std::shared_ptr<ExprAST> parser_if();
        std::shared_ptr<ExprAST> parser_while();
        std::shared_ptr<ExprAST> parser_do_while();
        std::shared_ptr<ExprAST> parser_for();
        
        std::vector<std::shared_ptr<ExprAST>> parser_block(const std::string& err_info); /* { statement } */

        void parser_init()
        {
            if (BinOpPrecedence.empty())
            {
                BinOpPrecedence["&&"] = 40;
                BinOpPrecedence["||"] = 40;
                BinOpPrecedence["="] = 50;
                BinOpPrecedence[">"] = 60;
                BinOpPrecedence["<"] = 60;
                BinOpPrecedence[">="] = 60;
                BinOpPrecedence["<="] = 60;
                BinOpPrecedence["=="] = 60;
                BinOpPrecedence["!="] = 60;
                // BinOpPrecedence["!"] = 60;
                BinOpPrecedence[">>"] = 70;
                BinOpPrecedence["<<"] = 70;
                BinOpPrecedence["&"] = 80;
                BinOpPrecedence["|"] = 80;
                BinOpPrecedence["^"] = 80;
                // BinOpPrecedence["~"] = 80;
                BinOpPrecedence["+"] = 90;
                BinOpPrecedence["-"] = 90;
                BinOpPrecedence["*"] = 100;
                BinOpPrecedence["/"] = 100;
                BinOpPrecedence["%"] = 100;
            }
            ParserResult.clear();
        }

        void parser_reset()
        {
            parser_init();
            lexer_reset();
        }

        auto parser() -> decltype(ParserResult)
        {
            get_next_token();
            while (!cin.eof())
            {
                // cout << "ready> ";
                if (CurToken.tk_type == Lexer::Type::tok_eof) // End of file
                    break;
                ParserResult.push_back(parser_one());
            }
            return std::move(ParserResult);
        }

        std::shared_ptr<ExprAST> parser_one() 
        {
        #ifdef LOG
            log("\nin parser_one");
        #endif
            // print_token(CurToken);
            switch (CurToken.tk_type)
            { 
                /* These are not need ';' to end */
                case Lexer::Type::tok_function:
                    return parser_function();
                case Lexer::Type::tok_if:
                    return parser_if();
                case Lexer::Type::tok_eof:
                    break;
                default: /* need ';' for end */
                {
                    if (auto ret = parser_experssion()) 
                    {
                        if (CurToken.tk_string != ";")
                            parser_log_err("[parser_one] Expected a ';'.");
                        get_next_token(); // eat ";"
                        return ret;
                    }
                    break;
                }
            }
            return nullptr;
        }

        void parser_log_err(const std::string& loginfo)
        {
            std::cerr << loginfo;
            std::cerr << "\n[PARSER_LOG_ERROR] in line: " << LineNumber << ", ";
            std::cerr << "in token: ";
            print_token(CurToken);
            std::cerr << std::endl;
            exit(1);
        }
    };
}

#endif