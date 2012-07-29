#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <assert.h>
#include <err.h>
#include <getopt.h>
#include <stdint.h>
#include <string.h>
#include <sysexits.h>

#include <vector>

#include "script.h"
#include "script-vm.h"

static std::vector<uint32_t> script_pointerOffsets;

static size_t SCRIPT_dumpOffset;
static int SCRIPT_64bit, SCRIPT_pointerAlign;

static void
SCRIPT_EmitStatement (struct ScriptStatement *statement);

static void
SCRIPT_EmitByte (unsigned int byte)
{
  putchar (byte & 0xff);
  SCRIPT_dumpOffset++;
}

static void
SCRIPT_EmitU32 (unsigned int u32)
{
  assert (!(SCRIPT_dumpOffset & 3));

  putchar (u32 & 0xff);
  putchar ((u32 >> 8) & 0xff);
  putchar ((u32 >> 16) & 0xff);
  putchar ((u32 >> 24) & 0xff);
  SCRIPT_dumpOffset += 4;
}

static void
SCRIPT_EmitVarInt (unsigned int u32)
{
  if (u32 > (0x7f << 14))
    {
      putchar (((u32 >> 21) & 0x7f) | 0x80);
      ++SCRIPT_dumpOffset;
    }

  if (u32 > (0x7f << 7))
    {
      putchar (((u32 >> 14) & 0x7f) | 0x80);
      ++SCRIPT_dumpOffset;
    }

  if (u32 > 0x7f)
    {
      putchar (((u32 >> 7) & 0x7f) | 0x80);
      ++SCRIPT_dumpOffset;
    }

  putchar (u32 & 0x7f);
  ++SCRIPT_dumpOffset;
}

static void
SCRIPT_EmitPointer (off_t ptr)
{
  assert (!(SCRIPT_dumpOffset & 3));

  if (ptr)
    script_pointerOffsets.push_back (SCRIPT_dumpOffset);

  putchar (ptr & 0xff);
  putchar ((ptr >> 8) & 0xff);
  putchar ((ptr >> 16) & 0xff);
  putchar ((ptr >> 24) & 0xff);

  SCRIPT_dumpOffset += 4;

  if (SCRIPT_64bit)
    {
      putchar ((ptr >> 32) & 0xff);
      putchar ((ptr >> 40) & 0xff);
      putchar ((ptr >> 48) & 0xff);
      putchar ((ptr >> 56) & 0xff);

      SCRIPT_dumpOffset += 4;
    }
}

static void
SCRIPT_Align (unsigned int bias, unsigned int mask)
{
  while ((SCRIPT_dumpOffset + bias) & mask)
    SCRIPT_EmitByte (0xaa);
}

static off_t
SCRIPT_EmitNumeric (const char *numeric, double scale)
{
  off_t result;

  union
    {
      float v_float;
      uint32_t v_floatAsU32;
    } v_float;

  char *end;

  SCRIPT_Align (1, 3);

  result = SCRIPT_dumpOffset;

  if (!scale)
    {
      int16_t v_longlong;

      v_longlong = strtoll (numeric, &end, 0);

      if (!*end && v_longlong >= 0 && v_longlong <= 0x7FFFFFFF)
        {
          SCRIPT_EmitByte (ScriptVMExpressionU32);
          SCRIPT_EmitByte (v_longlong & 0xFF);
          SCRIPT_EmitByte ((v_longlong >> 8) & 0xFF);
          SCRIPT_EmitByte ((v_longlong >> 16) & 0xFF);
          SCRIPT_EmitByte ((v_longlong >> 24) & 0xFF);

          return result;
        }
    }

  v_float.v_float = strtod (numeric, &end);

  if (scale)
    v_float.v_float *= scale;

  assert (!*end);

  SCRIPT_EmitByte (ScriptVMExpressionFloat);
  SCRIPT_EmitByte (v_float.v_floatAsU32 & 0xFF);
  SCRIPT_EmitByte ((v_float.v_floatAsU32 >> 8) & 0xFF);
  SCRIPT_EmitByte ((v_float.v_floatAsU32 >> 16) & 0xFF);
  SCRIPT_EmitByte ((v_float.v_floatAsU32 >> 24) & 0xFF);

  return result;
}

