"""Offline C regression: model update flags requiring watchdog clock startup."""
from pathlib import Path
import json
import re
import subprocess
import sys

R = Path(__file__).resolve().parents[1]
O = R / sys.argv[1]
O.mkdir(exist_ok=False)
test = r'''
#include "stm32f1xx_hal.h"
#include "boot_iwdg.h"
#include <stdio.h>
#include <stdlib.h>
RccMock rcc;PwrMock pwr;RtcMock rtc;FlashMock flash_regs;IwdgMock iwdg;
uint32_t irq,clock_hz;
static unsigned mode,started,configured,feeds,measure_calls,post_calls;
static uint32_t tick;
void ObserveKey(uint32_t key){
 if(key==0xCCCCU)started=1;
 if(key==0x5555U){configured=1;iwdg.SR=3;}
 if(key==0xAAAAU){if(!started||iwdg.SR)abort();++feeds;}
}
int Lsi_Measure(LsiMeasurement *p){++measure_calls;p->hz=mode==6?0:39784;return mode!=8;}
uint32_t HAL_GetTick(void){
 if(!((mode==3&&!configured)||(mode==4&&configured)))++tick;
 if(!configured){if(mode!=1&&mode!=3)iwdg.SR=0;}
 else {
  ++post_calls;
  if(started&&mode!=2&&mode!=4&&post_calls>=3)iwdg.SR=0;
  if(mode==5)iwdg.PR=5;
 }
 return tick;
}
int main(int argc,char **argv){int a,b;unsigned count;
 if(argc!=2)return 2;
 mode=(unsigned)atoi(argv[1]);tick=0xfffffff0U;
 flash_regs.OBR=mode==7?0:FLASH_OBR_IWDG_SW;
 iwdg.SR=(mode==1||mode==3)?1:0;
 a=Iwdg_StartMeasured();count=measure_calls;b=Iwdg_StartMeasured();
 if(a!=(mode==0)||b||Iwdg_Running()!=(mode==0))return 3;
 if(feeds!=(mode==0)||measure_calls!=count+((mode==6||mode==8)?1U:0U))return 4;
 if((mode==1||mode==3||mode>=6)&&started)return 5;
 if(mode==0){Iwdg_Progress();if(feeds!=2)return 6;tick+=2001;Iwdg_Progress();if(feeds!=2||Iwdg_Running())return 7;}
 printf("mode=%u PASS started=%u feeds=%u ticks=%u\n",mode,started,feeds,tick);
 return 0;
}
'''
rows = []
for label, path in [('before', R/'Tests/host/fixtures/iwdg_before_start_fix.c'),
                    ('after', R/'Bootloader/User/BSP/iwdg.c')]:
    d = O / label
    d.mkdir()
    original = path.read_text(encoding='utf-8')
    # Instrument only KR writes; all branches/timeouts execute the real C owner.
    source = re.sub(r'IWDG->KR=([^;]+);', r'IWDG->KR=\1;ObserveKey(\1);', original)
    (d/'source.c').write_text(source, encoding='utf-8')
    (d/'observe.h').write_text('#include <stdint.h>\nvoid ObserveKey(uint32_t key);\n')
    (d/'test.c').write_text(test)
    command = ['gcc','-O3','-std=c99','-Wall','-Wextra','-Werror','-DIAP_PHASE8_ENABLED',
               '-include',str(d/'observe.h')]
    command += ['-I'+str(R/p) for p in ['Tests/host/lsi_stubs','Bootloader/User/BSP','Shared']]
    command += [str(d/'source.c'),str(d/'test.c'),str(R/'Shared/watchdog_contract.c'),'-o',str(d/'test.exe')]
    (d/'command.json').write_text(json.dumps(command,indent=2))
    p = subprocess.run(command,capture_output=True)
    (d/'build.log').write_bytes(p.stdout+p.stderr)
    p.check_returncode()
    for mode in (range(1) if label=='before' else range(9)):
        p = subprocess.run([str(d/'test.exe'),str(mode)],capture_output=True)
        (d/f'mode{mode}.log').write_bytes(p.stdout+p.stderr)
        assert p.returncode == (3 if label=='before' else 0), (label, mode, p.returncode)
        rows.append(dict(source=label,mode=mode,exit_code=p.returncode,
                         result='EXPECTED_FAIL' if label=='before' else 'PASS'))
(O/'results.json').write_text(json.dumps(rows,indent=2))
print('Before normal-start regression reproduced; after 9 modes PASS; no hardware IO')
