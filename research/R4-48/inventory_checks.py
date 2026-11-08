"""Owned instruction controls for the conservative static inventory tools."""
import struct
from native_access_inventory import potential_fields
import capstone
from sprite_alias_inventory import analyze_method,sprite_instance_export
from quad_copy_patterns import copy_pattern
from gd_sprite_consumers import selectors


def decode(code):
    engine=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_64);engine.detail=True
    instructions=list(engine.disasm(code,0x1000))
    assert sum(i.size for i in instructions)==len(code)
    return instructions


def main():
    for suffix in ('QEAAXXZ','UEBA_NXZ','QEAA?AU_ccV3F_C4B_T2F_Quad@2@XZ','IEBAPEAVCCSprite@2@XZ'):
        assert sprite_instance_export('?method@CCSprite@cocos2d@@'+suffix)
    for name in ('?create@CCSprite@cocos2d@@SAPEAV12@XZ','?bad@Other@cocos2d@@QEAAXXZ','?bad@CCSprite@cocos2d@@QEAA', '?bad@CCSprite@cocos2d@@SAHXZ'):
        assert not sprite_instance_export(name)
    instructions=decode(bytes.fromhex('4889cf4881c7cc010000488b07c3'))
    access,_,status=analyze_method(instructions,0x1000)
    assert status['converged'] and len(access)==1 and access[0]['offset']=='0x1cc'
    instructions=decode(bytes.fromhex('4889cfe800100000488b87cc010000488b81cc010000c3'))
    access,_,_=analyze_method(instructions,0x1000)
    assert len(access)==1 and 'rdi' in access[0]['text'], 'volatile RCX must be killed across an unknown call'
    instructions=decode(bytes.fromhex('85c074054889cfeb034889d7488b87cc010000c3'))
    access,_,_=analyze_method(instructions,0x1000)
    assert not access, 'a branch with an unknown pointer must prevent a must-alias claim'
    instructions=decode(bytes.fromhex('4883ec38488d81cc0100004889442428e8001000004883c438c3'))
    access,escapes,_=analyze_method(instructions,0x1000)
    assert access[0]['access']=='address_only'
    assert any(e.get('aliases',{}).get('potential_stack_arg6')=='0x1cc' for e in escapes)
    lea=decode(bytes.fromhex('488d81cc010000'))[0]
    assert list(potential_fields(lea))[0]['access']=='address_only_not_memory_read'
    program=bytearray()
    first_end=0
    for block in range(6):
        program+=b'\x0f\x10\x81'+struct.pack('<i',0x1cc+block*16)
        program+=b'\x0f\x11\x02' if block==0 else b'\x0f\x11\x42'+bytes([block*16])
        if block==0:first_end=len(program)
    program+=b'\xc3'
    assert copy_pattern(decode(program),0)
    assert not copy_pattern(decode(program[:first_end]+b'\x48\x83\xc2\x10'+program[first_end:]),0)
    assert not copy_pattern(decode(program[:first_end]+b'\x31\xc9'+program[first_end:]),0)
    selected=selectors('''class Root : cocos2d::CCSprite {
    void a() = win 0x100;
    static void b() = win 0x200;
    bool c() = win 0x300;
}
class Child : Root {
    void d(int x) = win 0x400;
}
class Other : cocos2d::CCNode, Root {
    void e() = win 0x500;
}
''')
    assert [(s['class_name'],s['method']) for s in selected]==[('Root','a'),('Child','d')]
    print('static inventory controls passed; no native reader closure or FPS claim')


if __name__=='__main__':main()
