%{
#if HAVE_CONFIG_H
#include "config.h"
#endif
#include <ctype.h>
#include <string.h>

#include "array.h"
#include "arena.h"
#include "script.h"
#include "script-parser.h"

unsigned int character;
unsigned int line = 0;

static int
stringliteral(yyscan_t yyscanner);
%}
%option reentrant
%option noyywrap
%option bison-bridge
%option bison-locations
%option never-interactive
%%
0x[A-Fa-f0-9]+
-?[0-9]*(\.[0-9]+)?([eE][+-]?[0-9]+)? { yylval->p = arena_strdup(yyextra, yytext); character += yyleng; return Numeric; }

\"                     { return stringliteral(yyscanner); }

[A-Za-z-][A-za-z0-9.-]* { yylval->p = arena_strdup(yyextra, yytext); character += yyleng; return Identifier; }

[ \t\r\026]+           { character += yyleng; }

\n                     { ++line; character = 1; }

\357\273\277           { ; } 
.                      { ++character; return *yytext; }

<<EOF>>                { return EOF_; }
%%

static int
stringliteral(yyscan_t yyscanner)
{
  struct yyguts_t *yyg = (struct yyguts_t *) yyscanner;
  int ch;
  ARRAY(char) result;

  ARRAY_INIT(&result);

  while(EOF != (ch = input(yyscanner)))
  {
    if(ch == '\"')
      break;

    if(ch == '\n')
      {
        ++line;
        character = 1;
      }

    ARRAY_ADD(&result, ch);
  }

  yylval->p = arena_strndup(yyextra, &ARRAY_GET(&result, 0),
                            ARRAY_COUNT(&result));

  ARRAY_FREE(&result);

  return StringLiteral;
}

int
script_parse_file(struct script_parse_context *context, const char *path)
{
  YY_BUFFER_STATE buf;
  FILE* file;
  int result = -1;

  if(!(file = fopen(path, "r")))
    return -1;

  memset(context, 0, sizeof(*context));

  yylex_init(&context->scanner);
  arena_init(&context->statement_arena);

  context->error = 0;

  if(0 != (buf = yy_create_buffer(file, YY_BUF_SIZE, context->scanner)))
  {
    character = 1;
    line = 1;

    yy_switch_to_buffer(buf, context->scanner);
    yyset_extra(&context->statement_arena, context->scanner);
    result = yyparse(context);
    yy_delete_buffer(buf, context->scanner);
  }

  fclose(file);

  yylex_destroy(context->scanner);

  return result;
}
