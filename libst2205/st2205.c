#define NO_PARM_BLOCK
/*
    ST2205U image library
    Copyright (C) 2008 Jeroen Domburg <jeroen@spritesmods.com>
    Copyright (C) 2008 Cyril Hrubis <metan@atrey.karlin.mff.cuni.cz>
    Copyright (C) 2009 Michael Vogt <michu@neophob.com>

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

/* This is required for O_DIRECT */
#define _GNU_SOURCE

#include <string.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/types.h>
#ifndef _WIN32
#include <sys/mman.h>
#endif
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include "st2205.h"

#define BUFF_SIZE 320*240*3*2 //0x10000

#define DPRINT(...) fprintf(stderr, __VA_ARGS__)

#define PROTO_PCF8833 0
#define PROTO_MERCURY 1

/* Commands for PROTO_MERCURY */
#define COMMAND_BASE 0x10 /* Commands start from this number */
#define CMD_SETWIN (COMMAND_BASE+0) /* Set window in LCD controller */
#define CMD_BLON (COMMAND_BASE+1) /* Backlight on */
#define CMD_BLOFF (COMMAND_BASE+2) /* Backlight off */
#define CMD_LCDWAKE (COMMAND_BASE+3) /* Wake LCD from deep sleep */
#define CMD_LCDSLEEP (COMMAND_BASE+4) /* LCD deep sleep, forgetting image data */

/*
 Two routines to allocate/deallocate page-aligned memory, for use with the
 O_DIRECT-opened files.
*/
static void *malloc_aligned(long size)
{
    void *buff;

#ifdef _WIN32
    buff = malloc(size);
    if (buff == NULL) {
        perror("Can't allocate memory");
        return NULL;
    }
#else /* !_WIN32 */
    int f;

    f = open("/dev/zero", O_RDONLY);

    if (f < 0) {
        perror("Can't open /dev/zero:");
        return NULL;
    }

    buff = mmap(0, size, PROT_READ | PROT_WRITE, MAP_PRIVATE, f, 0);

    close(f);
#endif /* !_WIN32 */

    return buff;
}

static void free_aligned(void *addr, long size)
{
#ifdef _WIN32
    (void)size;
    free(addr);
#else /* !_WIN32 */
    /* To ignore free_aligned() on NULL addr like free() does */
    if (addr != NULL)
        if (munmap(addr, size) < 0)
            perror(NULL);
#endif /* _WIN32 */
}


typedef struct {
    char sig[4];
    char version;
    unsigned char width;
    unsigned char height;
    char bpp;
    char proto;
    char offx;
    char offy;
} fw_descriptor;

/*
 Checks if the device is a photo frame by reading the first 512 bytes and
 comparing against the known string that's there
*/
static int is_photoframe(int f)
{
    int res;
    char id[] = "SITRONIX CORP.";
    char *buff;

    buff = malloc_aligned(0x200);

    if (buff == NULL) {
        fprintf(stderr, "aligned_malloc failed :(\n");
        return 0;
    }

    if (lseek(f, 0x0, SEEK_SET) < 0 || read(f, buff, 0x200) < 0) {
        perror(NULL);
        return 0;
    }

    res = strncmp(buff, id, 15);

    free_aligned(buff, 0x200);

    return !res;
}

/*
 The interface works by writing bytes to the raw 'disk' at certain positions.
 Commands go to offset 0x6200, data to be read from the device comes from 0xB000,
 data to be written goes to 0x6600. Hacked firmware has an extra address,
 0x4200: bytes written there will go straight to the LCD.
*/

#define POS_CMD  0x6200
#define POS_WDAT 0x6600
#define POS_RDAT 0xb000

#ifndef NO_PARM_BLOCK
static int sendcmd(int fd, int cmd, unsigned int arg1, unsigned int arg2, unsigned char arg3)
{
    unsigned char *buff;

    buff = malloc_aligned(0x200);

    if (buff == NULL) {
        fprintf(stderr, "malloc_aligned failed :(\n");
        return -1;
    }

    buff[0] = cmd;
    buff[1] = (arg1>>0x18)&0xff;
    buff[2] = (arg1>>0x10)&0xff;
    buff[3] = (arg1>>0x08)&0xff;
    buff[4] = (arg1>>0x00)&0xff;
    buff[5] = (arg2>>0x18)&0xff;
    buff[6] = (arg2>>0x10)&0xff;
    buff[7] = (arg2>>0x08)&0xff;
    buff[8] = (arg2>>0x00)&0xff;
    buff[9] = (arg3);

    if (lseek(fd, POS_CMD, SEEK_SET) < 0) {
        return -1;
    }

    return write(fd, buff, 0x200);
}

static int read_data(int fd, char* buff, int len)
{
    if (lseek(fd, POS_RDAT, SEEK_SET) < 0)
        return -1;
    return read(fd, buff, len);
}

