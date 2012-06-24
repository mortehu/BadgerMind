#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <assert.h>
#include <err.h>
#include <getopt.h>
#include <stdint.h>
#include <string.h>
#include <sysexits.h>

#include "script.h"
#include "script-vm.h"

static size_t SCRIPT_dumpOffset;

static void
script_dump_statement (struct ScriptStatement *statement);

static void
SCRIPT_EmitByte (unsigned int byte)
{
  putchar (byte & 0xff);
  SCRIPT_dumpOffset++;
}

static void
SCRIPT_EmitPointer (off_t byte)
{
  putchar (byte & 0xff);
  putchar ((byte >> 8) & 0xff);
  putchar ((byte >> 16) & 0xff);
  putchar ((byte >> 24) & 0xff);

  SCRIPT_dumpOffset += 4;
}

static void
SCRIPT_EmitNumeric (const char *numeric)
{
  union
    {
      float v_float;
      uint32_t v_floatAsU32;
    } v_float;

  int16_t v_longlong;
  char *end;

  v_longlong = strtoll (numeric, &end, 0);

  if (!*end && v_longlong >= 0 && v_longlong <= 0xFFFFFFFF)
    {
      SCRIPT_EmitByte (ScriptVMExpressionU32);
      SCRIPT_EmitByte (v_longlong & 0xFF);
      SCRIPT_EmitByte ((v_longlong >> 8) & 0xFF);
      SCRIPT_EmitByte ((v_longlong >> 16) & 0xFF);
      SCRIPT_EmitByte ((v_longlong >> 24) & 0xFF);

      return;
    }

  v_float.v_float = strtod (numeric, &end);

  assert (!*end);

  SCRIPT_EmitByte (ScriptVMExpressionFloat);
  SCRIPT_EmitByte (v_float.v_floatAsU32 & 0xFF);
  SCRIPT_EmitByte ((v_float.v_floatAsU32 >> 8) & 0xFF);
  SCRIPT_EmitByte ((v_float.v_floatAsU32 >> 16) & 0xFF);
  SCRIPT_EmitByte ((v_float.v_floatAsU32 >> 24) & 0xFF);
}

static void
SCRIPT_EmitStringBytes (const char *string)
{
  while (*string)
    SCRIPT_EmitByte (*string++);
}

static void
SCRIPT_EmitString (const char *string)
{
  SCRIPT_EmitByte (ScriptVMExpressionString);

  SCRIPT_EmitStringBytes (string);

  SCRIPT_EmitByte (0);
}

static void
SCRIPT_EmitIdentifier (const char *string)
{
  SCRIPT_EmitByte (ScriptVMExpressionIdentifier);

  SCRIPT_EmitStringBytes (string);

  SCRIPT_EmitByte (0);
}

static void
script_dump_expression (struct ScriptExpression *expression)
{
  if (expression->offset)
    return;

  switch (expression->type)
    {
    case ScriptExpressionNumeric:

      expression->offset = SCRIPT_dumpOffset;
      SCRIPT_EmitNumeric (expression->lhs.numeric);

      break;

    case ScriptExpressionString:

      expression->offset = SCRIPT_dumpOffset;
      SCRIPT_EmitString (expression->lhs.string);

      break;

    case ScriptExpressionIdentifier:

      expression->offset = SCRIPT_dumpOffset;
      SCRIPT_EmitIdentifier (expression->lhs.string);

      break;

    case ScriptExpressionStatement:

      script_dump_statement (expression->lhs.statement);

      expression->offset = SCRIPT_dumpOffset;
      SCRIPT_EmitByte (ScriptVMExpressionStatement);
      SCRIPT_EmitPointer (expression->lhs.statement->offset);

      break;

    case ScriptExpressionParen:

      script_dump_expression (expression->lhs.expression);

      expression->offset = SCRIPT_dumpOffset;
      SCRIPT_EmitByte (ScriptVMExpressionParen);
      SCRIPT_EmitPointer (expression->lhs.expression->offset);

      break;

    case ScriptExpressionAdd:

      script_dump_expression (expression->lhs.expression);
      script_dump_expression (expression->rhs);

      expression->offset = SCRIPT_dumpOffset;
      SCRIPT_EmitByte (ScriptVMExpressionAdd);
      SCRIPT_EmitPointer (expression->lhs.expression->offset);
      SCRIPT_EmitPointer (expression->rhs->offset);

      break;

    case ScriptExpressionSubtract:

      script_dump_expression (expression->lhs.expression);
      script_dump_expression (expression->rhs);

      expression->offset = SCRIPT_dumpOffset;
      SCRIPT_EmitByte (ScriptVMExpressionSubtract);
      SCRIPT_EmitPointer (expression->lhs.expression->offset);
      SCRIPT_EmitPointer (expression->rhs->offset);

      break;

    case ScriptExpressionMultiply:

      script_dump_expression (expression->lhs.expression);
      script_dump_expression (expression->rhs);

      expression->offset = SCRIPT_dumpOffset;
      SCRIPT_EmitByte (ScriptVMExpressionMultiply);
      SCRIPT_EmitPointer (expression->lhs.expression->offset);
      SCRIPT_EmitPointer (expression->rhs->offset);

      break;

    case ScriptExpressionDivide:

      script_dump_expression (expression->lhs.expression);
      script_dump_expression (expression->rhs);

      expression->offset = SCRIPT_dumpOffset;
      SCRIPT_EmitByte (ScriptVMExpressionDivide);
      SCRIPT_EmitPointer (expression->lhs.expression->offset);
      SCRIPT_EmitPointer (expression->rhs->offset);

      break;
    }
}

static void
script_dump_statement (struct ScriptStatement *statement)
{
  struct ScriptParameter *parameter;

  if (statement->offset)
    return;

  if (statement->next)
    script_dump_statement (statement->next);

  for (parameter = statement->parameters; parameter; parameter = parameter->next)
    script_dump_expression (parameter->expression);

  statement->offset = SCRIPT_dumpOffset;

  SCRIPT_EmitStringBytes (statement->identifier);

  for (parameter = statement->parameters; parameter; parameter = parameter->next)
    {
      SCRIPT_EmitByte (':');
      SCRIPT_EmitStringBytes (parameter->identifier);
    }

  SCRIPT_EmitByte (0);

  for (parameter = statement->parameters; parameter; parameter = parameter->next)
    {
      if (!parameter->expression->offset)
        fprintf (stderr, "Warning: Parameter has zero offset\n");

      SCRIPT_EmitPointer (parameter->expression->offset);
    }

  if (statement->next)
    SCRIPT_EmitPointer (statement->next->offset);
  else
    SCRIPT_EmitPointer (0);
}

void
script_dump_binary (struct script_parse_context *context)
{
  SCRIPT_EmitByte (0xBA);
  SCRIPT_EmitByte (0xD9);
  SCRIPT_EmitByte (0xE2);

  if (context->statements)
    {
      script_dump_statement (context->statements);

      SCRIPT_EmitPointer (context->statements->offset);
    }
}
