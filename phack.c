/*
    Tool to interface with ST2205U-based picture frames
    Copyright (C) 2008 Jeroen Domburg <jeroen@spritesmods.com>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifdef __linux
#include <asm/fcntl.h>
#else
#include <fcntl.h>
#endif
#include <dirent.h>
#include <sys/mman.h>

#define USB_PACKET_SIZE 64
#define SCSI_SECTOR_SIZE 512
#define DRR_PAGE_SIZE 0x8000
#define FIRMWARE_SIZE (DRR_PAGE_SIZE*2)
#define RAM_SIZE 0x880
#define FREE_RAM_ADDR 0x580
#define FREE_RAM_SIZE 0x200

/*** Global variables ***/

/* I don't think these are a bad thing. This tool only works with one photo frame
 * at a time and passing arround a pointer to a photoframe struct or allocating
 * these repeately seems worse.
 */

unsigned char *buff; /* Main buffer used for data */
unsigned char *cmdbuf; /* Small buffer used for commands */

/*
Two routines to allocate/deallocate page-aligned memory, for use with the
O_DIRECT-opened files.
*/

void *malloc_aligned(long size) {
    int f;
    char *buff;
    f=open("/dev/zero",O_RDONLY);
    buff=mmap(0,size,PROT_READ|PROT_WRITE,MAP_PRIVATE,f,0);
    close(f);
    if (buff == MAP_FAILED) {
        printf("FATAL ERROR: Failed mmap in malloc_alligned.\n");
        exit(-1);
    }
    return buff;
}

void free_aligned(void *addr, long size) {
    munmap(addr,size);
}


/*
Checks if the device is a photo frame by reading the first 512 bytes and
comparing against the known string that's there
*/
int is_photoframe(int f) {
    int y,res;
    char id[]="SITRONIX CORP.";
    char *buff;
    buff=malloc_aligned(0x200);
    lseek(f,0x0,SEEK_SET);
    y=read(f,buff,0x200);
    buff[15]=0;
//    fprintf(stderr,"ID=%s\n",buff);
    res=strcmp(buff,id)==0?1:0;
    free_aligned(buff,0x200);
    return res;
}

/*
The interface works by writing bytes to the raw 'disk' at certain positions.
Commands go to offset 0x6200, data to be read from the device comes from 0xB000,
data to be written goes to 0x6600. Hacked firmware has an extra address,
0x4200: bytes written there will go straight to the LCD.
*/

#define POS_CMD 0x6200
#define POS_WDAT 0x6600
#define POS_RDAT 0xb000

/*
Functions accessible via the original interface at POS_CMD
*/

#define CMD_GET_MEM_SIZE 1
#define CMD_FLASH_CHECKSUM 2
#define CMD_FLASH_WRITE 3
#define CMD_FLASH_READ 4
#define CMD_GET_PIC_INFO 5
#define CMD_SET_CLOCK 6
#define CMD_GET_PIC_FMT 7
#define CMD_GET_VERSION 8
#define CMD_MESSAGE 9

#define MESSAGE_LEN 9

int sendcmd(int f,int cmd, unsigned int arg1, unsigned int arg2, unsigned char arg3) {
    ssize_t wrote_bytes;

    if (lseek(f,POS_CMD,SEEK_SET) != POS_CMD) {
        printf("ERROR: Seek to POS_CMD failed.\n");
        return 0;
    }

    cmdbuf[0]=cmd;
    cmdbuf[1]=(arg1>>24)&0xff;
    cmdbuf[2]=(arg1>>16)&0xff;
    cmdbuf[3]=(arg1>>8)&0xff;
    cmdbuf[4]=(arg1>>0)&0xff;
    cmdbuf[5]=(arg2>>24)&0xff;
    cmdbuf[6]=(arg2>>16)&0xff;
    cmdbuf[7]=(arg2>>8)&0xff;
    cmdbuf[8]=(arg2>>0)&0xff;
    cmdbuf[9]=(arg3);
    //printf("%02X %02X %02X %02X %02X\n", cmdbuf[0], cmdbuf[1], cmdbuf[2], cmdbuf[3], cmdbuf[4]);
    wrote_bytes = write(f,cmdbuf,SCSI_SECTOR_SIZE);

    if (wrote_bytes != SCSI_SECTOR_SIZE) {
        printf("ERROR: Write failed for command %i.\n", cmd);
        return 0;
    } else {
        return 1;
    }
}

