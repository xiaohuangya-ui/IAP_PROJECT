#ifndef TEXT_REPLY_H
#define TEXT_REPLY_H
#include <stdint.h>
/* 有界ASCII构造；容量包含末尾NUL，失败锁存，绝不输出截断成功回复。 */
typedef struct {char *data;uint16_t capacity,length;uint8_t failed;} TextReply;
void TextReply_Init(TextReply *s,char *data,uint16_t capacity);
void TextReply_Add(TextReply *s,const char *text);
void TextReply_U32(TextReply *s,uint32_t value);
const char *TextReply_Result(TextReply *s);
#endif
