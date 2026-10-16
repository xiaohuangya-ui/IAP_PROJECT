#ifndef BOOT_IWDG_H
#define BOOT_IWDG_H
#include "lsi_measure.h"
/* Boot唯一启动owner，Phase8编译；不可软件停止，不修改OptionBytes。
 * StartMeasured只有本次Lsi_Measure成功才执行。Progress仅在验证的事务进展调用，
 * 2s以上无进展锁停；主循环/WIP轮询禁止刷新。 */
int Iwdg_StartMeasured(void);
void Iwdg_Progress(void);
int Iwdg_Running(void);
const LsiMeasurement *Iwdg_Measurement(void);
#endif