#ifdef DEBUG
int tst(int f) {
    unsigned char *buff;
    buff=malloc_aligned(0x200);
    buff[0]=3;
    buff[1]=0;
    buff[2]=0;
    buff[3]=0;
    buff[4]=0x04;
    buff[5]=0;
    buff[6]=0;
    buff[7]=0;
    lseek(f,0x4400,SEEK_SET);
    return write(f,buff,0x200);
}
#endif

int read_data(int f, unsigned char *buff, int len) {
    lseek(f,POS_RDAT,SEEK_SET);
    return read(f,buff,len);
}

int write_data(int f, unsigned char *buff, int len) {
    lseek(f,POS_WDAT,SEEK_SET);
    return write(f,buff,len);
}

int get_mem_size(int f) {
    sendcmd(f,CMD_GET_MEM_SIZE,0,0,0);
    if (read_data(f,buff,SCSI_SECTOR_SIZE) == SCSI_SECTOR_SIZE) {
        return (buff[0]*128*1024)/512;
    } else {
        return -1;
    }
}

int calculate_flash_size(int f) {
    int mem, x;

    mem = get_mem_size(f);
    if (mem <= 0) {
        printf("ERROR: Failed to get memory size.\n");
    }
    printf("Device reports %i kb memory.\n",mem);

    x = 1;
    while (x < mem) x <<= 1;
    printf("Assuming flash size is %i kb or %i pages.\n",x,x/32);

    return x;
}


void print_image_size(int f) {
    sendcmd(f,CMD_GET_PIC_INFO,0,0,0);
    read_data(f,buff,SCSI_SECTOR_SIZE);
    int xsize = (buff[0]<<8)+buff[1];
    int ysize = (buff[2]<<8)+buff[3];
    int bpp = buff[4]-0x80;
    printf("Xres: %i, Yres: %i, bpp: %i\n",xsize,ysize,bpp);
}

void print_firmware_version(int f) {
    sendcmd(f,CMD_GET_VERSION,0,0,0);
    read_data(f,buff,SCSI_SECTOR_SIZE);
    printf("ver: %02x %02x %02x\n", buff[0], buff[1], buff[2]);
}

void print_picture_format(int f) {
    sendcmd(f,CMD_GET_PIC_FMT,0,0,0);
    read_data(f,buff,SCSI_SECTOR_SIZE);
    printf("picture format: %02x %02x\n", buff[0], buff[1]);
}

int checksum_page(int f, int p, unsigned int *c) {
    /* Firmware subtracts two from the whole 16 bit value */
    if (sendcmd(f,CMD_FLASH_CHECKSUM,(p-2)&0xFFFF,0,0) != 1) return 0;
    if (read_data(f,buff,SCSI_SECTOR_SIZE) != SCSI_SECTOR_SIZE) return 0;
    *c=(buff[0]<<24)+(buff[1]<<16)+(buff[2]<<8)+buff[3];
    return 1;
}

int read_page(int f, int p) {
    /* Firmware subtracts two from the low byte only */
    sendcmd(f,CMD_FLASH_READ,(p&0xFF00)|(((p&0xFF)-2)&0xFF),0,0);
    return read_data(f,buff,DRR_PAGE_SIZE);
}

int dump_pages(int f, int o, int start_page, int n) {
    int i, bytes;
    for (i = start_page; i < start_page+n; i++) {
        bytes = read_page(f, i);
        if (bytes != DRR_PAGE_SIZE) {
            printf("ERROR: got only 0x0%4x bytes for page 0x%04x!\n",
                   (int)bytes, i);
        }
        bytes = write(o,buff,DRR_PAGE_SIZE);
        if (bytes != DRR_PAGE_SIZE) {
            printf("FATAL ERROR: file write failed for page 0x%04x!\n", i);
            return 0;
        }
        fprintf(stderr,".");
    }
    return 1;
}

int set_clock(int f, int y, unsigned char month, unsigned char d, unsigned char h, unsigned char min) {
    return sendcmd(f,CMD_SET_CLOCK,
                   ((y&0xFFFF)<<16)|(month<<8)|d,
                   (h<<24)|(min<<16), 0);
}

