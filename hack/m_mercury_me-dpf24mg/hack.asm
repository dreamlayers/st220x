;    Routines to allow direct USB-to-LCD-transfers in an ST220x-device
;    Copyright (C) 2008 Jeroen Domburg <jeroen@spritesmods.com>
;    Copyright (C) 2013 Boris Gjenero <boris.gjenero@gmail.com>
;
;    This program is free software: you can redistribute it and/or modify
;    it under the terms of the GNU General Public License as published by
;    the Free Software Foundation, either version 3 of the License, or
;    (at your option) any later version.
;
;    This program is distributed in the hope that it will be useful,
;    but WITHOUT ANY WARRANTY; without even the implied warranty of
;    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;    GNU General Public License for more details.
;
;    You should have received a copy of the GNU General Public License
;    along with this program.  If not, see <http://www.gnu.org/licenses/>.


    CPU 65c02
    OUTPUT HEX

; Set MEMHACK to 1 to build RAM code and 0 to build flash code.
; Make the line just "MEMHACK=0" and let the build system build both.
MEMHACK=0

    INCLUDE spec
    INCLUDE sitronix.inc

; *** Variables in RAM ***

LCD_AWAKE=FREERAM+0

; *** Commands understood by code here ***

COMMAND_BASE=$10 ; Commands start from this number
CMD_SETWIN=COMMAND_BASE+0 ; Set window in LCD controller
WBASE=BKO_BUF+1 ; Location of window data
CMD_BLON=COMMAND_BASE+1
CMD_BLOFF=COMMAND_BASE+2
CMD_LCDWAKE=COMMAND_BASE+3
CMD_LCDSLEEP=COMMAND_BASE+4
BYTECNT_BASE=$C0 ; $C0 to $FE transfers 1 to 63 bytes to the LCD controller

; *** Entry point ***

IF MEMHACK != 0
    *=FREERAM
ELSE
; EMPTY_AT is the file and flash offset. Here * needs to be set to the memory
; address where the location will be mapped during exectuion. It should
; match the jmp destination in hack_jmp.asm.
    *=(EMPTY_AT&PRR_PAGE_MASK)+CODE_BASE
; First parameter HACK to activate hack
    lda COMMAND_BUF+1
    cmp #'H'
    bne nohack
    lda #0
    sta COMMAND_BUF+1 ; Probably pointless
    lda COMMAND_BUF+2
    cmp #'A'
    bne nohack
    lda COMMAND_BUF+3
    cmp #'C'
    bne nohack
    lda COMMAND_BUF+4
    cmp #'K'
    bne nohack

; Second parameter CODE for code uploading
    lda COMMAND_BUF+5
    cmp #'C'
    bne nohackcode
    lda COMMAND_BUF+6
    cmp #'O'
    bne nohackcode
    lda COMMAND_BUF+7
    cmp #'D'
    bne nohackcode
    lda COMMAND_BUF+8
    cmp #'E'
    bne nohackcode

; Jump into BKO buffer to execute code there
    jmp BKO_BUF

nohackcode=*
ENDC

; This allows the USB ISR to take care of USB sends which are needed
; in response to READ 10, for detecting the photo frame.
; WARNING: Variables overwrite this when running from RAM!
    lda #2
    sta USB_SEND_STATE

; Set to 1 so code below can notice when it has been reset to 0
; when a new command has been received.
    lda #1
    sta CMD_PARSED

; LCD is initially awake
    lda #1
    sta LCD_AWAKE

; Push registers
    lda DRRH
    pha
    phx

; First free up BKO from command so that data packets can come.
; The last packet of the command could be used for data, but
; since a loop here takes over USB the advantage would be negligible.

; Could call this function here, but there's no need
;    lda #0
;    sta DATA_VALID
;    jsr $820
;    db 0, 0
;    db ((FINISH_XFER&PRR_PAGE_MASK)+CODE_BASE-1)&$FF
;    db ((FINISH_XFER&PRR_PAGE_MASK)+CODE_BASE-1)>>8

    lda #USBBFS_BKO
    sta USBBFS

; Wait for USB ISR to set flag meaning data is available

    bra wait4xfer

; Exit for when a new command is received
; The command needs to be parsed again, but a return would complete parsing
; of the old command and then execute the new command. So, reset return
; addresses on the stack to cause the new command to be parsed.
; Stack addresses are hard-coded because they should always be the same.
; Return from page 0 command procedure to end of parser.
cmdexit=*
; OF expects LCD to be awake
    lda LCD_AWAKE
    bne nowakelcd

    ldx #lcdwaketab-lcdtab
    jsr lcdseq

