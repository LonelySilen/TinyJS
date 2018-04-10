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
    
    private:
        /* param list */
        std::vector<std::shared_ptr<ExprAST>> parser_parameter_list(const std::string& _start, const std::string& _end, const std::string& err_func_name, const std::string& separater);

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
        std::shared_ptr<ExprAST> parser_object();
        std::shared_ptr<ExprAST> parser_identifier(const std::string& DefineType = "");
        std::shared_ptr<ExprAST> parser_parenExpr();
        std::shared_ptr<FunctionAST> parser_function();
        std::shared_ptr<PrototypeAST> parser_prototype();
        std::shared_ptr<ExprAST> parser_return();
        std::shared_ptr<ExprAST> parser_break();
        std::shared_ptr<ExprAST> parser_continue();
        std::shared_ptr<ExprAST> parser_variable_define();
        std::shared_ptr<ExprAST> parser_if();
        std::shared_ptr<ExprAST> parser_while();
        std::shared_ptr<ExprAST> parser_do_while();
        std::shared_ptr<ExprAST> parser_for();
        std::shared_ptr<BlockExprAST> parser_block(const std::string& err_block_name = "__anony");

        void set_op(const std::string& Op, int Level)
        { BinOpPrecedence[Op] = Level; }

        void del_op(const std::string& Op)
        { BinOpPrecedence.erase(Op); }

        void parser_init()
        {
            if (BinOpPrecedence.empty())
            {
                BinOpPrecedence["="] = 30;
                BinOpPrecedence["&&"] = 40;
                BinOpPrecedence["||"] = 40;
                BinOpPrecedence[">>"] = 40;
                BinOpPrecedence["<<"] = 40;
                BinOpPrecedence[">"] = 60;
                BinOpPrecedence["<"] = 60;
                BinOpPrecedence[">="] = 60;
                BinOpPrecedence["<="] = 60;
                BinOpPrecedence["=="] = 60;
                BinOpPrecedence["!="] = 60;
                BinOpPrecedence["&"] = 80;
                BinOpPrecedence["|"] = 80;
                BinOpPrecedence["^"] = 80;
                BinOpPrecedence["+"] = 90;
                BinOpPrecedence["-"] = 90;
                BinOpPrecedence["*"] = 100;
                BinOpPrecedence["/"] = 100;
                BinOpPrecedence["%"] = 100;
            }
            ParserResult.clear();
            set_op(",", 1); // for domma expression
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
                // cout << "ready> " << "in line: " << LineNumber << endl;
                if (CurToken.tk_type == Lexer::Type::tok_eof) // End of file
                    break;
                auto E = parser_one();
                E->LineNumber = LineNumber;
                ParserResult.push_back(E);
            }
            return std::move(ParserResult);
        }

        std::shared_ptr<ExprAST> parser_one() 
        {
        #ifdef LOG
            log("\nin parser_one");
        #endif
            // print_token(CurToken);
            std::shared_ptr<ExprAST> ret;
            switch (CurToken.tk_type)
            {
                case Lexer::Type::tok_function: ret = parser_function(); break;
                case Lexer::Type::tok_if:       ret = parser_if();       break;
                case Lexer::Type::tok_while:    ret = parser_while();    break;
                case Lexer::Type::tok_for:      ret = parser_for();      break;
                case Lexer::Type::tok_do_while: ret = parser_do_while(); break;
                case Lexer::Type::tok_variable_declare: ret = parser_variable_define(); break;
                case Lexer::Type::tok_return:   ret = parser_return();   break;
                case Lexer::Type::tok_break:    ret = parser_break();    break;
                case Lexer::Type::tok_continue: ret = parser_continue(); break;
                case Lexer::Type::tok_eof:      return nullptr;
                default:
                {
                    if (CurToken.tk_string[0] == '{')
                        ret = parser_block();
                    else
                        ret = parser_experssion();
                    break;
                }
            }
            while (CurToken.tk_string == ";")
                get_next_token(); // eat ";"
            return ret;
        }

        void parser_err(const std::string& loginfo)
        {
            std::cerr << loginfo;
            std::cerr << "\n[PARSER_ERROR] in line: " << LineNumber << ", ";
            std::cerr << "in token: ";
            print_token(CurToken);
            std::cerr << std::endl;
            exit(1);
        }
    };
}

#endif