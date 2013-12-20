;    Include file for registers in chip in Mercury ME-DPF24MG photo frame.
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

; This is a partial list based on what seems to make sense
; considering the firmware code and Sitronix datasheets.

; GPIO registers

PA=$00
PB=$01
PC=$02
PD=$03

PSC=$06

PCA=$08
PCB=$09
PCC=$0A
PCD=$0B

PFB=$0D
PFC=$0E
PFD=$0F

; Bank switching registers as in ST2205 datasheet, without BRR at $36

IRRL=$30 ; $4000-$7FFF for interrupt vectors and during interrupt processing
IRRH=$31
PRRL=$32 ; $4000-$7FFF
PRRH=$33
DRRL=$34 ; $8000-$FFFF
DRRH=$35

; DMA registers, as in ST2202 datasheet, but with extra functionality

DMSL=$58 ; DMA source
DMSH=$59
DMDL=$5A ; DMA destination
DMDH=$5B
DCNTL=$5C ; Number of bytes to copy - 1. DCNTL write runs DMA.
DCNTH=$5D
DCNTH_DMAM=$10
DMRL=$5E ; Memory bank replacing DRR for DMA source
DMRH=$5F ; DMA destination uses DRR, not DMR

; USB registers, as in ST2205 datasheet

USBCON=$70
USBIEN=$71
USBIRQ=$72
USBBFS=$73
USBBFS_BKO=4
EP0CON=$74
BKCON=$76

; *** Other defines relating to chip ***

BKO_BUF=$200 ; Buffer for USB data from host
BKO_BUF_SIZE=64 ; Size of USB buffer and packets from host
CODE_BASE=$4000 ; Location where firmware pages are mapped
PRR_PAGE_MASK=$3FFF