static void
SCRIPT_EmitStringBytes (const char *string)
{
  while (*string)
    SCRIPT_EmitByte (*string++);
}

static off_t
SCRIPT_EmitString (const char *string)
{
  off_t result;

  result = SCRIPT_dumpOffset;

  SCRIPT_EmitByte (ScriptVMExpressionString);

  SCRIPT_EmitStringBytes (string);

  SCRIPT_EmitByte (0);

  return result;
}

static off_t
SCRIPT_EmitBinary (const char *string)
{
  off_t result;
  size_t i, length = 0;
  const unsigned char *c;

  length = strlen (string) / 2;

  SCRIPT_Align (1, 3);

  result = SCRIPT_dumpOffset;

  SCRIPT_EmitByte (ScriptVMExpressionBinary);
  SCRIPT_EmitU32 (length);

  c = (const unsigned char *) string;

  for (i = 0; i < length; ++i, c += 2)
    {
      static const unsigned char helper[] =
        {
          0, 10, 11, 12, 13, 14, 15,  0,
          0,  0,  0,  0,  0,  0,  0,  0,
          0,  1,  2,  3,  4,  5,  6,  7,
          8,  9,  0,  0,  0,  0,  0,  0
        };

      SCRIPT_EmitByte ((helper[c[0] & 0x1f] << 4) | helper[c[1] & 0x1f]);
    }

  return result;
}

static void
SCRIPT_EmitIdentifier (const char *string)
{
  SCRIPT_EmitByte (ScriptVMExpressionIdentifier);

  SCRIPT_EmitStringBytes (string);

  SCRIPT_EmitByte (0);
}

static void
SCRIPT_EmitExpression (struct ScriptExpression *expression)
{
  if (expression->offset)
    return;

  switch (expression->type)
    {
    case ScriptExpressionNumeric:

      expression->offset = SCRIPT_EmitNumeric (expression->lhs.numeric, expression->scale);

      break;

    case ScriptExpressionString:

      expression->offset = SCRIPT_EmitString (expression->lhs.string);

      break;

    case ScriptExpressionBinary:

      expression->offset = SCRIPT_EmitBinary (expression->lhs.binary);

      break;

    case ScriptExpressionIdentifier:

      expression->offset = SCRIPT_dumpOffset;
      SCRIPT_EmitIdentifier (expression->lhs.string);

      break;

    case ScriptExpressionStatement:

      SCRIPT_EmitStatement (expression->lhs.statement);

      SCRIPT_Align (1, SCRIPT_pointerAlign);
      expression->offset = SCRIPT_dumpOffset;
      SCRIPT_EmitByte (ScriptVMExpressionStatement);
      SCRIPT_EmitPointer (expression->lhs.statement->offset);

      break;

    case ScriptExpressionParen:

      SCRIPT_EmitExpression (expression->lhs.expression);

      SCRIPT_Align (1, SCRIPT_pointerAlign);
      expression->offset = SCRIPT_dumpOffset;
      SCRIPT_EmitByte (ScriptVMExpressionParen);
      SCRIPT_EmitPointer (expression->lhs.expression->offset);

      break;

    case ScriptExpressionAbsolute:

      SCRIPT_EmitExpression (expression->lhs.expression);

      SCRIPT_Align (1, SCRIPT_pointerAlign);
      expression->offset = SCRIPT_dumpOffset;
      SCRIPT_EmitByte (ScriptVMExpressionAbsolute);
      SCRIPT_EmitPointer (expression->lhs.expression->offset);

      break;

    case ScriptExpressionNegative:

      SCRIPT_EmitExpression (expression->lhs.expression);

      SCRIPT_Align (1, SCRIPT_pointerAlign);
      expression->offset = SCRIPT_dumpOffset;
      SCRIPT_EmitByte (ScriptVMExpressionNegative);
      SCRIPT_EmitPointer (expression->lhs.expression->offset);

      break;

    case ScriptExpressionAdd:

      SCRIPT_EmitExpression (expression->lhs.expression);
      SCRIPT_EmitExpression (expression->rhs);

      SCRIPT_Align (1, SCRIPT_pointerAlign);
      expression->offset = SCRIPT_dumpOffset;
      SCRIPT_EmitByte (ScriptVMExpressionAdd);
      SCRIPT_EmitPointer (expression->lhs.expression->offset);
      SCRIPT_EmitPointer (expression->rhs->offset);

      break;

    case ScriptExpressionSubtract:

      SCRIPT_EmitExpression (expression->lhs.expression);
      SCRIPT_EmitExpression (expression->rhs);

      SCRIPT_Align (1, SCRIPT_pointerAlign);
      expression->offset = SCRIPT_dumpOffset;
      SCRIPT_EmitByte (ScriptVMExpressionSubtract);
      SCRIPT_EmitPointer (expression->lhs.expression->offset);
      SCRIPT_EmitPointer (expression->rhs->offset);

      break;

    case ScriptExpressionMultiply:

      SCRIPT_EmitExpression (expression->lhs.expression);
      SCRIPT_EmitExpression (expression->rhs);

      SCRIPT_Align (1, SCRIPT_pointerAlign);
      expression->offset = SCRIPT_dumpOffset;
      SCRIPT_EmitByte (ScriptVMExpressionMultiply);
      SCRIPT_EmitPointer (expression->lhs.expression->offset);
      SCRIPT_EmitPointer (expression->rhs->offset);

      break;

    case ScriptExpressionDivide:

      SCRIPT_EmitExpression (expression->lhs.expression);
      SCRIPT_EmitExpression (expression->rhs);

      SCRIPT_Align (1, SCRIPT_pointerAlign);
      expression->offset = SCRIPT_dumpOffset;
      SCRIPT_EmitByte (ScriptVMExpressionDivide);
      SCRIPT_EmitPointer (expression->lhs.expression->offset);
      SCRIPT_EmitPointer (expression->rhs->offset);

      break;
    }
}

