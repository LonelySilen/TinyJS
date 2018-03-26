#ifndef TINYJS_AST
#define TINYJS_AST

#include <string>
#include <vector>
#include <memory>
#include <map>
#include <iostream>

namespace AST
{
    enum class Type
    {
        /* value_expr */
        integer_expr,
        float_expr,
        string_expr,
        /* syntax */
        variable_expr,
        unary_op_expr,
        binary_op_expr,
        call_expr,
        prototype_expr,
        function_expr,
        return_expr,
        if_else_expr,
        for_expr,
        while_expr,
        do_while_expr,
    };

    static std::map<Type, std::string> ASTName {
        { Type::integer_expr   , "integer_expr"   },
        { Type::float_expr     , "float_expr"     },
        { Type::string_expr    , "string_expr"    },
        { Type::variable_expr  , "variable_expr"  },
        { Type::unary_op_expr  , "unary_op_expr"  },
        { Type::binary_op_expr , "binary_op_expr" },
        { Type::call_expr      , "call_expr"      },
        { Type::prototype_expr , "prototype_expr" },
        { Type::function_expr  , "function_expr"  },
        { Type::return_expr    , "return_expr"    },
        { Type::if_else_expr   , "if_else_expr"   },
        { Type::for_expr       , "for_expr"       },
        { Type::while_expr     , "while_expr"     },
        { Type::do_while_expr  , "do_while_expr"  },
    };

    class ExprAST
    {
        public:
            Type SubType;

            ExprAST() = delete;
            ExprAST(Type SubType) : SubType(SubType) { }
            virtual ~ExprAST() = default;

            void print_ast() 
            {
                std::cout << "ASTName {" << std::endl;
                std::cout << "  " << ASTName[SubType] << std::endl;
                std::cout << "}" << std::endl;
            }
    };

    class IntegerValueExprAST : public ExprAST
    {
        public:
            typedef long long value_type;
            long long Val;
            IntegerValueExprAST(long long Val) : ExprAST(Type::integer_expr), Val(Val) {}
        
    };

    class FloatValueExprAST : public ExprAST
    {
        public:
            typedef double value_type;
            double Val;
            FloatValueExprAST(double Val) : ExprAST(Type::float_expr), Val(Val) { }
        
    };

    class StringValueExprAST : public ExprAST
    {
        public:
            typedef std::string value_type;
            std::string Val;
            StringValueExprAST(const std::string& Val) :  ExprAST(Type::string_expr), Val(Val) { }
        
    };

    class VariableExprAST : public ExprAST
    {
        public:
            std::string DefineType;
            std::string Name;
            VariableExprAST(const std::string& Name) : ExprAST(Type::variable_expr), DefineType(""), Name(Name) { }
            VariableExprAST(const std::string& DefineType, const std::string& Name) : ExprAST(Type::variable_expr), DefineType(DefineType), Name(Name) { }
            
    };


    class UnaryOpExprAST : public ExprAST
    {
    public:
        std::string Op;
        std::shared_ptr<ExprAST> Expression;
        UnaryOpExprAST(const std::string& Op, std::shared_ptr<ExprAST> Expression) : ExprAST(Type::unary_op_expr), Op(Op), Expression(Expression) { }
        
    };

    class BinaryOpExprAST : public ExprAST
    {
        public:
            std::string Op;
            std::shared_ptr<ExprAST> LHS, RHS;
            BinaryOpExprAST(const std::string& Op, std::shared_ptr<ExprAST> LHS, std::shared_ptr<ExprAST> RHS) : ExprAST(Type::binary_op_expr), Op(Op), LHS(LHS), RHS(RHS) { }

    };

    class CallExprAST : public ExprAST
    {
        public:
            std::string Callee;
            std::vector<std::shared_ptr<ExprAST>> Args;
            CallExprAST(const std::string& Callee, std::vector<std::shared_ptr<ExprAST>> Args) : ExprAST(Type::call_expr), Callee(Callee), Args(Args) { }

    };