static int write_data(int fd, char* buff, int len)
{
    if (lseek(fd, POS_WDAT, SEEK_SET) < 0)
        return -1;
    return write(fd, buff, len);
}

/*
Debugging routine to dump a buffer in a hexdump-like fashion.
*/
static void dumpmem(unsigned char* mem, int len)
{
    int x, y;

    for (x=0; x<len; x+=16) {

        printf("%04x: ", x);

        for (y=0; y<16; y++) {
            if ((x+y) > len) {
                printf("   ");
            } else {
                printf("%02hhx ", mem[x+y]);
            }
        }

        printf("- ");

        for (y=0; y<16; y++) {
            if ((x+y) <= len) {
                if (mem[x+y]<32 || mem[x+y]>127) {
                    printf(".");
                } else {
                    printf("%c", mem[x+y]);
                }
            }
        }

        printf("\n");
    }
}

#define FW_PAGE_OFFSET 0xFE //((2048-64)/32)
static fw_descriptor *get_parm_block(int fd, char* buff)
{
    int a, p;
    char lookfor[] = "H4CK\000";

    /*
     Read 64K of firmware into buff
     */
    sendcmd(fd, 4, FW_PAGE_OFFSET, 0x8000, 0);
    read_data(fd, buff, 0x8000);
    sendcmd(fd, 4, FW_PAGE_OFFSET+1, 0x8000, 0);
    read_data(fd, buff+0x8000, 0x8000);

    /*
     Look for 'H4CK' string
     */
    for (a=0; a<0x10000-8; a++) {
        p = 0;
        while (lookfor[p] != 0 && buff[a+p] == lookfor[p])
            p++;
        if (lookfor[p] == 0) {
            return (fw_descriptor*)(buff+a);
        }
    }

    return NULL;
}
#endif /* #ifndef NO_PARM_BLOCK */

static int enddata(char *buff, int p)
{
    int pageaddr, offset;

    offset = p & 63;
    pageaddr = p-offset;

    if (offset == 0)
        return p;

    //buff[pageaddr + 1] = offset - 1;
    buff[pageaddr] = 0xC0 + offset - 1;
    //fprintf(stderr, "(%i)", buff[pageaddr]);
    p = pageaddr + 64;

    return p;
}

static int pcf8833_setxy(st2205_handle *h, char *buff, int p, int xs, int xe, int ys, int ye)
{
    int xsoff = xs + h->offx;
    int xeoff = xe + h->offx;

    p = enddata(buff, p);

    switch (h->proto) {
    case PROTO_PCF8833:
        buff[p] = 1;
        buff[p+1] = xsoff;
        buff[p+2] = xeoff;
        buff[p+3] = ys + h->offy;
        buff[p+4] = ye + h->offy;
        break;

    case PROTO_MERCURY:
        buff[p] = CMD_SETWIN;
        buff[p+1] = (xsoff & 0xff00) >> 8;
        buff[p+2] = (xsoff & 0xff);
        buff[p+3] = (xeoff & 0xff00) >> 8;
        buff[p+4] = (xeoff & 0xff);
        buff[p+5] = ys + h->offy;
        buff[p+6] = ye + h->offy;
        break;
    default:
        fprintf(stderr, "libst2205: Unrecognized protocol: 0x%x!\n", h->proto);
        //TODO: do not exit here, library should just send error to app
        exit(1);
    }

    p += 64;
    return p;
}

static int adddata(char *buff, int p, char d)
{
    if ((p&63) == 0) {
//        buff[p] = 0;
//        p += 2;
        p += 1;
    }

    buff[p] = d;

    if ((p&63) == 63)
        p = enddata(buff, p);
    else
        p++;

    return p;
}

static unsigned int getpixel(st2205_handle *h, unsigned char *pix, unsigned int x, unsigned int y)
{
    unsigned int r, a;

    if (x >= h->width || y >= h->height)
        return 0;

    a = (x+h->width*y)*3;
    r = pix[a]+(pix[a+1]<<8)+(pix[a+2]<<16);

    return r;
}

static int write_stream(st2205_handle *h, char *buff, int len)
{
    /*
     Pad to 512-byte boundary, with 0xff
     */
    for(; len%512; len++) {
        buff[len] = 0;
    }

    //DPRINT("Writing 0x%x bytes.\n",len);

    if (lseek(h->fd, POS_WDAT, SEEK_SET) < 0)
        return -1;

    return write(h->fd, buff, len);
}

/*
 Sends image (xs,ys)-(xe,ye), inclusive.
 */