unsigned int checksum32(unsigned char *data, unsigned int len) {
    unsigned int i, checksum = 0;

    for (i = 0; i < len; i++) {
        checksum += data[i];
    }

    return checksum;
}

int write_page(int f, unsigned char *data, int p) {
    ssize_t wrote_bytes;

    /* Firmware subtracts two from the low byte only */
    if (sendcmd(f,CMD_FLASH_WRITE,(p&0xFF00)|(((p&0xFF)-2)&0xFF),DRR_PAGE_SIZE,0) != 1) {
        printf("ERROR: Write command sending failed.");
        return 0;
    }

    wrote_bytes = write_data(f, data, DRR_PAGE_SIZE);

    if (wrote_bytes != DRR_PAGE_SIZE) {
        printf("ERROR: Write for page %02x returned %i.\n",
               p, (int)wrote_bytes);
        return 0;
    }
    return 1;
}

int write_page_with_verify(int f, unsigned char *data, int page) {
    unsigned int csumhere, csumthere;

    csumhere = checksum32(data, DRR_PAGE_SIZE);

    write_page(f, data, page);

    if (checksum_page(f, page, &csumthere) != 1) {
        printf("ERROR: Checksum command failed at page %i\n", page);
        return 0;
    }

    if (csumhere != csumthere) {
        printf("ERROR: Checksum mismatch after write: page=%i buffer=%08x, device=%08x\n",
               page, csumhere, csumthere);
        return 0;
    }
    return 1;
}

off_t get_file_size(int o) {
    off_t offset;
    int ret = 1;

    offset = lseek(o,0,SEEK_END);
    if (offset < 0) {
        printf("ERROR: Failed to seek to end of file.");
        ret = 0;
    } else if (offset == 0) {
        printf("ERROR: File is empty.");
        ret = 0;
    }

    if (lseek(o,0,SEEK_SET) != 0) {
        printf("ERROR: Failed to start of file.");
        ret = 0;
    }

    return ret ? offset : 0;
}

int upload_file(int f, int p, int o) {
    int curpage, endpage;
    off_t offset;

    offset = get_file_size(o);
    if (offset <= 0) return 0;

    if (offset & 0x7FFF) {
        printf("ERROR: File size not multiple of page size.");
        return 0;
    }

    endpage = offset >> 15;
    if (endpage > 0x80) {
        printf("ERROR: Too many pages (FIXME hardcoded limit).");
        return 0;
    }
    printf("Writing %i pages\n", endpage);
    endpage += p;

    for (curpage = p; curpage < endpage; curpage++) {
        ssize_t gotbytes;
        gotbytes = read(o, buff, DRR_PAGE_SIZE);
        if (gotbytes != DRR_PAGE_SIZE) {
            printf("ERROR: Read for page %02x returned %i.\n",
                   curpage, (int)gotbytes);
            return 0;
        }
        if (write_page_with_verify(f,buff,curpage) != 1) {
            return 0;
        }
        fprintf(stderr,".");
    }
    return 1;
}

#if 0
int upload_firmware(int f, int o) {
    off_t offset;
    ssize_t gotbytes, wrote_bytes;

    offset = lseek(o,0,SEEK_END);
    if (offset < 0) {
        printf("ERROR: Failed to seek to end of file.");
        return 0;
    } else if (offset != FIRMWARE_SIZE) {
        printf("ERROR: File size wrong, should be %x bytes.", FIRMWARE_SIZE);
        return 0;
    }

    offset = lseek(o,0,SEEK_SET);
    if (offset != 0) {
        printf("ERROR: Failed to start of file.");
        return 0;
    }

    gotbytes = read(o, buff, FIRMWARE_SIZE);
    if (gotbytes != FIRMWARE_SIZE) {
        printf("ERROR: Firmware file read failed.\n");
        return 0;
    }

    printf("WARNING: Uploading firmware now. Do not power off or unplug!");

    if (sendcmd(f, CMD_FLASH_WRITE, 0x80000000, FIRMWARE_SIZE, 0) != 1) {
        printf("ERROR: Firmware write command sending failed.");
        return 0;
    }

    wrote_bytes = write_data(f, buff, FIRMWARE_SIZE);

    if (wrote_bytes != FIRMWARE_SIZE) {
        printf("ERROR: Firmware write returned %i.\n", wrote_bytes);
        return 0;
    }
    return 1;
}
#else
int upload_firmware(int f, int o) {
    unsigned int cksumhere, cksumthere;
    int    x, y;
    unsigned char *cksumbuf;

    cksumbuf = malloc_aligned(0x200);

    for (x=0; x<2; x++) {
        y=read(o,buff,0x8000);
        if (y!=0x8000) {
            printf("Premature file end. Hope everything still works OK.\n");
            break;
        }

        cksumhere = checksum32(buff, 0x8000);

        sendcmd(f,3,x|0x80000000,0x8000,0);
        write_data(f,buff,0x8000);

        sendcmd(f,2,x+4 /* x|0x80000000 should be this but buggy */,0x8000,0);
        read_data(f,cksumbuf,0x200);
        cksumthere = (cksumbuf[0]<<24)+(cksumbuf[1]<<16)+(cksumbuf[2]<<8)+cksumbuf[3];

        if (cksumhere == cksumthere) {
            //nosendcmd(f,3,x|0x1f40,0x8000,0);
            //nowrite_data(f,buff,0x8000);
            printf("Not sending silly command\n");
        } else {
            printf("Checksum error at block %i %08x %08x\n", x, cksumhere, cksumthere);
            break;
        }

        fprintf(stderr,".");
    }

    free_aligned(cksumbuf, 0x200);
    printf("Upload done");

    return 1;
}
#endif

