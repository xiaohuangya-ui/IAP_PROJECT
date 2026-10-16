#if !defined(PHASE3_TEST_ONLY)
#error Phase3 fixture only
#endif
#include "boot_protocol.h"
/* 真实维护命令入口；ENTER/LOG仍明确未就绪，不伪造持久状态。 */
void Phase3_Main(void){BootProtocol_Run();for(;;){}}