void st2205_send_partial(st2205_handle *h, unsigned char *pixinfo, int xs, int ys, int xe, int ye)
{
    int p, x, y, z;
    unsigned int r, g, b, c;
    long tr;

    /*
     bpp=12, make width and xstart even
     */
    if (h->bpp == 12) {
        xs-=(xs&1);
        xe+=(xe-xs+1)&1;
    }

    p = 0;
    p = pcf8833_setxy(h, h->buff, 0, xs, xe, ys, ye);
    for (y=ys; y<=ye; y++) {
        for (x=xs; x<=xe; x++) {
            //fprintf(stderr, "(%i,%i)",x,y);
            switch (h->bpp) {
                case 24:
                    c = getpixel(h, pixinfo, x, y);
                    r = (c&0xff); g=(c>>8)&0xff; b=(c>>16)&0xff;
                    p = adddata(h->buff, p, r);
                    p = adddata(h->buff, p, g);
                    p = adddata(h->buff, p, b);
                break;
                case 16:
                    c = getpixel(h, pixinfo, x, y);
                    r=(c&0xff); g=(c>>8)&0xff; b=(c>>16)&0xff;
                    r>>=3; g>>=2; b>>=3;
                    c=(r<<11)+(g<<5)+b;
                    p = adddata(h->buff, p, (c>>8));
                    p = adddata(h->buff, p, (c&255));
                break;
                case 12:
                    tr=0;
                    for (z=0; z<2; z++) {
                        c = getpixel(h, pixinfo, x+z, y);
                        r=(c&0xff); g=(c>>8)&0xff; b=(c>>16)&0xff;
                        r>>=4;g>>=4;b>>=4;
                        tr=(tr<<12)+(r<<8)+(g<<4)+(b);
                    }
                //        p=adddata(h->buff,p,0xff);
                //        p=adddata(h->buff,p,0xff);
                //        p=adddata(h->buff,p,0xff);

                    p = adddata(h->buff, p, (tr>>16)&0xff);
                    p = adddata(h->buff, p, (tr>>8)&0xff);
                    p = adddata(h->buff, p, (tr)&0xff);
                    x++; //because we handle 2 pixels at a time
                break;
                default:
                    fprintf(stderr, "libst2205: Unknown bpp for this display: %i\n", h->bpp);
                    //TODO: do not exit here, library should just send error to app
                    exit(1);
                break;
            }
        }
    }

    p = enddata(h->buff, p);
    write_stream(h, h->buff, p);
}

/*
 Pixinfo is a char array containing r,g,b triplets.
 */
void st2205_send_data(st2205_handle *h, unsigned char *pixinfo)
{
    unsigned int x,y,xs,xe,ys,ye,c1,c2;


    /*
     PCF8833 has the possibility to do partial transfers into a certain bounding
     box. Optimize for that by looking for the smallest bounding box containing
     all changes.
     */
        xe = 0; ye = 0; xs = h->width; ys = h->height;
    if (h->proto == PROTO_PCF8833 || h->proto == PROTO_MERCURY) {
        if (h->oldpix == NULL) {
            st2205_send_partial(h, pixinfo, 0, 0, h->width - 1, h->height - 1);
        } else {
            /*
             go send incremental image
             Algorithm: go find biggest bounding box.
             It's semi-efficient: usually it works, but it could be that
             dividing the difference in multiple bounding boxes works better.
             */


            for (x=0; x<h->width; x++) {
                for (y=0; y<h->height; y++) {
                    c1 = getpixel(h, pixinfo, x, y);
                    c2 = getpixel(h, h->oldpix, x, y);
                    if (c1 != c2) {
                        if (x < xs)
                            xs = x;
                        if (y < ys)
                            ys = y;
                        if (x > xe)
                            xe = x;
                        if (y > ye)
                            ye = y;
                    }
                }
            }

                         /* Sometimes there is no change. */
            if (xs <= xe) {
                            st2205_send_partial(h, pixinfo, xs, ys, xe, ye);
                          }
        }
    } else {
        fprintf(stderr, "libst2205: Unrecognized protocol: 0x%x!\n", h->proto);
    }

    /*
     Store old buffer in case we need to calculate differences next.
     */
    if (h->oldpix == NULL) {
        h->oldpix = malloc(h->width*h->height*3);
    }

    /*
     Not to fail if malloc haven't allocated memory.
     */
    if (h->oldpix != NULL && xs <= xe) {
            memcpy(h->oldpix, pixinfo, h->width*h->height*3);
        }

}

void st2205_rgba(st2205_handle *h, const unsigned char *data)
{
    int i, pixels = h->width * h->height;
    const unsigned char *src = data;
    unsigned char *dest;

    if (h->rgbabuf == NULL) {
        h->rgbabuf = malloc(pixels * 3);
        if (h->rgbabuf == NULL) return;
    }

    dest = h->rgbabuf;
    for (i = pixels; i > 0; i--) {
        dest[0] = src[2];
        dest[1] = src[1];
        dest[2] = src[0];
        dest += 3;
        src += 4;
    }

    st2205_send_data(h, h->rgbabuf);
}

