#include "lex.h"
#include "ast.h"
#include "parser.h"
#include "eval.h"
#include <string>
#include <cstdio>
#include <iostream>
#include <fstream>
using std::cin;
using std::cout;
// using namespace TinyJS;

void test_lexer()
{
    std::ifstream fin("TinyJS.h");
    Lexer::LexerImpl t(fin.rdbuf());
    while (!cin.eof())
    {
        cout << "ready> ";
        t.print_token(std::move(t.get_next_token()));
    }
}

void test_parser()
{
    std::ifstream fin("test");
    Parser::ParserImpl t(fin.rdbuf());
    // Parser::ParserImpl t;
    // t.parser();
    Eval::EvalImpl<decltype(t.parser())&&> e(t.parser());
}

int main()
{
    // test_lexer();
    test_parser();
    return 0;
}

