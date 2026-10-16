"""冻结本轮Router旧oracle，实际编译generic/Boot/App并比较完整轨迹。"""
from pathlib import Path
import subprocess,sys,zipfile,json
R=Path(__file__).resolve().parents[1];O=R/sys.argv[1];O.mkdir(parents=True,exist_ok=False)
old=(R/'Tests/host/fixtures/router_before_role_specialization.c').read_text(encoding='utf-8')
symbols=['Router_Init','Router_Feed','Router_AsciiFence']
for s in symbols:old=old.replace(s,'Old_'+s)
(O/'old.c').write_text(old,encoding='utf-8')
(O/'test.c').write_text(r'''
#include "router.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
void Old_Router_Init(Router*,int,void*,CommandHandler,BinaryHandler);
void Old_Router_Feed(Router*,uint8_t);void Old_Router_AsciiFence(Router*);
typedef struct {unsigned calls,binary;Command last;unsigned digest,accept;} Trace;
static int command(void*p,Command c){Trace*t=p;t->calls++;t->last=c;t->digest=t->digest*33+c;return t->accept;}
static void binary(void*p,uint8_t c){Trace*t=p;t->binary++;t->digest=t->digest*33+c;}
#define C(x) do{if(!(x)){fprintf(stderr,"line %d\n",__LINE__);exit(1);}}while(0)
int main(void){Router a,b;Trace x,y;unsigned boot,k,j,q=103;uint8_t ch;
const char *lines[]={"@APP:VERSION\r\n","@APP:TEMP\r\n","@APP:UPDATE\r\n","@IAP:VER\r\n","@IAP:STATUS\r\n","@IAP:ENTER\r\n","@IAP:LOG\r\n","@IAP:UNKNOWN\r\n","\r\n","x\rX\n","x\001\r\n"};
for(boot=0;boot<2;boot++){
 for(k=0;k<10000;k++){
 memset(&x,0,sizeof(x));x.accept=k&1;y=x;Router_Init(&a,boot,&x,command,binary);Old_Router_Init(&b,boot,&y,command,binary);
 for(j=0;j<512;j++){
 q=q*1664525+1013904223;ch=(uint8_t)(q>>24);
 if(k<22){const char*s=lines[k%11];ch=(uint8_t)s[j%strlen(s)];}
 if(k==22)ch='A';
 if((j==0&&k!=22) || (k>22&&j%97==0)){Router_AsciiFence(&a);Old_Router_AsciiFence(&b);}
 Router_Feed(&a,ch);Old_Router_Feed(&b,ch);
 if(ROLE==0||boot==(ROLE==1)){
 C(!memcmp(&x,&y,sizeof(x)));C(a.used==b.used&&a.drop==b.drop&&a.cr==b.cr&&a.boot==b.boot&&a.mode==b.mode);C(!memcmp(a.line,b.line,sizeof(a.line)));
 }else{C(x.calls==0&&x.binary==0&&a.mode==ROUTE_ASCII);}
 }
 }
}
puts("PASS 10240000 byte steps; commands/errors/fences/mode transitions; wrong-role fail closed");return 0;}
''')
results=[]
for role in range(3):
    cmd=['gcc','-O3','-std=c99','-Wall','-Wextra','-Werror','-DROLE='+str(role),'-DROUTER_ROLE='+str(role),'-I'+str(R/'Shared'),str(O/'test.c'),str(O/'old.c'),str(R/'Shared/router.c'),'-o',str(O/f'role{role}.exe')]
    p=subprocess.run(cmd,capture_output=True,text=True);(O/f'build{role}.log').write_text(p.stdout+p.stderr);p.check_returncode()
    p=subprocess.run([str(O/f'role{role}.exe')],capture_output=True,text=True);(O/f'run{role}.log').write_text(p.stdout+p.stderr);p.check_returncode()
    results.append(dict(role=role,command=cmd,result='PASS',output=p.stdout))
(O/'results.json').write_text(json.dumps(results,indent=2));print(json.dumps(results))
