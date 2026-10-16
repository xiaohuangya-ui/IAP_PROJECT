#include "boot_entry.h"
#include "product_boot.h"
#if !defined(PHASE8_TEST_ONLY) || !defined(IAP_PHASE8_ENABLED)
#error Phase8 isolated target required
#endif
void Boot_EarlyInit(void){Product_EarlyInit();}
__declspec(noreturn) void BootMain(void){Product_Main();for(;;){}}
