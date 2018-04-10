#include "parser.h"
#include "eval.h"
#include <string>
#include <cstdio>
#include <iostream>
#include <fstream>
#include <ctime>
using std::cin;
using std::cout;
using std::endl;

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
    std::ifstream fin("test2");
    Parser::ParserImpl t(fin.rdbuf());
    Eval::EvalImpl e(t.parser());
    e.eval();
}

int main()
{
    clock_t _start, _end;
    _start = clock();
    test_parser();
    _end = clock();
    cout << "Time : " << double(_end - _start) / CLOCKS_PER_SEC << endl;
    return 0;
}

