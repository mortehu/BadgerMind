%pure-parser

%locations
%defines
%error-verbose

%parse-param { struct script_parse_context *context }
%lex-param { void* scanner }

%union
{
  void *p;
  long l;
}

%{
#include <stdio.h>

#include "script.h"
#include "arena.h"

#define ALLOC(t) do { t = arena_calloc(&context->statement_arena, sizeof(*t)); } while(0)

void
yyerror(YYLTYPE *loc, struct script_parse_context *context, const char *message);

#define scanner context->scanner
%}
%token EOF_

%token MINUS MUL DIV DEGREES

%token Numeric
%type<p> Numeric

%token Identifier
%type<p> Identifier

%token StringLiteral
%type<p> StringLiteral

%token BinaryLiteral
%type<p> BinaryLiteral

%type<p> statements statement
%type<p> parameters parameter
%type<p> expression

%left '=' '<' '>' '+' '-' '*' '/' UMINUS

%start document
%%
document
    : statements EOF_
      {
        context->statements = $1; 

        return 0;
      }
    ;

statements
    : statement statements 
      {
        struct ScriptStatement *lhs, *rhs;
        lhs = $1;
        rhs = $2;

        if (lhs)
        {
          lhs->next = rhs;
          $$ = lhs;
        }
        else
          $$ = rhs;
      }
    | statement
      {
        $$ = $1;
      }
    ;

statement
    : '(' Identifier parameters ')'
      {
        struct ScriptStatement *stmt;
        ALLOC (stmt);
        stmt->identifier = $2;
        stmt->parameters = $3;
        $$ = stmt;
      }
    ;

parameters
    :
      {
        $$ = 0;
      }
    | parameter parameters
      {
        struct ScriptParameter *lhs, *rhs;
        lhs = $1;
        rhs = $2;

        if (lhs)
        {
          lhs->next = rhs;
          $$ = lhs;
        }
        else
          $$ = rhs;
      }
    ;

parameter
    : Identifier ':' expression
      {
        struct ScriptParameter *param;
        ALLOC (param);
        param->identifier = $1;
        param->expression = $3;
        $$ = param;
      }
    ;

expression
    : Numeric
      {
        struct ScriptExpression *expr;
        ALLOC (expr);
        expr->type = ScriptExpressionNumeric;
        expr->lhs.numeric = $1;
        $$ = expr;
      }
    | Numeric DEGREES
      {
        struct ScriptExpression *expr;
        ALLOC (expr);
        expr->type = ScriptExpressionNumeric;
        expr->lhs.numeric = $1;
        expr->scale = 3.14159265358979323846 / 180.0;
        $$ = expr;
      }
    | Identifier
      {
        struct ScriptExpression *expr;
        ALLOC (expr);
        expr->type = ScriptExpressionIdentifier;
        expr->lhs.identifier = $1;
        $$ = expr;
      }
    | StringLiteral
      {
        struct ScriptExpression *expr;
        ALLOC (expr);
        expr->type = ScriptExpressionString;
        expr->lhs.string = $1;
        $$ = expr;
      }
    | BinaryLiteral
      {
        struct ScriptExpression *expr;
        ALLOC (expr);
        expr->type = ScriptExpressionBinary;
        expr->lhs.string = $1;
        $$ = expr;
      }
    | statement
      {
        struct ScriptExpression *expr;
        ALLOC (expr);
        expr->type = ScriptExpressionStatement;
        expr->lhs.statement = $1;
        $$ = expr;
      }
    | '(' expression ')'
      {
        struct ScriptExpression *expr;
        ALLOC (expr);
        expr->type = ScriptExpressionParen;
        expr->lhs.expression = $2;
        $$ = expr;
      }
    | '|' expression '|'
      {
        struct ScriptExpression *expr;
        ALLOC (expr);
        expr->type = ScriptExpressionAbsolute;
        expr->lhs.expression = $2;
        $$ = expr;
      }
    | expression '+' expression
      {
        struct ScriptExpression *expr;
        ALLOC (expr);
        expr->type = ScriptExpressionAdd;
        expr->lhs.expression = $1;
        expr->rhs = $3;
        $$ = expr;
      }
    | expression MINUS expression
      {
        struct ScriptExpression *expr;
        ALLOC (expr);
        expr->type = ScriptExpressionSubtract;
        expr->lhs.expression = $1;
        expr->rhs = $3;
        $$ = expr;
      }
    | expression MUL expression
      {
        struct ScriptExpression *expr;
        ALLOC (expr);
        expr->type = ScriptExpressionMultiply;
        expr->lhs.expression = $1;
        expr->rhs = $3;
        $$ = expr;
      }
    | expression DIV expression
      {
        struct ScriptExpression *expr;
        ALLOC (expr);
        expr->type = ScriptExpressionDivide;
        expr->lhs.expression = $1;
        expr->rhs = $3;
        $$ = expr;
      }
    | MINUS expression %prec UMINUS
      {
        struct ScriptExpression *expr;
        ALLOC (expr);
        expr->type = ScriptExpressionNegative;
        expr->lhs.expression = $2;
        $$ = expr;
      }
    ;
%%
#include <stdio.h>

extern unsigned int character;
extern unsigned int line;

void
yyerror(YYLTYPE *loc, struct script_parse_context *context, const char *message)
{
  fprintf(stderr, "\033[31;1m%u:%u:Parse error: %s\n\033[00m\n",
          line, character, message);
  context->error = 1;
}
