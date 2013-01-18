//
//  main.c
//  blissc
//
//  Created by Matthew Madison on 10/22/12.
//  Copyright (c) 2012 Matthew Madison. All rights reserved.
//

#include <stdio.h>
#include <string.h>
#include "scanner.h"
#include "blissc/parser.h"
#include "blissc/lexer.h"
#include "blissc/nametable.h"
#include "blissc/lexeme.h"
#include "blissc/machinedef.h"
#include "blissc/expression.h"
#include "blissc/declarations.h"
#include "blissc/listings.h"
#include "blissc/support/logging.h"
#include "blissc/support/fileio.h"

int test_scanner(int argc, const char *argv[]);
int test_parser(int argc, const char *argv[]);
int test_expr(int argc, const char *argv[]);

int main(int argc, const char * argv[])
{
    const char *which;

    if (argc < 2) {
        fprintf(stderr, "Usage: %s scanner <filename>\n", argv[0]);
        fprintf(stderr, "Usage: %s parser  <filename>\n", argv[0]);
        fprintf(stderr, "Usage: %s expr    <filename>\n", argv[0]);
        return 997;
    }
    which = argv[1];
    argc -= 2;
    argv += 2;

    if (strcmp(which, "scanner") == 0) {
        return test_scanner(argc, argv);
    } else if (strcmp(which, "parser") == 0) {
        return test_parser(argc, argv);
    } else if (strcmp(which, "expr") == 0) {
        return test_expr(argc, argv);
    }

    fprintf(stderr, "*** unrecognized test: %s", which);
    return 1001;

} /* main */

/*
 * test_scanner
 * 
 * Run a file through the scanner.
 */
int
test_scanner (int argc, const char *argv[])
{
    scanctx_t ctx = 0;
    jmp_buf retenv;
    strctx_t strctx = 0;
    logctx_t logctx = 0;
    fioctx_t fioctx = 0;
    streamctx_t sctx = 0;
    strdesc_t *tok;
    scantype_t toktype;
    unsigned int sflags = SCAN_M_SIGNOK;
    unsigned int lno, cno;
    int keepgoing = 1;

    if (setjmp(retenv)) goto finish;
    strctx = strings_init();
    logctx =logging_init(retenv);
    fioctx = fileio_init(logctx);
    ctx = scan_init(strctx, logctx, fioctx);

    if (ctx == 0) {
        return 999;
    }
    if (!(sctx = scan_fopen(ctx, argv[0], strlen(argv[0]), 0))) {
        fprintf(stderr, "scan_fopen failed for %s\n", argv[0]);
        return 998;
    }
    while (keepgoing) {
        toktype = scan_getnext(sctx, sflags, &tok, &lno, &cno);
        if (scan_ok(toktype)) {
            printf("<%u:%u> ", lno, cno);
            switch (toktype) {
                case SCANTYPE_DECLITERAL:
                    printf("Decimal literal: %-*.*s\n", tok->len,tok->len,tok->ptr);
                    sflags &= ~SCAN_M_SIGNOK;
                    break;
                case SCANTYPE_IDENTIFIER:
                    printf("Identifier:      %-*.*s\n", tok->len,tok->len,tok->ptr);
                    sflags &= ~SCAN_M_SIGNOK;
                    break;
                case SCANTYPE_OPERATOR:
                    printf("Operator:        %-*.*s\n", tok->len,tok->len,tok->ptr);
                    sflags |= SCAN_M_SIGNOK;
                    break;
                case SCANTYPE_PUNCTUATION:
                    printf("Punctuation:     %-*.*s\n", tok->len,tok->len,tok->ptr);
                    if (strchr("]>)", *tok->ptr) != 0) {
                        sflags &= ~SCAN_M_SIGNOK;
                    } else {
                        sflags |= SCAN_M_SIGNOK;
                    }
                    break;
                case SCANTYPE_QUOTEDSTRING:
                    printf("Quoted string:   %-*.*s\n", tok->len,tok->len,tok->ptr);
                    sflags |= SCAN_M_SIGNOK;
                    break;
                case SCANTYPE_END:
                    printf("<< end of input >>\n");
                    keepgoing = 0;
                    break;
                default:
                    printf("-- unhandled token type %u --\n",
                           (unsigned int)toktype);
                    keepgoing = 0;
                    break;
            }
        } else {
            printf("***ERROR: %u\n", (unsigned int)toktype);
            keepgoing = 0;
        }
    }
finish:
    scan_finish(ctx);
    logging_finish(logctx);
    return 0;
}

