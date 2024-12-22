;interrtupts we use are AH
;STACK GROWS DOWNWARDS!!
org 0x7C00
bits 16

%define ENDL 0x0D, 0x0A

; FAT12 headers, refer to "https://wiki.osdev.org/FAT" for more info
jmp short start
nop

bdb_oem:                    db 'MSWIN4.1'    ;8bytes
bdb_bytes_per_sector:       dw 512     
bdb_sectors_per_cluster:    db 1
bdb_reserved_sectors:       dw 1
bdb_fat_count:              db 2
bdb_dir_entries_count:      dw 0E0h
bdb_total_sectors:          dw 2880    ; 1.44MB 
bdb_media_descriptor:       db 0F0h
bdb_sectors_per_fat:       dw 9
bdb_sectors_per_track:     dw 18
bdb_heads:                 dw 2
bdb_hidden_sectors:        dd 0
bdb_total_sectors_big:     dd 0


; extended boot
ebr_drive_number:          db 0
                           db 0   ;reserved
ebr_signature:             db 29h
ebr_volume_id:             dd 0x12345678
ebr_volume_label:          db 'MYOS       '
ebr_file_system:           db 'FAT12   '   

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
    
    ;read something from disk
    mov [ebr_drive_number], dl
    mov ax, 1
    mov cl, 1
    mov bx, 0x7E00
    call disk_read

    ;print
    mov si, hello
    call puts

    cli 
    hlt


.halt:
    cli
    hlt

;handling floppy errors

floppy_error:
    mov si, msg_read_fail
    call puts
    jmp wait_key_and_reboot
    
wait_key_and_reboot:
    mov ah, 0
    int 16h
    jmp 0ffffH:0

;function for lba to chs conversion
lba_to_chs:

    push ax
    push dx
    xor dx, dx
    div word [bdb_sectors_per_track]  ;dx = lba % sectors_per_track

    inc dx
    mov cx, dx  ;cx = dx + 1; cx = head
    
    xor dx, dx
    div word [bdb_heads]  ;dx = lba % heads

    mov dh, dl
    mov ch, al  ;ch = cylinder
    shl ah, 6
    or cl, ah   ;cl = sector

    pop ax
    mov dl, al
    pop ax
    ret

;read sectors from disk
disk_read:

    push ax
    push bx
    push cx
    push dx
    push di

    push cx
    call lba_to_chs
    pop ax

    mov ah, 02h
    mov di, 3

disk_reset:
    pusha
    mov ah, 0
    stc
    int 13h
    jc floppy_error
    popa
    ret
    
.retry:
    pusha
    stc
    int 13h
    jnc .done

    ;read failed
    popa
    call disk_reset

    dec di
    test di, di
    jnz .retry

.fail:
    jmp floppy_error


.done: 
    popa
    pop di
    pop dx
    pop cx
    pop bx
    pop ax ;restore modified registers



hello:db 'Hello world!', ENDL, 0
msg_read_fail:db 'Read from the message disk failed', ENDL, 0

times 510-($-$$) db 0
dw 0AA55h