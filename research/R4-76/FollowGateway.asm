option casemap:none
EXTERN R76ObserveFollow:PROC
.code
R76FollowGateway PROC FRAME
    sub rsp,38h
    .allocstack 38h
    .endprolog
    movzx eax,bpl
    mov dword ptr [rsp+20h],eax
    movzx r9d,r14b
    mov r8,rsi
    mov rdx,rdi
    call R76ObserveFollow
    add rsp,38h
    ret
R76FollowGateway ENDP
END
