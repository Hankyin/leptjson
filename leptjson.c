#include "leptjson.h"
#include <assert.h>  /* assert() */
#include <stdlib.h>  /* NULL strtod() */
#include <string.h>  /* strlen() */
#include <errno.h>   /* errno, ERANGE */
#include <math.h>    /* HUGE_VAL */

#define EXPECT(c, ch) do { assert(*c->json == (ch)); c->json++; } while(0)

#define ISDIGIT(ch)         ((ch) >= '0' && (ch) <= '9')
#define ISDIGIT1TO9(ch)     ((ch) >= '1' && (ch) <= '9')

#define PUTC(c, ch) do { *(char*)lept_context_push(c, sizeof(char)) = (ch); } while(0)
#define STRING_ERROR(ret) do { c->top = head; return ret; } while(0)

#ifndef LEPT_PARSE_STACK_INIT_SIZE
#define LEPT_PARSE_STACK_INIT_SIZE 256
#endif

typedef struct
{
        char *stack;
        size_t size,top;
        const char *json;
} lept_context;

static void* lept_context_push(lept_context* c, size_t size) 
{
        void* ret;
        assert(size > 0);
        if (c->top + size >= c->size) {
            if (c->size == 0)
                c->size = LEPT_PARSE_STACK_INIT_SIZE;
            while (c->top + size >= c->size)
                c->size += c->size >> 1;  /* c->size * 1.5 */
            c->stack = (char*)realloc(c->stack, c->size);
        }
        ret = c->stack + c->top;
        c->top += size;
        return ret;
}
    
static void* lept_context_pop(lept_context* c, size_t size) 
{
        assert(c->top >= size);
        return c->stack + (c->top -= size);
}