static int linewidth = 0;
static char *delim = "";


void PRINTCR (void)
{
    printf("\n");
    linewidth = 0;
    delim = "";
}

void PRINTEXPR(strctx_t strctx, expr_node_t *exp);

void PRINTLEX(lexeme_t *lex)
{
    lextype_t lt = lexeme_type(lex);
    strdesc_t *text = lexeme_text(lex);
    const char *typename = lextype_name(lt);

    if (lt == LEXTYPE_UNBOUND) {
        const char *btname = lextype_name(lexeme_boundtype(lex));
        linewidth += printf("%sU[%s]<%-*.*s>", delim, btname,
                            text->len, text->len, text->ptr);
    } else {
        linewidth += printf("%s%s<%-*.*s>", delim, typename,
                            text->len, text->len, text->ptr);
    }
    if (linewidth > 60) {
        delim = "\n";
        linewidth = 0;
    } else {
        delim = " ";
    }
}
/*
 * test_parser
 *
 * Test the lexical processor.
 */
int
test_parser (int argc, const char *argv[])
{
    parse_ctx_t pctx = 0;
    scopectx_t kwdscope;
    jmp_buf retenv;
    strctx_t strctx = 0;
    fioctx_t fioctx = 0;
    logctx_t logctx = 0;
    lexctx_t lctx;
    lexeme_t *lex;
    lextype_t lt;
//    int linewidth;
//    char *delim;
    machinedef_t machdef = { .bpunit=8, .bpval=32, .bpaddr=32, .signext_supported=1 };

    if (setjmp(retenv)) goto finish;
    strctx = strings_init();
    logctx = logging_init(retenv);
    fioctx = fileio_init(logctx);
    pctx = parser_init(strctx, 0, &machdef, &kwdscope, logctx, fioctx);
    lctx = parser_lexmemctx(pctx);
    if (!parser_fopen(pctx, argv[0], strlen(argv[0]), 0)) {
        fprintf(stderr, "parser_fopen failed for %s\n", argv[0]);
        return 998;
    }
//    linewidth = 0;
//    delim = "";
    for (lt = parser_next(pctx, QL_NORMAL, &lex); lt != LEXTYPE_END && lt != LEXTYPE_NONE;
         lt = parser_next(pctx, QL_NORMAL, &lex)) {
        PRINTLEX(lex);
        lexeme_free(lctx, lex);
    }
    if (lt == LEXTYPE_NONE) {
        fprintf(stderr, "parser_next returned error lexeme\n");
        return 997;
    } else {
        printf("<<end of input>>\n");
    }
finish:
    parser_finish(pctx);
    logging_finish(logctx);
    return 0;
}

/*
 * test_expr
 *
 * Test expression parsing.
 */
