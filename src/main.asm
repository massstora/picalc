default rel

global main
extern picalc_run

section .text
main:
    push rbp
    mov rbp, rsp

    ; System V AMD64 already passes argc in edi and argv in rsi.
    call picalc_run

    pop rbp
    ret

section .note.GNU-stack noalloc noexec nowrite progbits
