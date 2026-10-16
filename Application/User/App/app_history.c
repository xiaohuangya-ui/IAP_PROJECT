/* Business独占持久温度历史；先扫扇区头，再扫最近两代的全部记录。
 * 每次最多16次32B读取，避免启动时全量扫描1MiB阻塞业务进展。
 * 启动/Trial不擦写；所有危险擦写只在NORMAL且业务健康的poll内执行。 */
#include "app_history.h"
#include "image_contract.h"
#include "wire_codec.h"
#include "byte_buffer.h"
#include <string.h>
#define HEADER_MAGIC 0x48535431U
#define RECORD_MAGIC 0x544A5231U
#define SECTORS 256U
#define SLOTS 127U
static AppHistoryPort io;
static AppHistoryView view;
static uint32_t scan,phase,generation,second_generation,sector,second_sector;
static uint32_t first_sequence,next_slot,last_sample_ms,dirty_headers;
static uint8_t virgin_checked;
static uint8_t packet[32];
static int read_record(uint32_t sec,uint32_t slot)
{return io.read(sec*4096U+32U+slot*32U,packet,32U);}
static int valid(uint32_t magic)
{return Wire_ReadLE32(packet)==magic && Wire_ReadLE32(packet+28U)==CRC32_IEEE(packet,28U);}
static void error(void){view.state=HISTORY_ERROR;}
void AppHistory_Bind(const AppHistoryPort *p)
{
    memset(&view,0,sizeof(view));memset(&io,0,sizeof(io));
    scan=phase=generation=second_generation=sector=second_sector=first_sequence=next_slot=last_sample_ms=dirty_headers=0U;virgin_checked=0U;
    if(p && p->start && p->read && p->program && p->erase_sector){io=*p;}
    else{error();}
}
void AppHistory_Get(AppHistoryView *out){if(out){*out=view;}}
static void remember(void)
{
    AppHistoryRecord r;uint32_t i;
    r.sequence=Wire_ReadLE32(packet+4U);r.version=Wire_ReadLE32(packet+8U);
    r.uptime_ms=Wire_ReadLE32(packet+12U);r.temp_x10=(int32_t)Wire_ReadLE32(packet+16U);r.raw=Wire_ReadLE32(packet+20U);
    if(view.count==4U){for(i=0;i<3U;++i){view.recent[i]=view.recent[i+1U];}}
    else{++view.count;}
    view.recent[view.count-1U]=r;view.sequence=r.sequence;
}
static int record_valid(void)
{
    int32_t t=(int32_t)Wire_ReadLE32(packet+16U);
    uint32_t seq=Wire_ReadLE32(packet+4U);
    return valid(RECORD_MAGIC) && seq && seq<0xFFFFFFFEU &&
        Wire_ReadLE32(packet+8U) && t>=-400 && t<=1250 &&
        Wire_ReadLE32(packet+20U)>0U && Wire_ReadLE32(packet+20U)<4095U && Wire_ReadLE32(packet+24U)==0U;
}
static int commit_packet(uint32_t offset)
{
    uint8_t check[32];
    Wire_WriteLE32(packet+28U,CRC32_IEEE(packet,28U));
    /* 先字段再CRC，两个Program均由驱动回读；最后还需全32B独立回读。
     * 写裂记录保留为脏槽，下次复位扫描跳过，绝不原位修补NOR。 */
    return io.program(offset,packet,28U) && io.program(offset+28U,packet+28U,4U) &&
        io.read(offset,check,32U) && !memcmp(check,packet,32U);
}
static int new_sector(uint32_t destination,int erase)
{
    if(generation>=0xFFFFFFFDU || view.sequence>=0xFFFFFFFDU){return 0;}
    if(erase){
        if(destination==sector || !io.erase_sector(destination*4096U)){return 0;}
    }else if(!virgin_checked){return 0;}
    memset(packet,0,28U);Wire_WriteLE32(packet,HEADER_MAGIC);
    Wire_WriteLE32(packet+4U,generation+1U);Wire_WriteLE32(packet+8U,view.sequence+1U);
    if(!commit_packet(destination*4096U)){return 0;}
    ++generation;sector=destination;first_sequence=view.sequence+1U;next_slot=0U;return 1;
}
void AppHistory_Poll(uint32_t now,uint32_t version,int32_t temp,uint16_t raw,int allowed)
{
    uint32_t budget,g,first,destination;
    if(!allowed || view.state==HISTORY_ERROR){return;}
    if(view.state==HISTORY_WAIT){
        if(!io.start()){error();return;}view.state=HISTORY_SCAN;
    }
    if(view.state==HISTORY_SCAN){
        for(budget=0;budget<16U;++budget){
            if(phase==3U){
                /* 首个目标扇区逐片检查；不在一个poll里循环读完整4KB。 */
                if(!io.read(scan*32U,packet,32U)||!Bytes_AllFF(packet,32U)){error();return;}
                if(++scan==128U){virgin_checked=1U;view.state=HISTORY_READY;last_sample_ms=now;return;}
            }else if(phase==0U){
                if(!io.read(scan*4096U,packet,32U)){error();return;}
                if(!Bytes_AllFF(packet,32U)){
                    g=Wire_ReadLE32(packet+4U);first=Wire_ReadLE32(packet+8U);
                    if(!valid(HEADER_MAGIC)||!g||g>=0xFFFFFFFEU||!first||first>=0xFFFFFFFEU||
                       Wire_ReadLE32(packet+12U)||Wire_ReadLE32(packet+16U)||Wire_ReadLE32(packet+20U)||Wire_ReadLE32(packet+24U)){
                        ++view.corrupt;++dirty_headers;
                    }else if(g==generation || g==second_generation){error();return;}
                    else if(g>generation){second_generation=generation;second_sector=sector;generation=g;sector=scan;first_sequence=first;}
                    else if(g>second_generation){second_generation=g;second_sector=scan;}
                }
                ++scan;view.scanned=scan;
                if(scan==SECTORS){
                    if(!generation){
                        if(dirty_headers){error();return;}
                        /* 不在扫描中写；首个一分钟样本才创建扇区头。 */
                        phase=3U;scan=0U;continue;
                    }
                    phase=second_generation?1U:2U;scan=0U;
                }
            }else{
                destination=phase==1U?second_sector:sector;
                if(!read_record(destination,scan)){error();return;}
                if(!Bytes_AllFF(packet,32U)){
                    if(phase==2U){next_slot=scan+1U;}
                    if(!record_valid()){++view.corrupt;}
                    else{
                        g=Wire_ReadLE32(packet+4U);
                        if(g<=view.sequence || (phase==2U && g<first_sequence)){error();return;}
                        remember();
                    }
                }
                if(++scan==SLOTS){
                    if(phase==1U){phase=2U;scan=0U;}
                    else{
                        if(first_sequence>view.sequence+1U){error();return;}
                        view.state=HISTORY_READY;last_sample_ms=now;return;
                    }
                }
            }
        }
        return;
    }
    if((uint32_t)(now-last_sample_ms)<60000U || !version || temp < -400 || temp>1250 || !raw || raw>=4095U){return;}
    if(view.sequence>=0xFFFFFFFDU){error();return;}
    if(!generation){if(!new_sector(0U,0)){error();return;}}
    else if(next_slot==SLOTS){
        destination=(sector+1U)%SECTORS;
        if(!new_sector(destination,1)){error();return;}
    }
    memset(packet,0,28U);Wire_WriteLE32(packet,RECORD_MAGIC);
    Wire_WriteLE32(packet+4U,view.sequence+1U);Wire_WriteLE32(packet+8U,version);
    Wire_WriteLE32(packet+12U,now);Wire_WriteLE32(packet+16U,(uint32_t)temp);Wire_WriteLE32(packet+20U,raw);
    if(!commit_packet(sector*4096U+32U+next_slot*32U)){error();return;}
    ++next_slot;remember();last_sample_ms=now;
}
