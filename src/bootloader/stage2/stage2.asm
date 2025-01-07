;interrtupts we use are AH
;STACK GROWS DOWNWARDS!!
org 0x0
bits 16

%define ENDL 0x0D, 0x0A
start:

    ;print
    mov si, hello
    call puts

    cli
    hlt


.halt:
    cli
    hlt


puts:
    push si
    push ax

.loop:
    lodsb           ;loads next char in al
    or al, al       ; verify if null
    jz .done        ; if null, then done  


    mov ah, 0x0E    ;function 0E of interrupt 10h
    mov bh, 0x00    ;page number
    int 0x10        ;call interrupt
    jmp .loop     ;repeat

.done: 
    pop ax
    pop si
    ret 

hello: db "Hello, World from KERNEL!", ENDL, 0
