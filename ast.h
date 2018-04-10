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
        integer_expr, float_expr, string_expr,
        /* Op */
        unary_op_expr, binary_op_expr,
        /* Code */
        variable_expr,
        call_expr,
        prototype_expr,
        function_expr,
        block_expr,
        /* Control flow */
        if_else_expr, for_expr, while_expr, do_while_expr, return_expr, break_expr, continue_expr,
    };

    static std::map<Type, std::string> ASTName {
        { Type::integer_expr   , "integer"        },
        { Type::float_expr     , "float"          },
        { Type::string_expr    , "string"         },
        { Type::variable_expr  , "variable"       },
        { Type::unary_op_expr  , "unary_op"       },
        { Type::binary_op_expr , "binary_op"      },
        { Type::call_expr      , "call"           },
        { Type::prototype_expr , "prototype"      },
        { Type::function_expr  , "function"       },
        { Type::return_expr    , "return"         },
        { Type::if_else_expr   , "if_else"        },
        { Type::for_expr       , "for"            },
        { Type::while_expr     , "while"          },
        { Type::do_while_expr  , "do_while"       },
        { Type::break_expr     , "break"          },
        { Type::continue_expr  , "continue"       },
        { Type::block_expr     , "block"          },
    };

    using IntType = unsigned long long;

    class ExprAST
    {
        public:
            Type SubType;
            IntType LineNumber;

            ExprAST() = delete;
            ExprAST(Type SubType) : SubType(SubType) { }
            virtual ~ExprAST() = default;

            void print_ast() 
            {
                std::cout << "ASTName {" << std::endl;
                std::cout << "  " << ASTName[SubType] << std::endl;
                std::cout << "}" << std::endl;
            }

            std::string get_ast_name() { return ASTName[SubType]; }
    };

    using Expr = std::shared_ptr<ExprAST>;

    class IntegerValueExprAST : public ExprAST
    {
        public:
            typedef long long value_type;
            value_type Val;
            IntegerValueExprAST(value_type Val) : ExprAST(Type::integer_expr), Val(Val) {}
        
    };

    class FloatValueExprAST : public ExprAST
    {
        public:
            typedef double value_type;
            value_type Val;
            FloatValueExprAST(value_type Val) : ExprAST(Type::float_expr), Val(Val) { }
        
    };

    class StringValueExprAST : public ExprAST
    {
        public:
            typedef std::string value_type;
            value_type Val;
            StringValueExprAST(const value_type& Val) :  ExprAST(Type::string_expr), Val(Val) { }
        
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
            Expr Expression;
            UnaryOpExprAST(const std::string& Op, Expr Expression) : ExprAST(Type::unary_op_expr), Op(Op), Expression(Expression) { }
        
    };

    class BinaryOpExprAST : public ExprAST
    {
        public:
            std::string Op;
            Expr LHS, RHS;
            BinaryOpExprAST(const std::string& Op, Expr LHS, Expr RHS) : ExprAST(Type::binary_op_expr), Op(Op), LHS(LHS), RHS(RHS) { }

    };

    // {   } => Block 
    class BlockExprAST : public ExprAST
    {
        public:
            std::vector<Expr> Statement;
            BlockExprAST() : ExprAST(Type::block_expr) { }
            BlockExprAST(Expr E) : ExprAST(Type::block_expr) { Statement.push_back(E); }
            BlockExprAST(std::vector<Expr> Statement) : ExprAST(Type::block_expr), Statement(Statement) { }
        
    };

    class PrototypeAST : public ExprAST
    {
        public:
            std::string Name;
            std::vector<Expr> Args;
            PrototypeAST(const std::string& Name, std::vector<Expr> Args) : ExprAST(Type::prototype_expr), Name(Name), Args(Args) { }
    };

    class CallExprAST : public ExprAST
    {
        public:
            std::string Callee;
            std::vector<Expr> Args;
            CallExprAST(const std::string& Callee, std::vector<Expr> Args) : ExprAST(Type::call_expr), Callee(Callee), Args(Args) { }

    };

    class FunctionAST : public ExprAST
    {
        public:
            std::shared_ptr<PrototypeAST> Proto;
            std::shared_ptr<BlockExprAST> Body;
            // declare
            FunctionAST(std::shared_ptr<PrototypeAST> Proto) : ExprAST(Type::function_expr), Proto(Proto) { }
            // define
            FunctionAST(std::shared_ptr<PrototypeAST> Proto, std::shared_ptr<BlockExprAST> Body) : ExprAST(Type::function_expr), Proto(Proto), Body(Body) { }

    };

    class ReturnExprAST : public ExprAST
    {
        public:
            Expr RetValue;
            ReturnExprAST() : ExprAST(Type::return_expr), RetValue(nullptr) { }
            ReturnExprAST(Expr RetValue) : ExprAST(Type::return_expr), RetValue(RetValue) { }
        
    };

    class BreakExprAST : public ExprAST { public: BreakExprAST() : ExprAST(Type::break_expr) { } };
    class ContinueExprAST : public ExprAST { public: ContinueExprAST() : ExprAST(Type::continue_expr) { } };

    class IfExprAST : public ExprAST
    {
        public:
            Expr Cond;
            std::shared_ptr<BlockExprAST> IfBlock;
            std::shared_ptr<BlockExprAST> ElseBlock;
            Expr ElseIf;

            // if (cond) ;
            // param: (point)
            IfExprAST(Expr Cond) : ExprAST(Type::if_else_expr), Cond(Cond) { }

            // if (cond) Block
            // param: (point, vector)
            IfExprAST(Expr Cond, std::shared_ptr<BlockExprAST> IfBlock) : ExprAST(Type::if_else_expr), Cond(Cond), IfBlock(IfBlock) { }

            // if (cond) Block else Block 
            // param: (point, vector, vector)
            IfExprAST(Expr Cond, std::shared_ptr<BlockExprAST> IfBlock, std::shared_ptr<BlockExprAST> ElseBlock) : ExprAST(Type::if_else_expr), Cond(Cond), IfBlock(IfBlock), ElseBlock(ElseBlock) { }

            // if (cond) Block else if (cond) Block ...
            // param: (point, vector, point)
            IfExprAST(Expr Cond, std::shared_ptr<BlockExprAST> IfBlock, Expr ElseIf) : ExprAST(Type::if_else_expr), Cond(Cond), IfBlock(IfBlock), ElseIf(ElseIf) { }
        
    };

    class ForExprAST : public ExprAST
    {
        public:
            std::vector<Expr> Cond;
            std::shared_ptr<BlockExprAST> Block;
            // for (cond) ;
            ForExprAST(std::vector<Expr> Cond) : ExprAST(Type::for_expr), Cond(Cond) { }
            // for (cond) { statement }
            ForExprAST(std::vector<Expr> Cond, std::shared_ptr<BlockExprAST> Block) : ExprAST(Type::for_expr), Cond(Cond), Block(Block) { }
        
    };

    class WhileExprAST : public ExprAST
    {
        public:
            Expr Cond;
            std::shared_ptr<BlockExprAST> Block;
            // while (cond) ;
            WhileExprAST(Expr Cond) : ExprAST(Type::while_expr), Cond(Cond) { }
            // while (cond) { statement }
            WhileExprAST(Expr Cond, std::shared_ptr<BlockExprAST> Block) : ExprAST(Type::while_expr), Cond(Cond), Block(Block) { }
        
    };

    class DoWhileExprAST : public ExprAST
    {
        public:
            std::shared_ptr<BlockExprAST> Block;
            Expr Cond;
            // do { statement } while (cond)
            DoWhileExprAST(std::shared_ptr<BlockExprAST> Block, Expr Cond) : ExprAST(Type::do_while_expr), Block(Block), Cond(Cond) { }
        
    };


    inline bool isInt      (Expr e) { return e->SubType == Type::integer_expr;   }
    inline bool isFloat    (Expr e) { return e->SubType == Type::float_expr;     }
    inline bool isString   (Expr e) { return e->SubType == Type::string_expr;    }
    inline bool isVariable (Expr e) { return e->SubType == Type::variable_expr;  }
    inline bool isUnaryOp  (Expr e) { return e->SubType == Type::unary_op_expr;  }
    inline bool isBinaryOp (Expr e) { return e->SubType == Type::binary_op_expr; }
    inline bool isCall     (Expr e) { return e->SubType == Type::call_expr;      }
    inline bool isPrototype(Expr e) { return e->SubType == Type::prototype_expr; }
    inline bool isFunction (Expr e) { return e->SubType == Type::function_expr;  }
    inline bool isReturn   (Expr e) { return e->SubType == Type::return_expr;    }
    inline bool isIf       (Expr e) { return e->SubType == Type::if_else_expr;   }
    inline bool isFor      (Expr e) { return e->SubType == Type::for_expr;       }
    inline bool isWhile    (Expr e) { return e->SubType == Type::while_expr;     }
    inline bool isDoWhile  (Expr e) { return e->SubType == Type::do_while_expr;  }
    inline bool isBreak    (Expr e) { return e->SubType == Type::break_expr;     }
    inline bool isContinue (Expr e) { return e->SubType == Type::continue_expr;  }
    inline bool isBlock    (Expr e) { return e->SubType == Type::block_expr;     }
}

#endif