option casemap:none
EXTERN R76FollowGateway:PROC
.code
R76CallCount PROC FRAME
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
    mov rdi,rdx
    mov rsi,r8
    mov r14d,r9d
    and r14d,1
    mov ebp,r9d
    shr ebp,1
    and ebp,1
    call R76FollowGateway
    add rsp,28h
    pop r14
    pop rbp
    pop rsi
    pop rdi
    ret
R76CallCount ENDP
END
