    CPU 65c02
    OUTPUT HEX
    INCLUDE spec
    * = (PATCH_AT&$3FFF)+$4000

;This gets patched into the existing usb routines.
    jmp (EMPTY_AT&$3FFF)+$4000
    nop
    nop
