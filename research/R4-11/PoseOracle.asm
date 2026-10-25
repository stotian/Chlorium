; Fixture wrapper only. The native block is read from the user's local binary,
; fingerprinted and relocated into a separate RX page by the test.
.code
ChloriumPoseOracle proc
    sub rsp,100h
    movdqu [rsp+30h],xmm6
    movdqu [rsp+40h],xmm7
    movdqu [rsp+50h],xmm8
    movdqu [rsp+60h],xmm9
    movdqu [rsp+70h],xmm10
    movdqu [rsp+80h],xmm11
    movdqu [rsp+90h],xmm12
    movdqu [rsp+0a0h],xmm13
    movdqu [rsp+0b0h],xmm14
    movdqu [rsp+0c0h],xmm15
    mov [rsp+0f0h],rcx
    movups xmm11,[rdx]
    xorps xmm10,xmm10
    xorps xmm15,xmm15
    mov dword ptr [rsp],0
    mov eax,80000000h
    movd xmm12,eax
    mov eax,42652ee1h
    movd xmm13,eax
    ; Entry of the copied mid-function block needs 16-byte stack alignment;
    ; its CRT calls then see the actual native alignment and shadow space.
    call r8
    mov rcx,[rsp+0f0h]
    movss dword ptr [rcx],xmm8
    movss dword ptr [rcx+4],xmm7
    movss dword ptr [rcx+8],xmm6
    movss dword ptr [rcx+12],xmm1
    movdqu xmm6,[rsp+30h]
    movdqu xmm7,[rsp+40h]
    movdqu xmm8,[rsp+50h]
    movdqu xmm9,[rsp+60h]
    movdqu xmm10,[rsp+70h]
    movdqu xmm11,[rsp+80h]
    movdqu xmm12,[rsp+90h]
    movdqu xmm13,[rsp+0a0h]
    movdqu xmm14,[rsp+0b0h]
    movdqu xmm15,[rsp+0c0h]
    mov rax,rcx
    add rsp,100h
    ret
ChloriumPoseOracle endp
end
