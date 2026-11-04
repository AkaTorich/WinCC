; returns a*b + c
muladd:
    mov rax, rcx
    imul rax, rdx
    add rax, r8
    ret
