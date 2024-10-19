#include <prep.hpp>

#define NSTAK   1024
#define SGN     0
#define UNS     1
#define UND     2

#define UNSMARK 0x1000

struct value final {
        long val;
        long type;
};

// conversion types
enum class conversion_type : unsigned char { RELAT = 1, ARITH, LOGIC, SPCL, SHIFT, UNARY };

struct priority {
        char            pri;
        char            arity;
        conversion_type ctype;
};

// operator priority, arity, and conversion type, indexed by tokentype
constexpr priority op_priority[] = {
    {  0, 0,                      0 }, // END
    {  0, 0,                      0 }, // UNCLASS
    {  0, 0,                      0 }, // NAME
    {  0, 0,                      0 }, // NUMBER
    {  0, 0,                      0 }, // STRING
    {  0, 0,                      0 }, // CCON
    {  0, 0,                      0 }, // NL
    {  0, 0,                      0 }, // WS
    {  0, 0,                      0 }, // DSHARP
    { 11, 2, conversion_type::RELAT }, // EQ
    { 11, 2, conversion_type::RELAT }, // NEQ
    { 12, 2, conversion_type::RELAT }, // LEQ
    { 12, 2, conversion_type::RELAT }, // GEQ
    { 13, 2, conversion_type::SHIFT }, // LSH
    { 13, 2, conversion_type::SHIFT }, // RSH
    {  7, 2, conversion_type::LOGIC }, // LAND
    {  6, 2, conversion_type::LOGIC }, // LOR
    {  0, 0,                      0 }, // PPLUS
    {  0, 0,                      0 }, // MMINUS
    {  0, 0,                      0 }, // ARROW
    {  0, 0,                      0 }, // SBRA
    {  0, 0,                      0 }, // SKET
    {  3, 0,                      0 }, // LP
    {  3, 0,                      0 }, // RP
    {  0, 0,                      0 }, // DOT
    { 10, 2, conversion_type::ARITH }, // AND
    { 15, 2, conversion_type::ARITH }, // STAR
    { 14, 2, conversion_type::ARITH }, // PLUS
    { 14, 2, conversion_type::ARITH }, // MINUS
    { 16, 1, conversion_type::UNARY }, // TILDE
    { 16, 1, conversion_type::UNARY }, // NOT
    { 15, 2, conversion_type::ARITH }, // SLASH
    { 15, 2, conversion_type::ARITH }, // PCT
    { 12, 2, conversion_type::RELAT }, // LT
    { 12, 2, conversion_type::RELAT }, // GT
    {  9, 2, conversion_type::ARITH }, // CIRC
    {  8, 2, conversion_type::ARITH }, // OR
    {  5, 2,  conversion_type::SPCL }, // QUEST
    {  5, 2,  conversion_type::SPCL }, // COLON
    {  0, 0,                      0 }, // ASGN
    {  4, 2,                      0 }, // COMMA
    {  0, 0,                      0 }, // SHARP
    {  0, 0,                      0 }, // SEMIC
    {  0, 0,                      0 }, // CBRA
    {  0, 0,                      0 }, // CKET
    {  0, 0,                      0 }, // ASPLUS
    {  0, 0,                      0 }, // ASMINUS
    {  0, 0,                      0 }, // ASSTAR
    {  0, 0,                      0 }, // ASSLASH
    {  0, 0,                      0 }, // ASPCT
    {  0, 0,                      0 }, // ASCIRC
    {  0, 0,                      0 }, // ASLSH
    {  0, 0,                      0 }, // ASRSH
    {  0, 0,                      0 }, // ASOR
    {  0, 0,                      0 }, // ASAND
    {  0, 0,                      0 }, // ELLIPS
    {  0, 0,                      0 }, // DSHARP1
    {  0, 0,                      0 }, // NAME1
    { 16, 1, conversion_type::UNARY }, // DEFINED
    { 16, 0, conversion_type::UNARY }, // UMINUS
};

