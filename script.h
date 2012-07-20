#include <stdio.h>
#include <sys/types.h>

#include "arena.h"

struct script_parse_context
{
  void *scanner;
  struct arena_info statement_arena;
  int error;

  struct ScriptStatement *statements;
};

struct ScriptStatement
{
  const char *identifier;
  struct ScriptParameter *parameters;

  struct ScriptStatement *next;

  off_t offset;
};

struct ScriptParameter
{
  const char *identifier;
  struct ScriptExpression *expression;

  struct ScriptParameter *next;
};

enum ScriptExpressionType
{
  /* Unary */
  ScriptExpressionNumeric,
  ScriptExpressionString,
  ScriptExpressionBinary,
  ScriptExpressionIdentifier,
  ScriptExpressionStatement,
  ScriptExpressionParen,
  ScriptExpressionNegative,
  ScriptExpressionAbsolute,

  /* Binary */
  ScriptExpressionAdd,
  ScriptExpressionSubtract,
  ScriptExpressionMultiply,
  ScriptExpressionDivide,
};

struct ScriptExpression
{
  enum ScriptExpressionType type;

  union
  {
    struct ScriptExpression *expression;
    struct ScriptStatement *statement;
    const char *string;
    const char *binary;
    const char *numeric;
    const char *identifier;
  } lhs;

  struct ScriptExpression *rhs;

  double scale;
  off_t offset;
};

int
script_parse_file(struct script_parse_context *context, FILE *file);

void
script_dump (struct script_parse_context *context);
