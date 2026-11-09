option casemap:none
EXTERN R72Observe:PROC
.code
R72PointGateway PROC FRAME
    lea r10,[rsp+8]
    sub rsp,48h
    .allocstack 48h
    .endprolog
    mov r9,rbx
    mov qword ptr [rsp+20h],r10
    movzx eax,r12b
    mov dword ptr [rsp+28h],eax
    mov qword ptr [rsp+30h],rdi
    movss dword ptr [rsp+38h],xmm6
    call R72Observe
    add rsp,48h
    ret
R72PointGateway ENDP
END