// forward declarations
int   evalop(struct priority);
value tokval(token*);

value     vals[NSTAK + 1], *vp;
token_type ops[NSTAK + 1], *op;

// Evaluates an #if #elif #ifdef #ifndef line.  trp->tp points to the keyword.
long eval(_In_ token_row* trp, _In_ const keyword_type& keyword) noexcept {
    token* tp {};
    Nlist* np {};
    int    ntok {}, rand {};

    trp->tp++;
    if (keyword == keyword_type::KIFDEF || keyword == keyword_type::KIFNDEF) {
        if (trp->lp - trp->bp != 4 || trp->tp->type != token_type::NAME) {
            error(ERROR, "Syntax error in #ifdef/#ifndef");
            return 0;
        }
        np = lookup(trp->tp, 0);
        return (keyword == keyword_type::KIFDEF) == (np && np->flag & (ISDEFINED | ISMAC));
    }
    ntok           = trp->tp - trp->bp;
    kwdefined->val = keyword_type::KDEFINED; // activate special meaning of defined
    expandrow(trp, "<if>", NOT_IN_MACRO);
    kwdefined->val = NAME;
    vp             = vals;
    op             = ops;
    *op++          = END;
    for (rand = 0, tp = trp->bp + ntok; tp < trp->lp; tp++) {
        if (op >= ops + NSTAK) sysfatal("cpp: can't evaluate #if: increase NSTAK");
        switch (tp->type) {
            case token_type::WS :
            case token_type::NL : continue;

            // nilary
            case token_type::NAME :
            case token_type::NAME1 :
            case token_type::NUMBER :
            case token_type::CCON :
            case token_type::STRING :
                if (rand) goto syntax;
                *vp++ = tokval(tp);
                rand  = 1;
                continue;

            // unary
            case token_type::DEFINED :
            case token_type::TILDE :
            case token_type::NOT :
                if (rand) goto syntax;
                *op++ = tp->type;
                continue;

            // unary-binary
            case token_type::PLUS :
            case token_type::MINUS :
            case token_type::STAR :
            case token_type::AND :
                if (rand == 0) {
                    if (tp->type == MINUS) *op++ = UMINUS;
                    if (tp->type == STAR || tp->type == AND) {
                        error(ERROR, "Illegal operator * or & in #if/#elif");
                        return 0;
                    }
                    continue;
                }
                [[fallthrough]];

            // plain binary
            case EQ :
            case NEQ :
            case LEQ :
            case GEQ :
            case LSH :
            case RSH :
            case LAND :
            case LOR :
            case SLASH :
            case PCT :
            case LT :
            case GT :
            case CIRC :
            case OR :
            case QUEST :
            case COLON :
            case COMMA :
                if (rand == 0) goto syntax;
                if (evalop(op_priority[tp->type]) != 0) return 0;
                *op++ = tp->type;
                rand  = 0;
                continue;

            case LP :
                if (rand) goto syntax;
                *op++ = LP;
                continue;

            case RP :
                if (!rand) goto syntax;
                if (evalop(op_priority[RP]) != 0) return 0;
                if (op <= ops || op[-1] != LP) goto syntax;
                op--;
                continue;

            default : error(ERROR, "Bad operator (%t) in #if/#elif", tp); return 0;
        }
    }
    if (rand == 0) goto syntax;
    if (evalop(op_priority[END]) != 0) return 0;
    if (op != &ops[1] || vp != &vals[1]) {
        error(ERROR, "Botch in #if/#elif");
        return 0;
    }
    if (vals[0].type == UND) error(ERROR, "Undefined expression value");
    return vals[0].val;
syntax:
    error(ERROR, "Syntax error in #if/#elif");
    return 0;
}

