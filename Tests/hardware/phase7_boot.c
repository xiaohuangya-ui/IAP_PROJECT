#ifndef PHASE7_TEST_ONLY
#error Isolated Phase7 integration only
#endif
#include "boot_entry.h"
#include "product_boot.h"
void Boot_EarlyInit(void){Product_EarlyInit();}
__declspec(noreturn) void BootMain(void){Product_Main();for(;;){}}