; Return from page 0 command procedure to end of parser.
nowakelcd=*
    lda #$A8
    sta $1FA
    lda #$5F
    sta $1FB

; Return from parser to start of command processing
    lda #$C9
    sta $1FC
    lda #$5E
    sta $1FD

; Immediate exit point here
    plx
    pla
    sta DRRH

; This was for returning to initial hack code in flash
;    jmp $7E52
; This is for returning to original firmware after patch location
nohack=*
    lda #$5A
    sta $325
    jmp (PATCH_AT&PRR_PAGE_MASK)+CODE_BASE+3

; TODO: WAI here to reduce power consumption?
wait4xfer=*
    lda USBCON
    and #2
    beq cmdexit ; USB disconnected. Could exitnow but cmdexit probably safer.
    lda CMD_PARSED
    beq cmdexit ; New original firmware command arrived
    lda DATA_VALID
    beq wait4xfer

; BKO interrupt not needed for transfer
    lda USBIEN
    and #$FD
    sta USBIEN

; Select lcd
    lda #$3
    sta $35

; Normally, one cannot assume that DMA registers are preserved while interrupts
; are enabled because USB interrupt code uses DMA without restoring registers.
; Here, the USB interrupt code won't be messing with registers, so
; they can be set up once:
    stz DMDL
    lda #$C0
    sta DMDH

    lda #DCNTH_DMAM|0 ; Destination address is fixed
    sta DCNTH ; Transfer size can always fit in one byte

    lda #(BKO_BUF+1)>>8
    sta DMSH ; Source high should not change because low won't ever roll over

; Unrolled part of loop start, because loop entry needs to skip
; waiting, because DATA_VALID indicates packet has arrived.
    lda #(BKO_BUF+1)&$FF
    sta DMSL
    sec ; for sbc
    bra entry

nextpacket=*
; Optimize code path for uploading data to LCD,
; because performance is most critical there.
; Minimize delay between seeing a packet has arrived and
; starting DMA to copy the data and free BKO buffer for next packet.

    lda #(BKO_BUF+1)&$FF
    sta DMSL
    sec ; for sbc below

; Wait for next packet
waitpacket=*
    lda USBBFS
    and #USBBFS_BKO
    beq waitpacket

; Now the packet data is available in the BKO buffer.
; It should be safe there until a write to bit 3 of USBBFS.
entry=*
    lda BKO_BUF

    sbc #BYTECNT_BASE
    bcc check4cmd ; Nope, not bytes to upload
; Carry is now set for 0x40 subtraction below.

; Upload data to LCD controller, using DMA
    sta DCNTL ; DMA runs here

; DMA complete, so tell hardware that BKO is free for new packet
; Other operations jump here when they're done.
packetdone=*
    lda #USBBFS_BKO
    sta USBBFS

; Subtract 0x40 (USB packet size) from length sent by SCSI command
; sec not needed in upload path, but needed in other command paths
    sec
    lda LEN0
    sbc #BKO_BUF_SIZE
    sta LEN0
    lda LEN1
    sbc #$0
    sta LEN1
    lda LEN2
    sbc #$0
    sta LEN2
; This allows up to 16 megs, so that's more than enough without LEN3.
;    lda LEN3
;    sbc #$0
;    sta LEN3
; When this rolls around, it means LEN was 0 and transfer is done.
; This avoids having separate code which tests if LEN was zero before.
; The first subtraction was done by the USB interrupt handler.
    bcs nextpacket

; This SCSI transfer is finished
    stz LEN0
    stz LEN1
    stz LEN2

; Re-enable BKO interrupt
    lda USBIEN
    ora #2
    sta USBIEN

; Send USBC response
    lda #0
    sta DATA_VALID
    lda #0
    jsr $820
    db 0, 0
    db ((SEND_CSW&PRR_PAGE_MASK)+CODE_BASE-1)&$FF
    db ((SEND_CSW&PRR_PAGE_MASK)+CODE_BASE-1)>>8

    jmp wait4xfer ; try BRA TODO FIXME

; Packet was not a data transfer. Check if it is a command for code here.
; (Not to be confused with commands for the original firmware.)
check4cmd cmp #(CMD_SETWIN-BYTECNT_BASE)&$FF
    beq setaddr
    cmp #(CMD_BLON-BYTECNT_BASE)&$FF
    beq blon
    cmp #(CMD_BLOFF-BYTECNT_BASE)&$FF
    beq bloff
    cmp #(CMD_LCDWAKE-BYTECNT_BASE)&$FF
    beq lcdwake
    cmp #(CMD_LCDSLEEP-BYTECNT_BASE)&$FF
    beq lcdsleep

; Packet had no command, so simply ignore it.
    bra packetdone