unsigned char *get_ram(int f) {
    int bytes;

    bytes = read_page(f, 0);
    if (bytes != DRR_PAGE_SIZE) {
        printf("ERROR: got only %i bytes for page 0.", bytes);
        return NULL;
    }

    /* This read wraps around, reading RAM */
    bytes = read_data(f, buff, DRR_PAGE_SIZE);
    if (bytes != DRR_PAGE_SIZE) {
        printf("ERROR: got only %i bytes for wrapped page.\n", bytes);
        return NULL;
    } else {
        return buff;
    }
}

int dump_ram(int f, int o) {
    int bytes;
    unsigned char *ram;

    ram = get_ram(f);
    if (ram == NULL) return 0;

    bytes = write(o,ram,RAM_SIZE);
    if (bytes != RAM_SIZE) {
        printf("FATAL ERROR: file write failed.\n");
        return 0;
    } else {
        return 1;
    }
}

int send_message(int f, char *s)
{
    if (sendcmd(f,CMD_MESSAGE,0,0,0) != 1) {
        printf("ERROR: Failed to send message command");
        return 0;
    }

    strncpy((char *)buff, s, 9);

    if (write_data(f, buff, SCSI_SECTOR_SIZE) != SCSI_SECTOR_SIZE) {
        printf("ERROR: Failed to send data for message.\n");
        return 0;
    } else {
        return 1;
    }
}

int hack_frame(int f, char *tag, unsigned char *b)
{
    ssize_t wrote_bytes;

    if (lseek(f,POS_CMD,SEEK_SET) != POS_CMD) {
        printf("ERROR: Seek to POS_CMD failed.\n");
        return 0;
    }

    buff[0]=8;
    buff[1]=tag[0];
    buff[2]=tag[1];
    buff[3]=tag[2];
    buff[4]=tag[3];

    memcpy(&buff[SCSI_SECTOR_SIZE-USB_PACKET_SIZE], b, USB_PACKET_SIZE);

    wrote_bytes = write(f,buff,SCSI_SECTOR_SIZE);

    if (wrote_bytes != SCSI_SECTOR_SIZE) {
        printf("ERROR: Write failed for command hack.\n");
        return 0;
    } else {
        return 1;
    }
}

int hack_text(int f, char *msg)
{
    int len;

    len = strlen(msg);
    if (len > USB_PACKET_SIZE) len = USB_PACKET_SIZE;

    if (len == USB_PACKET_SIZE) {
        return hack_frame(f, "TXTP", (unsigned char *)msg);
    } else {
        unsigned char buf[USB_PACKET_SIZE];
        memcpy(buf, msg, len);
        memset(&buf[len], ' ', USB_PACKET_SIZE-len);
        return hack_frame(f, "TXTP", (unsigned char *)buf);
    }
}