    class PrototypeAST : public ExprAST
    {
        public:
            std::string Name;
            std::vector<std::string> Args;
            PrototypeAST(const std::string& Name, std::vector<std::string> Args) : ExprAST(Type::prototype_expr), Name(Name), Args(Args) { }
    };

    class FunctionAST : public ExprAST
    {
        public:
            std::shared_ptr<PrototypeAST> Proto;
            std::vector<std::shared_ptr<ExprAST>> Body;
            // declare
            FunctionAST(std::shared_ptr<PrototypeAST> Proto) : ExprAST(Type::function_expr), Proto(Proto) { }
            // define
            FunctionAST(std::shared_ptr<PrototypeAST> Proto, std::vector<std::shared_ptr<ExprAST>> Body) : ExprAST(Type::function_expr), Proto(Proto), Body(Body) { }

    };

    class ReturnExprAST : public ExprAST
    {
        public:
            std::shared_ptr<ExprAST> RetValue;
            ReturnExprAST() : ExprAST(Type::return_expr), RetValue(nullptr) { }
            ReturnExprAST(std::shared_ptr<ExprAST> RetValue) : ExprAST(Type::return_expr), RetValue(RetValue) { }
        
    };

    class IfExprAST : public ExprAST
    {
        public:
            std::shared_ptr<ExprAST> Cond;
            std::vector<std::shared_ptr<ExprAST>> If_Statement;
            std::vector<std::shared_ptr<ExprAST>> Else_Statement;
            std::shared_ptr<ExprAST> ElseIf;

            // if (cond) ;
            // param: (point)
            IfExprAST(std::shared_ptr<ExprAST> Cond) : ExprAST(Type::if_else_expr), Cond(Cond) { }

            // if (cond) { statement }
            // param: (point, vector)
            IfExprAST(std::shared_ptr<ExprAST> Cond, std::vector<std::shared_ptr<ExprAST>> If_Statement) : ExprAST(Type::if_else_expr), Cond(Cond), If_Statement(If_Statement) { }

            // if (cond) { statement } else { statement }
            // param: (point, vector, vector)
            IfExprAST(std::shared_ptr<ExprAST> Cond, std::vector<std::shared_ptr<ExprAST>> If_Statement, std::vector<std::shared_ptr<ExprAST>> Else_Statement) : ExprAST(Type::if_else_expr), Cond(Cond), If_Statement(If_Statement), Else_Statement(Else_Statement) { }

            // if (cond) { statement } else if (cond) { statement } ...
            // param: (point, vector, point)
            IfExprAST(std::shared_ptr<ExprAST> Cond, std::vector<std::shared_ptr<ExprAST>> If_Statement, std::shared_ptr<ExprAST> ElseIf) : ExprAST(Type::if_else_expr), Cond(Cond), If_Statement(If_Statement), ElseIf(ElseIf) { }
        
    };

    class ForExprAST : public ExprAST
    {
        public:
            std::shared_ptr<ExprAST> Cond;
            std::vector<std::shared_ptr<ExprAST>> Statement;
            // for (cond) { statement }
            ForExprAST(std::shared_ptr<ExprAST> Cond, std::vector<std::shared_ptr<ExprAST>> Statement) : ExprAST(Type::for_expr), Cond(Cond), Statement(Statement) { }
        
    };

    class WhileExprAST : public ExprAST
    {
        public:
            std::shared_ptr<ExprAST> Cond;
            std::vector<std::shared_ptr<ExprAST>> Statement;
            // while (cond) { statement }
            WhileExprAST(std::shared_ptr<ExprAST> Cond, std::vector<std::shared_ptr<ExprAST>> Statement) : ExprAST(Type::while_expr), Cond(Cond), Statement(Statement) { }
        
    };

    class DoWhileExprAST : public ExprAST
    {
    public:
        std::vector<std::shared_ptr<ExprAST>> Statement;
        std::shared_ptr<ExprAST> Cond;
        // do { statement } while (cond)
        DoWhileExprAST(std::vector<std::shared_ptr<ExprAST>> Statement, std::shared_ptr<ExprAST> Cond) : ExprAST(Type::do_while_expr), Statement(Statement), Cond(Cond) { }
        
    };

}

#endif