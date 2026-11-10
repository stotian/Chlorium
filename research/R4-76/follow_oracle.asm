option casemap:none
.code
R76Follow PROC FRAME
    push rdi
    .pushreg rdi
    push rsi
    .pushreg rsi
    push rbp
    .pushreg rbp
    push r14
    .pushreg r14
    sub rsp,28h
    .allocstack 28h
    .endprolog
    mov rdi,r8
    mov r14d,dword ptr [rdx+48h]
    mov ebp,r14d
    and r14d,1
    shr ebp,1
    and ebp,1
    lea rsi,[rdx-38h]
    mov rdx,rcx
    include native_follow_fragment.inc
    add rsp,28h
    pop r14
    pop rbp
    pop rsi
    pop rdi
    ret
R76Follow ENDP
END
