
#include "euboea.h"
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

int32_t lex(char * code) {
    int32_t codeSize = strlen(code), line = 1;
    int is_crlf = 0;
    for (int32_t i = 0; i < codeSize; i++) {
        if (tok_t.size <= i)
            tok_t.tok_t = realloc(tok_t.tok_t, (tok_t.size += 512 * sizeof(Token)));
        if (isdigit(code[i])) {
            for (; isdigit(code[i]); i++)
                strncat(tok_t.tok_t[tok_t.pos].val, &(code[i]), 1);
            tok_t.tok_t[tok_t.pos].nline = line;
            i--;
            tok_t.pos++;
        } else if (isalpha(code[i])) {
            char * str = tok_t.tok_t[tok_t.pos].val;
            for (; isalpha(code[i]) || isdigit(code[i]) || code[i] == '_'; i++)
                *str++ = code[i];
            tok_t.tok_t[tok_t.pos].nline = line;
            i--;
            tok_t.pos++;
        } else if (code[i] == ' ' || code[i] == '\t') {
        } else if (code[i] == '#') {
            for (i++; code[i] != '\n'; i++)    line++;
        } else if (code[i] == '"') {
            strcpy(tok_t.tok_t[tok_t.pos].val, "\"");
            tok_t.tok_t[tok_t.pos++].nline = line;
            for (i++; code[i] != '"' && code[i] != '\0'; i++)
                strncat(tok_t.tok_t[tok_t.pos].val, &(code[i]), 1);
            tok_t.tok_t[tok_t.pos].nline = line;
            if (code[i] == '\0')
                error("%d: expected expression '\"'", tok_t.tok_t[tok_t.pos].nline);
            tok_t.pos++;
        } else if (code[i] == '\n' || (is_crlf = (code[i] == '\r' && code[i + 1] == '\n'))) {
            i += is_crlf;
            strcpy(tok_t.tok_t[tok_t.pos].val, ";");
            tok_t.tok_t[tok_t.pos].nline = line++;
            tok_t.pos++;
        } else {
            strncat(tok_t.tok_t[tok_t.pos].val, &(code[i]), 1);
            if (code[i + 1] == '=' || (code[i] == '+' && code[i + 1] == '+') || (code[i] == '-' && code[i + 1] == '-'))
                strncat(tok_t.tok_t[tok_t.pos].val, &(code[++i]), 1);
            tok_t.tok_t[tok_t.pos].nline = line;
            tok_t.pos++;
        }
    }
    tok_t.tok_t[tok_t.pos].nline = line;
    tok_t.size = tok_t.pos - 1;
    return 0;
}