int hack_code(int f, int o)
{
    off_t filesize;
    ssize_t gotbytes;
    unsigned char buf[USB_PACKET_SIZE];

    filesize = get_file_size(o);
    if (filesize <= 0) return 0;

    if (filesize > USB_PACKET_SIZE) {
        printf("ERROR: Simple code upload limited to %i bytes.\n", USB_PACKET_SIZE);
        return 0;
    }

    gotbytes = read(o, buf, filesize);
    if (gotbytes != filesize) {
        printf("ERROR: File read returned %i.\n", (int)gotbytes);
        return 0;
    }

    return hack_frame(f, "HACK", buf);
}

/* This is to for uploading to BKO buffer appended with data for
 * copying to another bigger free area in memory
 */
const unsigned char uploader[] = {
    /* 0200 */ 0x78,       /* sei */
    /* 0201 */ 0xA9, 0x1F, /* lda #cpdata&$FF */
    /* 0203 */ 0x85, 0x58, /* sta DMSL */
    /* 0205 */ 0xA9, 0x02, /* lda #cpdata>>8 */
    /* 0207 */ 0x85, 0x59, /* sta DMSH */
    /* 0209 */ 0x64, 0x5E, /* stz DMRL */
    /* 020B */ 0x64, 0x5F, /* stz DMRH */
#define UPLOADER_DESTL 0xE
    /* 020D */ 0xA9, 0x80, /* lda #FREERAM&$FF */
    /* 020F */ 0x85, 0x5A, /* sta DMDL */
#define UPLOADER_DESTH 0x12
    /* 0211 */ 0xA9, 0x05, /* lda #FREERAM>>8 */
    /* 0213 */ 0x85, 0x5B, /* sta DMDH */
    /* 0215 */ 0x64, 0x5D, /* stz DCNTH */
#define UPLOADER_SIZEL 0x18
    /* 0217 */ 0xA9, 0x20, /* lda #BKO_BUF+BKO_BUF_SIZE-cpdata-1 */
    /* 0219 */ 0x85, 0x5C, /* sta DCNTL ; DMA runs here */
    /* 021B */ 0x58,       /* cli */
    /* 021C */ 0x4C, 0x52, 0x7E /* jmp  $7E52 */
    /* 021F */
};

int hack_code_long(int f, int o) {
    unsigned char buf[USB_PACKET_SIZE];
    unsigned char codebuf[FREE_RAM_SIZE];
    int bytesremain;
    unsigned int offset = 0;
    ssize_t gotbytes;
    unsigned char *ram;

    bytesremain = get_file_size(o);
    if (bytesremain <= 0) return 0;

    if (bytesremain > FREE_RAM_SIZE) {
        printf("ERROR: Long code upload limited to %i bytes.\n", FREE_RAM_SIZE);
        return 0;
    }

    gotbytes = read(o, codebuf, bytesremain);
    if (gotbytes != bytesremain) {
        printf("ERROR: File read returned %i.\n", (int)gotbytes);
        return 0;
    }

    memcpy(buf, uploader, sizeof(uploader));

    fprintf(stderr, "Uploading code: ");

    while (bytesremain > 0) {
        unsigned int chunksize;

        if (bytesremain > USB_PACKET_SIZE-sizeof(uploader)) {
            chunksize = USB_PACKET_SIZE-sizeof(uploader);
        } else {
            chunksize = bytesremain;
            buf[UPLOADER_SIZEL] = bytesremain - 1;
        }

        buf[UPLOADER_DESTH] = (offset+FREE_RAM_ADDR) >> 8;
        buf[UPLOADER_DESTL] = (offset+FREE_RAM_ADDR) & 0xFF;

        memcpy(&buf[sizeof(uploader)], &codebuf[offset], chunksize);

        //write(STDOUT_FILENO, buf, USB_PACKET_SIZE);
        if (hack_frame(f, "HACK", buf) != 1) {
            printf("ERROR: Upload failed at %i\n", offset);
            return 0;
        }
        fprintf(stderr, ".");

        offset += chunksize;
        bytesremain -= chunksize;
    }

    printf("\nVerifying code upload.\n");

    ram = get_ram(f);
    if (memcmp(codebuf, &ram[FREE_RAM_ADDR], gotbytes) != 0) {
        printf("Code upload verification failed\n");
        return 0;
    }

    printf("Executing code now.\n");

    buf[0] = 0x4C; /* jmp FREE_RAM_ADDR */
    buf[1] = FREE_RAM_ADDR & 0XFF;
    buf[2] = FREE_RAM_ADDR >> 8;

    if (hack_frame(f, "HACK", buf) != 1) {
        printf("ERROR: Upload of execution jump failed.\n");
        return 0;
    }

    return 1;
}

