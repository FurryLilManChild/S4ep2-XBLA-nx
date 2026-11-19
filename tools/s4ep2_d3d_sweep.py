# S4ep2-XBLA-nx: every Direct3D function the game can reach (bl, b, code and data pointers, .pdata excluded) through
# unhooked functions, and those with no Episode I pair. Run from /home/claude (paths below).
import re,glob,bisect,struct,subprocess,collections
B=0x82000000
d=open('s4ep2-xbla-nx/out/default_image.bin','rb').read()
pairs=[l.split() for l in open('s4ep2-xbla-nx/out/align_pairs.txt')]
paired={int(p[1][4:],16):p[0] for p in pairs}
LO,HI=0x823EC100,0x82413800
fs=set()
for p in glob.glob('s4ep2-xbla-nx/app/generated/renut_recomp.*.cpp'):
    for m in re.finditer(r'^DEFINE_REX_FUNC\(sub_([0-9A-F]{8})\)',open(p).read(),re.M): fs.add(int(m.group(1),16))
fs=sorted(fs)
def owner(a): return fs[bisect.bisect_right(fs,a)-1]
src=subprocess.run("grep -rhoE '(BANJO_RENUT_HOOK(_RAW)?|REX_HOOK(_RAW)?)\\(sub_[0-9A-F]{8}' s4ep2-xbla-nx/app/src | grep -oE '[0-9A-F]{8}'",shell=True,capture_output=True,text=True).stdout.split()
hooked={int(x,16) for x in src}
# call graph (bl and b to function starts, plus lis/addi pointers)
calls=collections.defaultdict(set); fset=set(fs)
for off in range(0x130000,0x8E0000,4):
    w=struct.unpack_from('>I',d,off)[0]
    if w>>26==18:
        li=w&0x03FFFFFC
        if li&0x02000000: li-=0x04000000
        t=(B+off+li)&0xFFFFFFFF
        if t in fset: calls[owner(B+off)].add(t)
    # data words that are D3D function addresses (pointer tables)
    if LO<=w<HI and w in fset: calls[owner(B+off) if 0x130000<=off else 0].add(w)
# also data section pointers
for off in range(0,len(d)-4,4):
    if 0x130000<=off<0x8E0000 or 0x10B600<=off<0x12D3D0: continue
    w=struct.unpack_from('>I',d,off)[0]
    if LO<=w<HI and w in fset: calls['data'].add(w)
roots=[f for f in fs if not (LO<=f<HI) and f not in hooked]+['data']
seen=set(); stack=list(roots); via={}
while stack:
    f=stack.pop()
    for t in calls.get(f,()):
        if t in seen: continue
        if LO<=t<HI:
            seen.add(t); via[t]=f
            if t not in hooked: stack.append(t)
reach=[t for t in sorted(seen)]
unp=[t for t in reach if t not in paired and t not in hooked]
print('D3D functions reachable from game code:',len(reach),'hooked:',sum(t in hooked for t in reach),'unpaired and unhooked:',len(unp))
for t in unp:
    v=via[t]; print('sub_%08X'%t,'reached from',v if isinstance(v,str) else 'sub_%08X'%v, 'size',(fs[bisect.bisect_right(fs,t)]-t)//4)
