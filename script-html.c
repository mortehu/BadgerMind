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

static void
script_dump_statement (struct ScriptStatement *statement, int level);

static void
indent (int level)
{
  while (level--)
    putchar (' ');
}

static void
script_dump_expression (struct ScriptExpression *expression, int isURI, int level)
{
  if (expression->offset)
    return;

  switch (expression->type)
    {
    case ScriptExpressionNumeric:

      printf ("%s", expression->lhs.numeric);

      break;

    case ScriptExpressionString:

      if (isURI)
        printf ("\"<a href=\"%1$s\">%1$s</a>\"", expression->lhs.numeric);
      else
        printf ("\"%s\"", expression->lhs.numeric);

      break;

    case ScriptExpressionBinary:

      printf ("<Binary Data>");

      break;

    case ScriptExpressionIdentifier:

      printf ("%s", expression->lhs.numeric);

      break;

    case ScriptExpressionStatement:

      script_dump_statement (expression->lhs.statement, level + 1);

      break;

    case ScriptExpressionParen:

      printf ("(");
      script_dump_expression (expression->lhs.expression, 0, level + 1);
      printf (")");

      break;

    case ScriptExpressionAdd:

      script_dump_expression (expression->lhs.expression, 0, level + 1);
      printf (" + ");
      script_dump_expression (expression->rhs, 0, level + 1);

      break;

    case ScriptExpressionSubtract:

      script_dump_expression (expression->lhs.expression, 0, level + 1);
      printf (" - ");
      script_dump_expression (expression->rhs, 0, level + 1);

      break;

    case ScriptExpressionMultiply:

      script_dump_expression (expression->lhs.expression, 0, level + 1);
      printf (" * ");
      script_dump_expression (expression->rhs, 0, level + 1);

      break;

    case ScriptExpressionDivide:

      script_dump_expression (expression->lhs.expression, 0, level + 1);
      printf (" / ");
      script_dump_expression (expression->rhs, 0, level + 1);

      break;

    case ScriptExpressionNegative:

      printf ("-");
      script_dump_expression (expression->lhs.expression, 0, level + 1);

      break;
    }
}

static void
script_dump_statement (struct ScriptStatement *statement, int level)
{
  struct ScriptParameter *parameter;

  if (statement->offset)
    return;

  printf ("(%s\n", statement->identifier);

  for (parameter = statement->parameters; parameter; parameter = parameter->next)
    {
      indent (level + 2);

      printf (" %s: ", parameter->identifier);
      script_dump_expression (parameter->expression, NULL != strstr (parameter->identifier, "URI"), level + 4 + strlen (parameter->identifier));

      if (parameter->next)
        printf ("\n");
    }

  printf (")");

  if (statement->next)
    {
      printf ("\n\n");
      script_dump_statement (statement->next, level);
    }
}

void
script_dump_html (struct script_parse_context *context)
{
  printf ("<!DOCTYPE html>\n"
          "<head>\n"
          "<title>Badgermind script</title>\n"
          "<style>body { white-space: pre; font-family: monospace; }</style>\n"
          "<body>");

  if (context->statements)
    {
      script_dump_statement (context->statements, 0);

      printf ("\n");
    }
}
