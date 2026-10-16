#include "text_reply.h"
void TextReply_Init(TextReply *s,char *data,uint16_t capacity)
{s->data=data;s->capacity=capacity;s->length=0;s->failed=!data||!capacity;if(!s->failed){data[0]=0;}}
void TextReply_Add(TextReply *s,const char *text)
{
    if(s->failed){return;}
    while(*text){if(s->length+1U>=s->capacity){s->failed=1;return;}s->data[s->length++]=*text++;}
    s->data[s->length]=0;
}
void TextReply_U32(TextReply *s,uint32_t value)
{
    char digits[11];unsigned n=10;digits[n]=0;
    do{digits[--n]=(char)('0'+value%10U);value/=10U;}while(value);
    TextReply_Add(s,digits+n);
}
const char *TextReply_Result(TextReply *s)
{return s->failed?"ERR RESPONSE_SIZE\r\n":s->data;}