int evalop(struct priority pri) noexcept {
    struct value v1, v2;
    long         rv1, rv2;
    int          rtype, oper;

    rv2   = 0;
    rtype = 0;
    while (pri.pri < op_priority[op[-1]].pri) {
        oper = *--op;
        if (op_priority[oper].arity == 2) {
            v2  = *--vp;
            rv2 = v2.val;
        }
        v1  = *--vp;
        rv1 = v1.val;
        switch (op_priority[oper].ctype) {
            case 0 :
            default : error(WARNING, "Syntax error in #if/#endif"); return 1;
            case ARITH :
            case RELAT :
                if (v1.type == UNS || v2.type == UNS)
                    rtype = UNS;
                else
                    rtype = SGN;
                if (v1.type == UND || v2.type == UND) rtype = UND;
                if (op_priority[oper].ctype == RELAT && rtype == UNS) {
                    oper  |= UNSMARK;
                    rtype  = SGN;
                }
                break;
            case SHIFT :
                if (v1.type == UND || v2.type == UND)
                    rtype = UND;
                else
                    rtype = v1.type;
                if (rtype == UNS) oper |= UNSMARK;
                break;
            case UNARY : rtype = v1.type; break;
            case LOGIC :
            case SPCL  : break;
        }
        switch (oper) {
            case EQ :
            case EQ | UNSMARK  : rv1 = rv1 == rv2; break;
            case NEQ           :
            case NEQ | UNSMARK : rv1 = rv1 != rv2; break;
            case LEQ           : rv1 = rv1 <= rv2; break;
            case GEQ           : rv1 = rv1 >= rv2; break;
            case LT            : rv1 = rv1 < rv2; break;
            case GT            : rv1 = rv1 > rv2; break;
            case LEQ | UNSMARK : rv1 = (unsigned long) rv1 <= rv2; break;
            case GEQ | UNSMARK : rv1 = (unsigned long) rv1 >= rv2; break;
            case LT | UNSMARK  : rv1 = (unsigned long) rv1 < rv2; break;
            case GT | UNSMARK  : rv1 = (unsigned long) rv1 > rv2; break;
            case LSH           : rv1 <<= rv2; break;
            case LSH | UNSMARK : rv1 = (unsigned long) rv1 << rv2; break;
            case RSH           : rv1 >>= rv2; break;
            case RSH | UNSMARK : rv1 = (unsigned long) rv1 >> rv2; break;
            case LAND :
                rtype = UND;
                if (v1.type == UND) break;
                if (rv1 != 0) {
                    if (v2.type == UND) break;
                    rv1 = rv2 != 0;
                } else
                    rv1 = 0;
                rtype = SGN;
                break;
            case LOR :
                rtype = UND;
                if (v1.type == UND) break;
                if (rv1 == 0) {
                    if (v2.type == UND) break;
                    rv1 = rv2 != 0;
                } else
                    rv1 = 1;
                rtype = SGN;
                break;
            case AND   : rv1 &= rv2; break;
            case STAR  : rv1 *= rv2; break;
            case PLUS  : rv1 += rv2; break;
            case MINUS : rv1 -= rv2; break;
            case UMINUS :
                if (v1.type == UND) rtype = UND;
                rv1 = -rv1;
                break;
            case OR    : rv1 |= rv2; break;
            case CIRC  : rv1 ^= rv2; break;
            case TILDE : rv1 = ~rv1; break;
            case NOT :
                rv1 = !rv1;
                if (rtype != UND) rtype = SGN;
                break;
            case SLASH :
                if (rv2 == 0) {
                    rtype = UND;
                    break;
                }
                if (rtype == UNS)
                    rv1 /= (unsigned long) rv2;
                else
                    rv1 /= rv2;
                break;
            case PCT :
                if (rv2 == 0) {
                    rtype = UND;
                    break;
                }
                if (rtype == UNS)
                    rv1 %= (unsigned long) rv2;
                else
                    rv1 %= rv2;
                break;
            case COLON :
                if (op[-1] != QUEST)
                    error(ERROR, "Bad ?: in #if/endif");
                else {
                    op--;
                    if ((--vp)->val == 0) v1 = v2;
                    rtype = v1.type;
                    rv1   = v1.val;
                }
                break;
            case DEFINED : break;
            default      : error(ERROR, "Eval botch (unknown operator)"); return 1;
        }
        v1.val  = rv1;
        v1.type = rtype;
        *vp++   = v1;
    }
    return 0;
}