static void
SCRIPT_EmitStatement (struct ScriptStatement *statement)
{
  struct ScriptParameter *parameter;

  if (statement->offset)
    return;

  if (statement->next)
    SCRIPT_EmitStatement (statement->next);

  for (parameter = statement->parameters; parameter; parameter = parameter->next)
    SCRIPT_EmitExpression (parameter->expression);

  statement->offset = SCRIPT_dumpOffset;

  SCRIPT_EmitStringBytes (statement->identifier);

  for (parameter = statement->parameters; parameter; parameter = parameter->next)
    {
      SCRIPT_EmitByte (':');
      SCRIPT_EmitStringBytes (parameter->identifier);
    }

  SCRIPT_EmitByte (0);
  SCRIPT_Align (0, SCRIPT_pointerAlign);

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
SCRIPT_EmitPointerTable (void)
{
  uint32_t previous = 0;

  for (auto offset : script_pointerOffsets)
    {
      assert (offset != previous);
      assert (!(offset & SCRIPT_pointerAlign));

      SCRIPT_EmitVarInt ((offset - previous) / (SCRIPT_pointerAlign + 1));
      previous = offset;
    }

  SCRIPT_EmitByte (0);
}

void
script_dump_binary (struct script_parse_context *context, int ptrsize)
{
  size_t pointerTableOffset;
  script_pointerOffsets.clear ();

  SCRIPT_EmitByte (0xBA);
  SCRIPT_EmitByte (0xD9);
  SCRIPT_EmitByte (0xE2);
  SCRIPT_EmitByte (0x01);

  if (!context->statements)
    return;

  if (0 != (SCRIPT_64bit = (ptrsize == 64)))
    SCRIPT_pointerAlign = 7;
  else
    SCRIPT_pointerAlign = 3;

  SCRIPT_EmitStatement (context->statements);

  SCRIPT_Align (0, SCRIPT_pointerAlign);

  pointerTableOffset = SCRIPT_dumpOffset;
  SCRIPT_EmitPointerTable ();

  SCRIPT_Align (0, SCRIPT_pointerAlign);
  SCRIPT_EmitPointer (pointerTableOffset);

  SCRIPT_EmitPointer (context->statements->offset);
}