int hack_image(int f, int o)
{
    int bytesremain;
    unsigned int bufidx = 64;
    ssize_t bytes;

    //int d=open("dump",O_WRONLY|O_TRUNC|O_CREAT,0644);

    bytesremain = get_file_size(o);
    if (bytesremain <= 0) return 0;

    if (bytesremain != 320*240*3) {
        printf("ERROR: 100*100*3\n");
        return 0;
    }

    buff[0] = 0x10;
    buff[1] = 0;
    buff[2] = 0;
    buff[3] = 320>>8;
    buff[4] = 320&0xFF;
    buff[5] = 0;
    buff[6] = 240;

    while (bytesremain > 0) {
        int chunksize;

        if (bytesremain > 63) {
            chunksize = 63;
        } else {
            chunksize = bytesremain;
        }

        buff[bufidx] = 0xC0+chunksize-1;

        bytes = read(o, &buff[bufidx+1], chunksize);
        if (bytes != chunksize) {
            printf("ERROR: File read returned %i.\n", (int)bytes);
            return 0;
        }

        bufidx += 64;
        bytesremain -= chunksize;

        if (bufidx == DRR_PAGE_SIZE) {
            bytes = write_data(f, buff, bufidx);

            if (bytes != bufidx) {
                printf("ERROR: Write returned %i.\n", (int)bytes);
                return 0;
            }

            bufidx = 0;
        }
    }

    while (bufidx & (SCSI_SECTOR_SIZE-1)) {
        buff[bufidx] = 0;
        bufidx += 64;
    }

    bytes = write_data(f, buff, bufidx);

    if (bytes != bufidx) {
        printf("ERROR: Write returned %i.\n", (int)bytes);
        return 0;
    }

    return 1;
}

//#ifdef DEBUG
/*
Debugging routine to dump a buffer in a hexdump-like fashion.
*/
void dumpmem(unsigned char* mem, int len) {
    int x,y;
    for (x=0; x<len; x+=16) {
    printf("%04x: ",x);
    for (y=0; y<16; y++) {
        if ((x+y)>len) {
        printf("   ");
        } else {
        printf("%02hhx ",mem[x+y]);
        }
    }
    printf("- ");
    for (y=0; y<16; y++) {
        if ((x+y)<=len) {
        if (mem[x+y]<32 || mem[x+y]>127) {
            printf(".");
        } else {
            printf("%c",mem[x+y]);
        }
        }
    }
    printf("\n");
    }
}
//#endif

enum mode_e {
    M_NONE = 0,
    M_UP,
    M_DMP,
    M_FUP,
    M_FDMP,
    M_RDMP,
    M_MSG,
//#define M_LCD   6
    M_INFO,
    M_H_MSG,
    M_H_CODE64,
    M_H_CODELONG,
    M_H_IMAGE
};

enum paramtype_e {
    P_NONE = 0,
    P_TEXT,
    P_INFILE,
    P_OUTFILE
};

struct command_s {
    char *cmdlparam;
    char *help;
    enum mode_e mode;
    enum paramtype_e parameter;
};

const struct command_s commands[] = {
    { "-dp", "dump picture memory", M_DMP, P_OUTFILE },
    { "-up", "upload picture memory", M_UP, P_INFILE },
    { "-df", "dump firmware", M_FDMP, P_OUTFILE },
    { "--upload-firmware", "upload firmware", M_FUP, P_INFILE },
    { "-dr", "dump RAM", M_RDMP, P_OUTFILE },
    { "-m", "display message (9 characters)", M_MSG, P_TEXT },
    { "-i", "print information", M_INFO, P_NONE },
    { "-Hm", "display message (64 characters)", M_H_MSG, P_TEXT },
    { "--upload-code", "upload and execute up to 64 bytes of code at 0x200", M_H_CODE64, P_INFILE },
    { "--upload-long-code", "upload and execute up to 0x200 bytes of code at 0x580", M_H_CODELONG, P_INFILE },
    { "--upload-image", "upload image", M_H_IMAGE, P_INFILE },
};