int
test_expr (int argc, const char *argv[])
{
    parse_ctx_t pctx = 0;
    strctx_t strctx = 0;
    logctx_t logctx = 0;
    fioctx_t fioctx = 0;
    expr_ctx_t  ectx = 0;
    scopectx_t kwdscope;
    jmp_buf retenv;
//    lexeme_t *lex;
//   lextype_t lt;
//    int linewidth;
//    char *delim;
    machinedef_t machdef = { .bpunit=8, .bpval=64, .bpaddr=64,
        .signext_supported=1, .max_align=3, .reg_count = 16, .ltc_initializers = 1 };

    if (setjmp(retenv)) {
        goto finish;
    }

    strctx = strings_init();
    logctx = logging_init(retenv);
    fioctx = fileio_init(logctx);
    pctx = parser_init(strctx, 0, &machdef, &kwdscope, logctx, fioctx);
    ectx = expr_init(strctx, pctx, kwdscope);
    if (!parser_fopen_main(pctx, argv[0], strlen(argv[0]), 0, 0, 0)) {
        fprintf(stderr, "parser_fopen failed for %s\n", argv[0]);
        return 998;
    }
    if (!declare_module(ectx)) {
        fprintf(stderr, "declare_module failed\n");
        return 997;
    } else {
        printf("<<end of module>>\n");
    }
finish:
    expr_finish(ectx);
    logging_finish(logctx);
    fileio_finish(fioctx);
    strings_finish(strctx);
    return 0;
}

void PRINTEXPR_internal(int level, strctx_t strctx, expr_node_t *exp)
{
    static char pfx[] = "                    +";
    if (level >= sizeof(pfx)) level = sizeof(pfx)-1;
    if (exp == 0) return;
    switch (expr_type(exp)) {
        case EXPTYPE_NOOP:
            printf("%-*.*s{NOOP}\n", level, level, pfx);
            break;
        case EXPTYPE_PRIM_BLK: {
            expr_node_t *e;
            printf("%-*.*s{BLK:BEGIN}\n", level, level, pfx);
            for (e = exprseq_head(expr_blk_seq(exp)); e != 0; e = expr_next(e)) {
                PRINTEXPR_internal(level+1, strctx, e);
            }
            printf("%-*.*s{BLK:END}\n", level, level, pfx);
            break;
        }
        case EXPTYPE_PRIM_STRUREF:
            printf("%-*.*s{STRUREF<}\n", level, level, pfx);
            PRINTEXPR_internal(level+1, strctx, expr_struref_accexpr(exp));
            printf("%-*.*s{>STRUREF}\n", level, level, pfx);
            break;
        case EXPTYPE_PRIM_LIT:
            printf("%-*.*s{LIT=%ld}\n", level, level, pfx, expr_litval(exp));
            break;
        case EXPTYPE_PRIM_SEG: {
            strdesc_t *str;
            if (expr_seg_name(exp) != 0) {
                str = name_string(expr_seg_name(exp));
            } else {
                str = string_printf(strctx, 0, "<UNKNOWN>");
            }
            printf("%-*.*s{SEG:%-*.*s(%lx)}\n",
                   level, level, pfx, str->len, str->len, str->ptr,
                   expr_seg_offset(exp));
            break;
        }
        case EXPTYPE_PRIM_FLDREF:
            printf("%-*.*s{FLDREF<}\n", level, level, pfx);
            PRINTEXPR_internal(level+1, strctx, expr_fldref_addr(exp));
            PRINTEXPR_internal(level+1, strctx, expr_fldref_pos(exp));
            PRINTEXPR_internal(level+1, strctx, expr_fldref_size(exp));
            printf("%-*.*s{>FLDREF}\n", level, level, pfx);
            break;
        case EXPTYPE_OPERATOR:
            printf("%-*.*s{%s<}\n", level, level, pfx, oper_name(expr_op_type(exp)));
            PRINTEXPR_internal(level+1, strctx, expr_op_lhs(exp));
            PRINTEXPR_internal(level+1, strctx, expr_op_rhs(exp));
            printf("%-*.*s{>%s}\n", level, level, pfx, oper_name(expr_op_type(exp)));
            break;
        default:
            printf("%-*.*s{EXPTYPE:%d}\n", level, level, pfx, expr_type(exp));
            break;
    }
}
void PRINTEXPR(strctx_t strctx, expr_node_t *exp) {
    PRINTEXPR_internal(0, strctx, exp);
}

