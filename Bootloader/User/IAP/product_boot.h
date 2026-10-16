#ifndef PRODUCT_BOOT_H
#define PRODUCT_BOOT_H
void Product_EarlyInit(void);
void Product_Main(void);
#ifdef W25_PRODUCT_BINDING
/* 与原Progress相同的调用时点；Service不得重入W25/FSM。 */
void Product_StorageProgress(void);
#endif
#ifdef PROTOCOL_PRODUCT_BINDING
#include "router.h"
/* 固定产品控制连接：不向Protocol暴露FSM/介质私有状态。 */
int Product_ControlEnter(void);
const char *Product_ControlQuery(Command command);
void Product_ControlClosed(int success);
#ifdef IAP_PHASE8_ENABLED
const char *Product_ControlNextText(void);
#endif
#endif
#endif