struct value tokval(token* tp) {
    struct value  v;
    Nlist*        np;
    int           i, base, c, longcc;
    unsigned long n;
    Rune          r;
    uchar*        p;

    v.type = SGN;
    v.val  = 0;
    switch (tp->type) {
        case NAME : v.val = 0; break;

        case NAME1 :
            if ((np = lookup(tp, 0)) && np->flag & (ISDEFINED | ISMAC)) v.val = 1;
            break;

        case NUMBER :
            n          = 0;
            base       = 10;
            p          = tp->t;
            c          = p[tp->len];
            p[tp->len] = '\0';
            if (*p == '0') {
                base = 8;
                if (p[1] == 'x' || p[1] == 'X') {
                    base = 16;
                    p++;
                }
                p++;
            }
            for (;; p++) {
                if ((i = digit(*p)) < 0) break;
                if (i >= base) error(WARNING, "Bad digit in number %t", tp);
                n *= base;
                n += i;
            }
            if (n >= 0x80000000 && base != 10) v.type = UNS;
            for (; *p; p++) {
                if (*p == 'u' || *p == 'U')
                    v.type = UNS;
                else if (*p == 'l' || *p == 'L') {
                } else {
                    error(ERROR, "Bad number %t in #if/#elif", tp);
                    break;
                }
            }
            v.val          = n;
            tp->t[tp->len] = c;
            break;

        case CCON :
            n      = 0;
            p      = tp->t;
            longcc = 0;
            if (*p == 'L') {
                p      += 1;
                longcc  = 1;
            }
            p += 1;
            if (*p == '\\') {
                p += 1;
                if ((i = digit(*p)) >= 0 && i <= 7) {
                    n  = i;
                    p += 1;
                    if ((i = digit(*p)) >= 0 && i <= 7) {
                        p  += 1;
                        n <<= 3;
                        n  += i;
                        if ((i = digit(*p)) >= 0 && i <= 7) {
                            p  += 1;
                            n <<= 3;
                            n  += i;
                        }
                    }
                } else if (*p == 'x') {
                    p += 1;
                    while ((i = digit(*p)) >= 0 && i <= 15) {
                        p  += 1;
                        n <<= 4;
                        n  += i;
                    }
                } else {
                    static char cvcon[] = "a\ab\bf\fn\nr\rt\tv\v''\"\"??\\\\";
                    for (i = 0; i < sizeof(cvcon); i += 2) {
                        if (*p == cvcon[i]) {
                            n = cvcon[i + 1];
                            break;
                        }
                    }
                    p += 1;
                    if (i >= sizeof(cvcon)) error(WARNING, "Undefined escape in character constant");
                }
            } else if (*p == '\'')
                error(ERROR, "Empty character constant");
            else {
                i  = chartorune(&r, (char*) p);
                n  = r;
                p += i;
                if (i > 1 && longcc == 0) error(WARNING, "Undefined character constant");
            }
            if (*p != '\'')
                error(WARNING, "Multibyte character constant undefined");
            else if (n > 127 && longcc == 0)
                error(WARNING, "Character constant taken as not signed");
            v.val = n;
            break;

        case STRING : error(ERROR, "String in #if/#elif"); break;
    }
    return v;
}

int digit(int i) {
    if ('0' <= i && i <= '9')
        i -= '0';
    else if ('a' <= i && i <= 'f')
        i -= 'a' - 10;
    else if ('A' <= i && i <= 'F')
        i -= 'A' - 10;
    else
        i = -1;
    return i;
}