void st2205_rgba_partial(st2205_handle *h, const unsigned char *data,
                         int xs, int ys, int xe, int ye)
{
    int x, rows, y, cols, pixidx, pixskip, srcskip, destskip;
    const unsigned char *src;
    unsigned char *dest;

    if (h->rgbabuf == NULL) {
        h->rgbabuf = malloc(h->width * h->height * 3);
        if (h->rgbabuf == NULL) return;
    }

    cols = xe - xs + 1;
    rows = ye - ys + 1;

    pixidx = h->width * ys + xs;
    pixskip = h->width - cols;
    src = &data[pixidx * 4];
    srcskip = pixskip * 4;
    dest = &(h->rgbabuf[pixidx * 3]);
    destskip = pixskip * 3;

    for (y = rows; y != 0; y--) {
        for (x = cols; x != 0; x--) {
            dest[0] = src[2];
            dest[1] = src[1];
            dest[2] = src[0];
            dest += 3;
            src += 4;
        }
        src += srcskip;
        dest += destskip;
    }

    st2205_send_partial(h, h->rgbabuf, xs, ys, xe, ye);
}

/*
 Send command to turn bl on or off
 */
void st2205_backlight(st2205_handle *h, int on)
{
    if (on) {
        h->buff[0]=CMD_BLON;
    } else {
        h->buff[0]=CMD_BLOFF;
    }

    write_stream(h, h->buff, 1);
}

void st2205_lcd_sleep(st2205_handle *h, int sleep)
{
    if (sleep) {
        h->buff[0]=CMD_LCDSLEEP;
    } else {
        h->buff[0]=CMD_LCDWAKE;
    }

    write_stream(h, h->buff, 1);
}

void st2205_close(st2205_handle *h)
{
    close(h->fd);
    free_aligned(h->buff, BUFF_SIZE);

    if (h->oldpix != NULL)
        free(h->oldpix);

    if (h->rgbabuf != NULL)
        free(h->rgbabuf);

    free(h);
}

static int hack_frame(int f, char *buff)
{
    ssize_t wrote_bytes;

    if (lseek(f,POS_CMD,SEEK_SET) != POS_CMD) {
        printf("ERROR: Seek to POS_CMD failed.\n");
        return 0;
    }

    buff[0]=8;
    buff[1]='H';
    buff[2]='A';
    buff[3]='C';
    buff[4]='K';
    memset(&buff[5], 0, 4);

    wrote_bytes = write(f,buff,0x200);

    if (wrote_bytes != 0x200) {
        printf("ERROR: Write failed for command hack.\n");
        return 0;
    } else {
        return 1;
    }
}

st2205_handle *st2205_open(const char *dev)
{
    st2205_handle *r = NULL;
#ifndef NO_PARM_BLOCK
    fw_descriptor *b = NULL;
#endif
    int fd;
    void *buff = NULL;

    fd = open(dev, O_RDWR
#ifdef _WIN32
                   | O_BINARY
#else
                   | O_DIRECT
#endif
              );

    if (fd < 0) {
        perror(dev);
        return NULL;
    }

    if (!is_photoframe(fd)) {
        close(fd);
        return NULL;
    }

    buff = malloc_aligned(BUFF_SIZE);
    if (buff == NULL) {
        close(fd);
        free_aligned(buff, BUFF_SIZE);
        return NULL;
    }

#ifndef NO_PARM_BLOCK
    b = get_parm_block(fd, buff);
    if (b==NULL) {
        printf("Unable to get parm_block\n");
        close(fd);
        free_aligned(buff,BUFF_SIZE);
        return(0);
    }
#endif

    r = malloc(sizeof(st2205_handle));
#ifndef NO_PARM_BLOCK
    if (b->version != 1 || r == NULL) {
        if (b->version != 1)
            fprintf(stderr, "Unknown version %hhi\n", b->version);
        close(fd);
        free_aligned(buff, BUFF_SIZE);
        return NULL;
    }
#endif

    r->fd     = fd;
    r->buff   = buff;
#ifndef NO_PARM_BLOCK
    r->width  = b->width;
    r->height = b->height;
    r->bpp    = b->bpp;
    r->proto  = b->proto;
    r->oldpix = NULL;
    r->offx   = b->offx;
    r->offy   = b->offy;
#else
    r->width  = 320;
    r->height = 240;
    r->bpp    = 24;
    r->proto  = PROTO_MERCURY;
    r->oldpix = NULL;
    r->offx   = 0;
    r->offy   = 0;
#endif
    r->rgbabuf = NULL;

    hack_frame(fd, buff);

    DPRINT("libst2205: detected device, %ix%i, %i bpp.\n", r->width, r->height, r->bpp);

    return r;
}
