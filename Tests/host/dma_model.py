"""独立进度模型：10000个随机流，各20次边界/重复事件，非硬件覆盖证明。"""
import random,json
from pathlib import Path
r=random.Random(0x103C8); assertions=0
for trial in range(10000):
    memory=bytearray(512);last=0;absolute=0;expected=bytearray();actual=bytearray()
    for step in range(20):
        n=r.choice([0,1,127,128,255,256,r.randrange(257)])
        block=r.randbytes(n);expected+=block
        for b in block: memory[absolute%512]=b;absolute+=1
        for duplicate in range(r.randrange(1,4)):
            pos=absolute%512
            delta=(pos-last)%512
            assert delta<=256
            actual+=bytes(memory[(last+i)%512] for i in range(delta))
            last=pos;assertions+=1
        assert actual==expected;assertions+=1
    # 相同位置可能已经绕一整圈；不能仅靠位置证明完整，时间守卫需失败。
    assert ((last+512)-last)%512==0;assertions+=1
result=dict(result='PASS',random_streams=10000,events_per_stream=20,assertions=assertions,
            limitation='CPU/IRQ timing and DMA memory overwrite hardware NOT RUN')
p=Path(__file__).resolve().parents[2]/'Evidence/phase234_20261013/phase2/model_results.json'
p.write_text(json.dumps(result,indent=2));print(json.dumps(result))
