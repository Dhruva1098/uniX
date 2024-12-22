;interrtupts we use are AH
;STACK GROWS DOWNWARDS!!
org 0x7C00
bits 16

%define ENDL 0x0D, 0x0A
start:
    jmp main   


;prints "Hello, World!" to the screen
;Params: 
;   -ds:si points to string
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

main:
    
    mov ax, 0
    mov ds, ax
    mov es, ax  ;setting up data segments, also cannto write in es,ds directly 
    
    ;setup stack
    mov ss, ax
    mov sp, 0x7C00 ;stack starts at 0x7C00
    
    ;print
    mov si, hello
    call puts

    hlt


.halt:
    jmp .halt

hello:db 'Hello world!', ENDL, 0
times 510-($-$$) db 0
dw 0AA55h