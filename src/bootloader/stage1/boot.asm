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
    mov ax, 0
    mov ds, ax
    mov es, ax  ;setting up data segments, also cannto write in es,ds directly 
    
    ;setup stack
    mov ss, ax
    mov sp, 0x7C00

    ;some bios start at 0x7C00, so we need to copy it to 0x7E00
    push es
    push word .after
    rettf

.after:

    
    ;read something from disk
    mov [ebr_drive_number], dl

    ;show loading
    mov si, msg_loading
    call puts

    ;read drive params
    push es
    mov ah, 08h
    int 13h
    jc floppy_error
    pop es

    and cl, 0x3F ;masking out the high 2 bits
    xor ch, ch
    mov [bdb_total_sectors_big], cx     ;total sectors

    inc dh
    mov [bdb_heads], dh     ;heads

    ;read FAT root directory 
    mov ax, [bdb_sectors_per_fat]
    mov bl, [bdb_fat_count]
    xor bh, bh
    mul bx
    add ax, [bdb_reserved_sectors]
    push ax

    ;compute size of root directory
    mov ax, [bdb_dir_entries_count]
    shl ax, 5     ;ax = *= 32
    div word [bdb_bytes_per_sector]   ;ax = ax / bytes_per_sector
    xor dx, dx

    test dx, dx   ;if dx is 0, then no need to add 1
    jz .root_dir_size_done
    inc ax

.root_dir_size_done:
    
    ;read root directory
    mov cl, al   ;cl = root directory size
    pop ax     ;ax = root directory start
    mov dl, [ebr_drive_number]  ;drive number
    mov bx, buffer   ;buffer
    call disk_read

    ;search for kernel.bin
    xor bx, bx
    mov di, buffer
    
.search_kernel:
    mov si, file_kernel_bin
    mov cx, 11  ;length of file name
    push di
    repe cmpsb  ;compare si and di, if equal, then repeat. repe repeats until cx = 0 or si != di
    pop di
    je .kernel_found

    add di, 32
    inc bx
    cmp bx, [bdb_dir_entries_count]
    jl .search_kernel

    ;kernel not found
    jmp kernel_not_found_error

.kernel_found:
    ;di should have address to the entyr
    mov ax, [di+0Fh]  ;cluster number
    mov [kernel_cluster], ax

    ;load FAT from disk to mem
    mov ax, [bdb_reserved_sectors]
    mov bx, buffer
    mov cl, [bdb_fat_count]
    mov dl, [ebr_drive_number]
    call disk_read

    ;read kernel and process FAT Chain
    mov bx, KERNEL_LOAD_SEGMENT
    mov es, bx
    mov bx, KERNEL_LOAD_OFFSET

.load_kernel_loop:
    ;read next cluster
    mov ax, [kernel_cluster]
    add ax, 31    ;first cluster = (kernel_cluster - 2) * 32 + root_dir_start
                  ;refactor to not be hardcoded
    mov cl, 1
    mov dl, [ebr_drive_number]
    call disk_read

    add bx, [bdb_bytes_per_sector]

    ;find next cluster
    mov ax, [kernel_cluster]
    mov cx, 3
    mul cx
    mov cx, 2
    div cx

    mov si, buffer
    add si, ax
    mov ax, [ds:si]

    or dx,dx
    jz .even

.odd:
    shr ax, 4
    jmp .next_cluster_after

.even:
    and ax, 0FFFH

.next_cluster_after:
    cmp ax, 0FF8H ;end of file
    jae .read_done

    mov[kernel_cluster], ax
    jmp .load_kernel_loop

.read_done:
    mov dl, [ebr_drive_number] ;jmp to kernel
    ;set segment registers
    mov ax, KERNEL_LOAD_SEGMENT
    mov es, ax
    mov ds, ax

    jmp KERNEL_LOAD_SEGMENT:KERNEL_LOAD_OFFSET

    jmp wait_key_and_reboot


    cli             ;disable interrupts
    hlt


puts:
    push si
    push ax
    push
;handling floppy errors

floppy_error:
    mov si, msg_read_fail
    call puts
    jmp wait_key_and_reboot

kernel_not_found_error:
    mov si, msg_kernel_not_found
    call puts
    jmp wait_key_and_reboot


wait_key_and_reboot:
    mov ah, 0
    int 16h
    jmp 0ffffH:0

;function for lba to chs conversion

.halt:
    cli
    hlt

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


disk_reset:
    pusha
    mov ah, 0
    stc
    int 13h
    jc floppy_error
    popa
    ret


msg_loading: db 'Loading OS...', ENDL, 0
msg_read_fail: db 'Read from the message disk failed', ENDL, 0
msg_kernel_not_found: db 'Kernel bin is  !found', ENDL, 0
file_kernel_bin: db 'KERNEL  BIN', 0
kernel_cluster: dw 0
KERNEL_LOAD_SEGMENT equ 0x2000
KERNEL_LOAD_OFFSET equ 0x0000


times 510-($-$$) db 0
dw 0AA55h

buffer: