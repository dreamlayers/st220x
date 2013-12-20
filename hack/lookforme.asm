    CPU 65c02
    OUTPUT HEX
    INCLUDE spec
    * = (PATCH_AT&$3FFF)+$4000

;Look for this bit to patch.
    LDA #$5A
    STA $325
    LDA #1
    STA $326
    LDA #5
    STA $327
    PLA
    RTS
