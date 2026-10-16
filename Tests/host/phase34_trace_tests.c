#include "usart.h"
#include "uart_port.h"
#include "phase34_ram_trace.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
uint32_t mock_primask,mock_ipsr,mock_basepri,mock_faultmask,mock_tc,mock_rxne;
uint32_t SystemCoreClock=72000000U;
static uint32_t tick,errors;static int start=1,read=1,submit=1;static unsigned calls;
static UartStats stats;
static uint32_t slot(unsigned i){uint32_t v;memcpy(&v,(const uint8_t *)(const void *)&g_phase34_ram+4U*i,4);return v;}
int Trace_Uart_Start(void);void Trace_Uart_Service(void);int Trace_Uart_ReadByte(uint8_t *);
int Trace_Uart_TxSubmit(const uint8_t *,uint16_t);uint32_t Trace_Uart_TakeErrors(void);
void Trace_BootProtocol_Init(void);void Trace_BootProtocol_Poll(void);
uint32_t HAL_GetTick(void){return tick;}
void Uart_GetStats(UartStats *s){*s=stats;}
int Mock_Uart_Start(void){calls++;return start;}
void Mock_Uart_Service(void){calls++;}
int Mock_Uart_ReadByte(uint8_t *b){calls++;*b=42;return read;}
int Mock_Uart_TxSubmit(const uint8_t *p,uint16_t n){calls++;if(!p || n!=7U){exit(2);}return submit;}
uint32_t Mock_Uart_TakeErrors(void){calls++;return errors;}
void Mock_BootProtocol_Init(void){calls++;}
void Mock_BootProtocol_Poll(void){calls++;}
#define C(x) do{if(!(x)){fprintf(stderr,"line %d %s\n",__LINE__,#x);return 1;}}while(0)
int main(void)
{
    uint8_t b,data[7]={0};unsigned i;
    C(Trace_Uart_Start()==1);C(slot(0)==0x5034524DU && slot(2)%2U==0U);
    C(slot(6)==1 && slot(7)==1);
    C(Trace_Uart_ReadByte(&b)==1 && b==42);read=0;C(Trace_Uart_ReadByte(&b)==0);
    C(Trace_Uart_TxSubmit(data,7)==1);submit=0;C(Trace_Uart_TxSubmit(data,7)==0);
    errors=UART_ERR_FE;C(Trace_Uart_TakeErrors()==UART_ERR_FE);
    Trace_BootProtocol_Init();Trace_BootProtocol_Poll();stats.received_bytes=11;stats.transmitted_bytes=7;
    stats.total_errors=1;stats.rx_ready=1;tick=10;Trace_Uart_Service();
    C(slot(12)==11 && slot(13)==1 && slot(14)==7 && slot(15)==7);
    C(slot(18)==UART_ERR_FE && slot(4)==1 && slot(5)==1);
    start=0;C(Trace_Uart_Start()==0 && slot(7)==0);
    mock_primask=1;tick=20;Trace_Uart_Service();C(slot(8)==2 && slot(11)==0);
    C(calls==11);for(i=0;i<24;i++){printf("%lu%s",(unsigned long)slot(i),i==23?"\n":",");}
    mock_primask=0;tick=0;
    for(i=0;i<4094;i++){Trace_Uart_Service();}
    C(slot(5)==4096 && slot(9)==0 && slot(11)==1 && slot(2)%2U==0U);
    return 0;
}