void print_usage(char *s)
{
    int i;

    printf("Usage:\n%s DEVICE OPERATION [PARAMETER]\n",s);
    for (i = 0; i < sizeof(commands)/sizeof(struct command_s); i++) {
        printf(" %s: %s\n", commands[i].cmdlparam, commands[i].help);
    }
// v2.0 - moved to setpic
//    printf(" -l: send png- to framebuffer mem (hacked fw only)\n");
    printf(" file: file to dump to or upload from\n");
    printf(" device: /dev/sdX (default: /dev/sda)\n");
//    printf("-l accepts directories too, in which case it'll proceed to send every file in the\n");
//    printf("directory to the LCD.\n");
}

int main(int argc, char** argv) {
    int f,o;
    unsigned int i;
    const struct command_s *command=NULL;

    int mem;

    if (argc<3) {
        print_usage(argv[0]);
        exit(0);
    }

    //check requested command
    for (i = 0; i < sizeof(commands)/sizeof(struct command_s); i++) {
        if (strcmp(argv[2],commands[i].cmdlparam) == 0) {
            command = &commands[i];
            break;
        }
    }

    if (command == NULL) {
        printf("Invalid command: %s\n",argv[1]);
        exit(1);
    }

    if (argc > ((command->parameter == 0) ? 3 : 4)) {
        printf("Too many parameters on command line.\n\n");
        print_usage(argv[0]);
        exit(1);
    }

    if (command->parameter != 0 && argc != 4) {
        printf("Command requires parameter.\n\n");
        print_usage(argv[0]);
        exit(1);
    }

    f=open(argv[1],O_RDWR|O_DIRECT|O_SYNC);

    //check if dev really is a photo-frame
    if (!is_photoframe(f)) {
        fprintf(stderr,"No photoframe found there.\n");
        exit(1);
    }

    switch (command->parameter) {
    case P_INFILE:
        o=open(argv[3],O_RDONLY);
        break;
    case P_OUTFILE:
        o=open(argv[3],O_WRONLY|O_TRUNC|O_CREAT,0644);
        break;
    default:
        break;
    };

    if (command->parameter >= P_INFILE && f < 0) {
        fprintf(stderr,"Error opening %s.\n",argv[3]);
        exit(1);
    }

    //Allocate buffer and send a command. Check the result as an extra caution
    //against non-photoframe devices.
    buff=malloc_aligned(FIRMWARE_SIZE);
    cmdbuf=malloc_aligned(SCSI_SECTOR_SIZE);

    //mem=calculate_flash_size(f);
    mem = 4 * 1024;
#if 0
    //print_image_size(f);

    //print_firmware_version(f);

    //print_picture_type(f);

    //int i;
    //for (x=125; x < 132; x++) {
    //for (i = 1; i < 258; i++) {
    for (x = 0; 1; x+=0x1) {
        printf("%03x ", x); checksum_page(f, x);
    }
    set_clock(f, 2004, 7, 24, 13, 45);
    upload_firmware(f,o);

    if (mode==M_DMP) {
        //dump picture memory FIXME
        dump_pages(f, o, 0x7FFF, 5);
    } else {
        int x;
        sendcmd(f,CMD_FLASH_READ,0xFE,0,0);
        dump_pages(f, o, 0, 0x80);
    }


    if (mode==M_DMP) {
        //dump picture memory FIXME
    } else if (mode==M_MSG) {

    } else {
        hack_code_file(f, o);
    }


#endif

    switch (command->mode) {
    //case M_UP:
    //case M_DMP,
    //case M_FUP,
    case M_FDMP:
        dump_pages(f, o, 0, 2);
        break;
    case M_RDMP:
        dump_ram(f, o);
        break;
    case M_MSG:
        send_message(f, argv[3]);
        break;
    case M_INFO:
        print_image_size(f);
        print_picture_format(f);
        print_firmware_version(f);
        break;
    case M_H_MSG:
        hack_text(f, argv[3]);
        break;
    case M_H_CODE64:
        hack_code(f, o);
        break;
    case M_H_CODELONG:
        hack_code_long(f, o);
        break;
    case M_H_IMAGE:
        hack_image(f, o);
        break;
    default:
        printf("Command not implemented.");
    }

    free_aligned(buff, FIRMWARE_SIZE);
    free_aligned(buff, SCSI_SECTOR_SIZE);

    return 0;
}