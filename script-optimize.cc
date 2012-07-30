#include <cstring>
#include <map>
#include <set>

#include "script.h"

typedef std::map<struct ScriptExpression *, std::set<struct ScriptExpression **>> script_ExpressionList;


static void
script_CollectExpressions (struct ScriptStatement *statement,
                           script_ExpressionList &container);

static void
script_CollectExpressions (struct ScriptExpression **expression,
                           script_ExpressionList &container)
{
  container[*expression].insert (expression);

  switch ((*expression)->type)
    {
    case ScriptExpressionStatement:

      script_CollectExpressions ((*expression)->lhs.statement, container);

      break;

    case ScriptExpressionParen:
    case ScriptExpressionNegative:
    case ScriptExpressionAbsolute:

      script_CollectExpressions (&(*expression)->lhs.expression, container);

      break;

    case ScriptExpressionAdd:
    case ScriptExpressionSubtract:
    case ScriptExpressionMultiply:
    case ScriptExpressionDivide:

      script_CollectExpressions (&(*expression)->lhs.expression, container);
      script_CollectExpressions (&(*expression)->rhs, container);

      break;

    case ScriptExpressionNumeric:
    case ScriptExpressionString:
    case ScriptExpressionBinary:
    case ScriptExpressionIdentifier:

      /* Nothing to do */

      break;
    }
}

static void
script_CollectExpressions (struct ScriptStatement *statement,
                           script_ExpressionList &container)
{
  while (statement)
    {
      struct ScriptParameter *parameter;

      for (parameter = statement->parameters;
           parameter; parameter = parameter->next)
        {
          script_CollectExpressions (&parameter->expression, container);
        }

      statement = statement->next;
    }
}

void
SCRIPT_Optimize (struct script_parse_context *context)
{
  script_ExpressionList expressions;
  bool dirty;

  script_CollectExpressions (context->statements, expressions);

  do
    {
      dirty = false;

      for (auto lhs = expressions.begin (); lhs != expressions.end (); ++lhs)
        {
          struct ScriptExpression *a;

          if (lhs->second.empty ())
            continue;

          a = lhs->first;

          for (auto rhs = lhs; ++rhs != expressions.end(); )
            {
              struct ScriptExpression *b;

              if (rhs->second.empty ())
                continue;

              b = rhs->first;

              if (a->type != b->type)
                continue;

              switch (a->type)
                {
                case ScriptExpressionNumeric:

                  if (strcmp (a->lhs.numeric, b->lhs.numeric)
                      || a->scale != b->scale)
                    continue;

                  break;

                case ScriptExpressionString:

                  if (strcmp (a->lhs.string, b->lhs.string))
                    continue;

                  break;

                case ScriptExpressionBinary:

                  if (strcmp (a->lhs.binary, b->lhs.binary))
                    continue;

                  break;

                case ScriptExpressionIdentifier:

                  if (strcmp (a->lhs.identifier, b->lhs.identifier))
                    continue;

                  break;

                case ScriptExpressionStatement:

                    {
                      struct ScriptParameter *pa, *pb;

                      if (strcmp (a->lhs.statement->identifier,
                                  b->lhs.statement->identifier))
                        {
                          continue;
                        }

                      pa = a->lhs.statement->parameters;
                      pb = b->lhs.statement->parameters;

                      while (pa && pb)
                        {
                          if (strcmp (pa->identifier, pb->identifier))
                            break;

                          if (pa->expression != pb->expression)
                            break;

                          pa = pa->next;
                          pb = pb->next;
                        }

                      if (pa || pb)
                        continue;
                    }

                  break;

                case ScriptExpressionParen:
                case ScriptExpressionNegative:
                case ScriptExpressionAbsolute:

                  if (a->lhs.expression != b->lhs.expression)
                    continue;

                  break;

                case ScriptExpressionAdd:
                case ScriptExpressionMultiply:

                  if (a->lhs.expression != b->lhs.expression
                      || a->rhs != b->rhs)
                    {
                      if (a->lhs.expression != b->rhs
                          || a->rhs != b->lhs.expression)
                        continue;
                    }

                  break;

                case ScriptExpressionSubtract:
                case ScriptExpressionDivide:

                  if (a->lhs.expression != b->lhs.expression
                      || a->rhs != b->rhs)
                    continue;

                  break;

                default:

                  continue;
                }

              for (auto pointer : rhs->second)
                *pointer = a;

              rhs->second.clear ();

              dirty = true;
            }
        }
    }
  while (dirty);
}