; Turn on backlight
blon=*
    lda #PC_SAVED
    ora #$10
    bra blwrite

; Turn off backlight
bloff=*
    lda #PC_SAVED
    and #$EF

blwrite=*
    sta PC_SAVED
    sta PC
    bra packetdone

; Wake LCD from deep sleep
lcdwake=*
    lda LCD_AWAKE
    bne packetdone
    lda #1
    sta LCD_AWAKE
    ldx #lcdwaketab-lcdtab
    bra lcddoseq

; Put LCD in deep sleep
lcdsleep=*
    lda LCD_AWAKE
    beq packetdone
    lda #0
    sta LCD_AWAKE
    ldx #lcdsleeptab-lcdtab

lcddoseq=*
    jsr lcdseq
    jmp packetdone

; LCD window setting function
setaddr=*
    ldx #0 ; X=0 for storing zeros without needing LDA
    stx $8000
    lda #$20 ; y1
    sta $8000

    stx $c000
    lda WBASE+4
    sta $c000

    stx $8000
    lda #$50 ; y1
    sta $8000

    stx $c000
    lda WBASE+4
    sta $c000

    stx $8000
    lda #$51 ; y2
    sta $8000

    stx $c000
    lda WBASE+5
    sta $c000

    stx $8000
    lda #$21 ; x1
    sta $8000

    lda WBASE+0
    sta $c000
    lda WBASE+1
    sta $c000

    stx $8000
    lda #$52 ; x1
    sta $8000

    lda WBASE+0
    sta $c000
    lda WBASE+1
    sta $c000

    stx $8000
    lda #$53 ; x2
    sta $8000

    lda WBASE+2
    sta $c000
    lda WBASE+3
    sta $c000

    stx $8000
    lda #$22 ; data port
    sta $8000

    jmp packetdone

; *** LCD command sequences ***

; LCD sequences are stored starting at lcdtab. This
; code uses those values to set LCD registers.

; These special commands can be used in the sequence:
LCDSEQ_DELAY=$FE ; Call delay routine
LCDSEQ_END=$FF   ; Marker for end of sequence
; There is no data afterwards for these.

lcdseq=*
lcdseqloop=*
    lda lcdtab,x
    inx
; Check for special commands in sequence
    cmp #LCDSEQ_END
    beq lcdseqdone
    cmp #LCDSEQ_DELAY
    beq lcdseqwait

; Send LCD register number
    lda #0          ; High byte is always 0 so no need to load from table
    sta $8000
    lda lcdtab-1,x  ; Load low byte. Note X was incremented earlier.
    sta $8000

; Send LCD register value
    lda lcdtab,x    ; High byte
    inx
    sta $C000
    lda lcdtab,x    ; Low byte
    inx
    sta $C000
    bra lcdseqloop

; Call delay routine
; The application note specifies 50ms and 200ms but
; the firmware calls this routine once or twice.
lcdseqwait=*
    jsr $820
    db 1, 0
    db $FF, $3F
    bra lcdseqloop

lcdseqdone rts

lcdtab=*
; LCD deep sleep sequence, as in ILI9320 application note V0.92
; Image data is not retained in deep sleep.
lcdsleeptab=*
    db $07, $00, $00 ; display off
    db $10, $00, $00 ; power control registers
    db $11, $00, $00
    db $12, $00, $00
    db $13, $00, $00
    db LCDSEQ_DELAY  ; wait for capacitor discharge
    db LCDSEQ_DELAY
    db $10, $00, $02 ; deep sleep
    db LCDSEQ_END

; LCD wake sequence
; LCD deep sleep sequence, based on ILI9320 application note V0.92
; Final register values are from LCD initialization function in firmware.
; Some differ from the application note, probably because the LCD panel
; is different.
lcdwaketab=*
    db $10, $00, $00 ; power control registers
    db $11, $00, $00
    db $12, $00, $00
    db $13, $00, $00
    db LCDSEQ_DELAY  ; wait for capacitor discharge
    db LCDSEQ_DELAY
    db $10, $17, $B0 ; power control registers
    db $11, $00, $37
    db LCDSEQ_DELAY
    db $12, $01, $30
    db LCDSEQ_DELAY
    db $13, $00, $CC
    db $29, $00, $00
    db LCDSEQ_DELAY
    db $07, $01, $73 ; turn on display
    db LCDSEQ_END

IF MEMHACK == 0
; *** Info block seen by libst2205 ***

    db "H","4","C","K"
; Version of info block increased, because 2 bytes are needed for CONF_XRES.
    db 2
    db CONF_XRES>>8
    db CONF_XRES&$FF
    db CONF_YRES
    db CONF_BPP
    db CONF_PROTO
    db OFFX
    db OFFY
ENDC