static void lept_parse_whitespace(lept_context *c)
{
        const char *p = c->json;
        //去掉字符串开头的空白
        while(*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
                p++;
        c->json = p;
}

static int lept_parse_literal(lept_context *c,lept_value *v,const char *literal,lept_type type)
{       
        int len = strlen(literal);
        const char *p = c->json;
        for(int i = 0;i < len;i++)
        {
                if(p[i] != literal[i])
                {
                        return LEPT_PARSE_INVALID_VALUE;
                }
        }
        c->json += len;
        v->type = type;
        return LEPT_PARSE_OK;
}

static int lept_parse_number(lept_context *c,lept_value *v)
{
        const char* p = c->json;
        /*
        数字的格式
        number = [ "-" ] int [ frac ] [ exp ]
        int = "0" / digit1-9 *digit
        frac = "." 1*digit
        exp = ("e" / "E") ["-" / "+"] 1*digit

        先检测传入的字符串是否合法，然后使用strtod直接转换。
        */
        //一下代码判断开头字符是否是- 0 或 1-9 ，否则返回非法错误
        if(*p == '-')
                p++;
        if(*p == '0')
        {
                p++;
        }              
        else 
        {
                if(!ISDIGIT1TO9(*p))
                        return LEPT_PARSE_INVALID_VALUE;
                p++;
                for(;ISDIGIT(*p);p++);
        }
        //从至少第二位开始，可以有. ，且.后必须有数字。
        if(*p == '.')
        {
                p++;
                if(!ISDIGIT(*p))
                {
                        return LEPT_PARSE_INVALID_VALUE;
                }
                for(;ISDIGIT(*p);p++);
        }
        //从至少第二位开始，可以有e/E ，且e/E后可以有+- ,但必须有0-9
        if(*p == 'e' || *p == 'E')
        {
                p++;
                if(*p == '+' || *p == '-')
                {
                       p++; 
                }
                if(!ISDIGIT(*p))
                {
                        return LEPT_PARSE_INVALID_VALUE;  
                }
                for(;ISDIGIT(*p);p++);
        }

        errno = 0;
        v->n = strtod(c->json, NULL);
        if (errno == ERANGE && (v->n == HUGE_VAL || v->n == -HUGE_VAL))
            return LEPT_PARSE_NUMBER_TOO_BIG;
        v->type = LEPT_NUMBER;
        c->json = p;
        return LEPT_PARSE_OK;
}

static const char* lept_parse_hex4(const char* p, unsigned* u) 
{
        int i;
        *u = 0;
        for (i = 0; i < 4; i++) {
            char ch = *p++;
            *u <<= 4;
            if      (ch >= '0' && ch <= '9')  *u |= ch - '0';
            else if (ch >= 'A' && ch <= 'F')  *u |= ch - ('A' - 10);
            else if (ch >= 'a' && ch <= 'f')  *u |= ch - ('a' - 10);
            else return NULL;
        }
        return p;
}
    
static void lept_encode_utf8(lept_context* c, unsigned u) 
{
        if(0x0000 <= u && u <= 0x007f)
        {
                PUTC(c, u & 0xFF);
        }
        else if(0x0080 <= u && u <= 0x07ff)
        {
                PUTC(c, 0xC0 | ((u >> 6) & 0xFF));
                PUTC(c, 0x80 | ( u       & 0x3F));
        }
        else if(0x0800 <= u && u <= 0x7fff)
        {
                PUTC(c, 0xE0 | ((u >> 12) & 0xFF));
                PUTC(c, 0x80 | ((u >>  6) & 0x3F));
                PUTC(c, 0x80 | ( u        & 0x3F));
        }
        else if(0x10000 <= u && u <= 0x10ffff)
        {
                assert(u <= 0x10FFFF);
                PUTC(c, 0xF0 | ((u >> 18) & 0xFF));
                PUTC(c, 0x80 | ((u >> 12) & 0x3F));
                PUTC(c, 0x80 | ((u >>  6) & 0x3F));
                PUTC(c, 0x80 | ( u        & 0x3F));
        }
}

static int lept_parse_string(lept_context* c, lept_value* v) 
{
        size_t head = c->top, len;
        const char* p;
        unsigned int u;//用来存放码点
        unsigned int u2;
        EXPECT(c, '\"');
        p = c->json;
        for (;;) 
        {
            char ch = *p++;
            switch (ch) 
            {
                case '\"'://字符串结束
                    len = c->top - head;
                    lept_set_string(v, (const char*)lept_context_pop(c, len), len);
                    c->json = p;
                    return LEPT_PARSE_OK;
                case '\\'://处理转义字符
                    switch (*p++) 
                    {
                        case '\"': PUTC(c, '\"'); break;
                        case '\\': PUTC(c, '\\'); break;
                        case '/':  PUTC(c, '/' ); break;
                        case 'b':  PUTC(c, '\b'); break;
                        case 'f':  PUTC(c, '\f'); break;
                        case 'n':  PUTC(c, '\n'); break;
                        case 'r':  PUTC(c, '\r'); break;
                        case 't':  PUTC(c, '\t'); break;
                        case 'u':  
                                if (!(p = lept_parse_hex4(p, &u)))
                                        STRING_ERROR(LEPT_PARSE_INVALID_UNICODE_HEX);
                                if (u >= 0xD800 && u <= 0xDBFF) 
                                { /* surrogate pair */
                                        if (*p++ != '\\')
                                                STRING_ERROR(LEPT_PARSE_INVALID_UNICODE_SURROGATE);
                                        if (*p++ != 'u')
                                                STRING_ERROR(LEPT_PARSE_INVALID_UNICODE_SURROGATE);
                                        if (!(p = lept_parse_hex4(p, &u2)))
                                                STRING_ERROR(LEPT_PARSE_INVALID_UNICODE_HEX);
                                        if (u2 < 0xDC00 || u2 > 0xDFFF)
                                                STRING_ERROR(LEPT_PARSE_INVALID_UNICODE_SURROGATE);
                                        u = (((u - 0xD800) << 10) | (u2 - 0xDC00)) + 0x10000;
                                }
                                lept_encode_utf8(c, u);
                                break;
                        default:
                                STRING_ERROR(LEPT_PARSE_INVALID_STRING_ESCAPE);
                    }
                    break;
                case '\0'://不应当出现这个字符，字符串的最后一个字符应当是"
                    STRING_ERROR(LEPT_PARSE_MISS_QUOTATION_MARK);
                default:
                    if ((unsigned char)ch < 0x20) 
                    { 
                        STRING_ERROR(LEPT_PARSE_INVALID_STRING_CHAR);
                    }
                    PUTC(c, ch);
            }
        }
}

static int lept_parse_value(lept_context *c,lept_value *v)
{
        //检测传入字符串的第一个字符
        switch(*c->json)
        {
                case 'n':       return lept_parse_literal(c,v,"null",LEPT_NULL);
                case 'f':       return lept_parse_literal(c,v,"false",LEPT_FALSE);
                case 't':       return lept_parse_literal(c,v,"true",LEPT_TRUE);
                default:        return lept_parse_number(c,v);
                case '"':       return lept_parse_string(c,v);
                case '\0':      return LEPT_PARSE_EXPECT_VALUE;
                
        }
}

//json分析主函数
int lept_parse(lept_value *v,const char *json)
{
        lept_context c;
        int ret;
        assert(v != NULL);//检测空指针
        c.json = json;
        c.stack = NULL;//创建 lept_context 的时候初始化 stack 并最终释放内存
        c.size = c.top = 0;
        lept_init(v);
        lept_parse_whitespace(&c);
        if ((ret = lept_parse_value(&c, v)) == LEPT_PARSE_OK) 
        {
            lept_parse_whitespace(&c);
            if (lept_parse_value(&c, v) != LEPT_PARSE_EXPECT_VALUE)
            {
                v->type = LEPT_NULL;
                ret = LEPT_PARSE_ROOT_NOT_SINGULAR;
            }  
        }
        assert(c.top == 0);//确保lept_context中的所有数据都被弹出
        free(c.stack);
        return ret;
}

void lept_init(lept_value *v) 
{
        v->type = LEPT_NULL;
}

void lept_free(lept_value *v)
{
        assert(v != NULL);
        if (v->type == LEPT_STRING)
            free(v->s);
        v->type = LEPT_NULL;//这句可以避免重复释放，我觉得我想是不到的。
}

lept_type lept_get_type(const lept_value* v) 
{
        assert(v != NULL);
        return v->type;
}

int lept_get_boolean(const lept_value* v)
{
        assert(v != NULL && (v->type == LEPT_TRUE || v->type == LEPT_FALSE));
        return v->type == LEPT_TRUE;//这个返回比较巧妙
}
void lept_set_boolean(lept_value* v, int b)
{
        lept_free(v);//注意释放，原来的有可能是字符串之类的
        v->type = b ? LEPT_TRUE : LEPT_FALSE;
}

double lept_get_number(const lept_value *v)
{
        assert(v != NULL && v->type == LEPT_NUMBER);
        return v->n;
}

void lept_set_number(lept_value *v,double n)
{
        lept_free(v);
        v->n = n;
        v->type = LEPT_NUMBER;
}

const char* lept_get_string(const lept_value* v) 
{
        assert(v != NULL && v->type == LEPT_STRING);
        return v->s;
}
    
size_t lept_get_string_length(const lept_value* v)
{
        assert(v != NULL && v->type == LEPT_STRING);
        return v->len;
}
    
void lept_set_string(lept_value* v, const char* s, size_t len) 
{
        assert(v != NULL && (s != NULL || len == 0));
        lept_free(v);
        v->s = (char*)malloc(len + 1);
        memcpy(v->s, s, len);
        v->s[len] = '\0';
        v->len = len;
        v->type = LEPT_STRING;
}
    