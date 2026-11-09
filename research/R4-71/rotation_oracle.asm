option casemap:none
EXTERN r71PointCtor:QWORD
EXTERN r71PointApply:QWORD
.code
R71Rotation PROC FRAME
    push rbx
    .pushreg rbx
    push rdi
    .pushreg rdi
    push rbp
    .pushreg rbp
    push r12
    .pushreg r12
    sub rsp,0a8h
    .allocstack 0a8h
    movaps xmmword ptr [rsp+80h],xmm6
    .savexmm128 xmm6,80h
    movaps xmmword ptr [rsp+90h],xmm7
    .savexmm128 xmm7,90h
    .endprolog
    mov rbx,rcx
    lea rdi,[rdx-3a0h]
    lea rbp,[rdx+88h]
    mov r12d,dword ptr [rdx+48h]
    movss xmm0,dword ptr [rdx+28h]
    movss dword ptr [rsp+58h],xmm0
    movss xmm0,dword ptr [rdx+2ch]
    movss dword ptr [rsp+5ch],xmm0
    movss xmm0,dword ptr [rdx+30h]
    movss dword ptr [rsp+60h],xmm0
    movss xmm0,dword ptr [rdx+34h]
    movss dword ptr [rsp+64h],xmm0
    pxor xmm6,xmm6
    pxor xmm7,xmm7
    include native_rotation_fragment.inc
    movaps xmm6,xmmword ptr [rsp+80h]
    movaps xmm7,xmmword ptr [rsp+90h]
    add rsp,0a8h
    pop r12
    pop rbp
    pop rdi
    pop rbx
    ret
R71Rotation ENDP
END
