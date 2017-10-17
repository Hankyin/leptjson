#ifndef _LEPTJSON_H_
#define _LEPTJSON_H_

#include <stdlib.h> /* size_t */

typedef enum //json数据类型
{
        LEPT_NULL,
        LEPT_FALSE,
        LEPT_TRUE,
        LEPT_NUMBER,
        LEPT_STRING,
        LEPT_ARRAY,
        LEPT_OBJECT
} lept_type;

typedef struct
{
        union 
        {
                struct { char* s; size_t len; };  /* string */
                double n;                          /* number */
        };//C11 新增了匿名 struct/union 语法，可以采用 v->n、v->s、v->len 来访问。
        lept_type type;
} lept_value;

enum //json语法检测结果
{
        LEPT_PARSE_OK = 0,
        LEPT_PARSE_EXPECT_VALUE,        /* 未发现json的值 */
        LEPT_PARSE_INVALID_VALUE,       /* json的值非法 */
        LEPT_PARSE_ROOT_NOT_SINGULAR,   /* json的值不止一个 */
        LEPT_PARSE_NUMBER_TOO_BIG,       /* json数字类型的值太大*/
        LEPT_PARSE_MISS_QUOTATION_MARK, /* 丢失引号 */
        LEPT_PARSE_INVALID_STRING_ESCAPE,/* 非法转义字符 */
        LEPT_PARSE_INVALID_STRING_CHAR,/* 非法字符串字符 */
        LEPT_PARSE_INVALID_UNICODE_HEX,
        LEPT_PARSE_INVALID_UNICODE_SURROGATE
};

void lept_init(lept_value *v);
void lept_free(lept_value *v);

int lept_parse(lept_value *v,const char *json);
lept_type lept_get_type(const lept_value *v);

//由于 lept_free() 实际上也会把 v 变成 null 值，我们只用一个宏来提供 lept_set_null() 这个 API。
#define lept_set_null(v) lept_free(v)

int lept_get_boolean(const lept_value* v);
void lept_set_boolean(lept_value* v, int b);

double lept_get_number(const lept_value* v);
void lept_set_number(lept_value* v, double n);

const char* lept_get_string(const lept_value* v);
size_t lept_get_string_length(const lept_value* v);
void lept_set_string(lept_value* v, const char* s, size_t len);

#endif