/*
    Send a png to a sst2205 unit using libst2205.
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



#include <stdlib.h>
#include <dirent.h>
#include <stdio.h>
#include <unistd.h>
#include <gd.h>
#include <string.h>
#ifndef _WIN32
#include <termios.h>
#endif

#include <st2205.h>

enum imgtype {
    IMGTYPE_PNG,
    IMGTYPE_JPEG,
    IMGTYPE_GIF
};

enum imgtype imgtype_from_extension(char *filename) {
    enum imgtype ret = IMGTYPE_PNG;
    char *fext;

    fext = strrchr(filename, '.');
    if (fext == NULL) return ret;
    fext++;

    if (!strcasecmp(fext, "jpg") || !strcasecmp(fext, "jpeg")) {
        ret = IMGTYPE_JPEG;
    } else if (!strcasecmp(fext, "gif")) {
        ret = IMGTYPE_GIF;
    } /* Default to PNG */

    return ret;
}

/*
This routine reads a png-file from disk and puts it into buff in a format
lib understands. (basically 24-bit rgb)
*/
int sendpic(st2205_handle *h, char* filename) {
    FILE *in;
    gdImagePtr im, im2;
    unsigned char* pixels;
    int x,y;
    int p=0;
    unsigned int c,r,g,b;

    pixels=malloc(h->width*h->height*3);

    in=fopen(filename,"rb");
    if (in==NULL) return 0;
    //Should try other formats too
    switch (imgtype_from_extension(filename)) {
    case IMGTYPE_JPEG:
        im=gdImageCreateFromJpeg(in);
        break;
    case IMGTYPE_GIF:
        im=gdImageCreateFromGif(in);
        break;
    default:
    case IMGTYPE_PNG:
        im=gdImageCreateFromPng(in);
        break;
    }
    fclose(in);
    if (im==NULL) {
    printf("%s: not supported.\n",filename);
    return 0;
    }

    y = h->height * im->sx / h->width;
    if (im->sy > y) {
        y = h->height;
        x = im->sx * h->height / im->sy;
    } else {
        x = h->width;
        y = im->sy * h->width / im->sx;
    }
    fprintf(stderr, "resizing to %f x %f\n", (double)im->sx/x, (double)im->sy/y);
    /* Resize image to fit frame */
    im2 = gdImageCreateTrueColor(h->width, h->height);
    gdImageCopyResized(im2, im,
                       (h->width - x) / 2, (h->height - y) / 2, 0, 0,
                       x, y, im->sx, im->sy);
    gdImageDestroy(im);
    im = im2;

    //fprintf(stderr, "<%i,%i>\n", h->width, h->height);
    for (y=0; y<h->height; y++) {
    for (x=0; x<h->width; x++) {
        c = gdImageGetPixel(im, x, y);
        if (gdImageTrueColor(im) ) {
        r=gdTrueColorGetRed(c);
        g=gdTrueColorGetGreen(c);
        b=gdTrueColorGetBlue(c);
        } else {
        r=gdImageRed(im,c);
        g=gdImageGreen(im,c);
        b=gdImageBlue(im,c);
        }

//        pixels[p++]=0xff;
//        pixels[p++]=0;
//        pixels[p++]=0;
        pixels[p++]=r;
        pixels[p++]=g;
        pixels[p++]=b;
    }
    }
    st2205_send_data(h,pixels);
    gdImageDestroy(im);
    return 1;
}


void sendthis(st2205_handle *h, char* what) {
    int y;
    y=sendpic(h,what);
    if (!y) {
    //p'rhaps a dir?
    DIR *dir;
    struct dirent *dp;
    char fn[2048];
    dir=opendir(what);
    if (dir==NULL) {
        //Nope :/
        printf("Couldn't open %s.\n",what);
        exit(1);
    }
    while ((dp = readdir (dir)) != NULL) {
        strcpy(fn,what);
        strcat(fn,"/");
        strcat(fn,dp->d_name);
        sendpic(h,fn);
    }
    }
}

#ifndef _WIN32
//This is a debugging routine. Don't EVER mess with the internals of the
//st2205_handle like this in a real program!
void testpic(st2205_handle *h, char* what) {
    char c;
    struct termios term;
    struct termios oldterm;

    tcgetattr(0, &oldterm);
    tcgetattr(0, &term);
    term.c_lflag &= ~ICANON;
    tcsetattr(0, TCSANOW, &term);
    setbuf(stdin, NULL);

    c='i';
    while (c!='q') {
    //force complete image redraw instead of the optimized
    //draw-only-whats-changed stuff
    if (h->oldpix!=NULL) {
        free(h->oldpix);
        h->oldpix=NULL;
    }
    if (c=='i') {
        printf(" With this mode, you can find some values for a new specfile.\n");
        printf(" u and d moves picture up and down\n");
        printf(" l and r moves picture left and right\n");
        printf(" t toggles bpp\n");
        printf(" i gives this info\n");
        printf(" q quits\n");
    } else if (c=='u') {
        h->offy--;
    } else if (c=='d') {
        h->offy++;
    } else if (c=='l') {
        h->offx--;
    } else if (c=='r') {
        h->offx++;
    } else if (c=='t') {
        if (h->bpp==12) h->bpp=16; else h->bpp=12;
    }
    printf(" pressed\nCurrent settings: offx=%i offy=%i bpp=%i\n",h->offx,h->offy,h->bpp);
    sendpic(h,what);
    c=getchar();
    }
    printf("K, bye.");
    tcsetattr(0, TCSANOW, &oldterm);
}
#endif /* !_WIN32 */

int main(int argc, char **argv) {
    st2205_handle *h;
    if (argc<2) {
#ifdef _WIN32
#define SAMPLE_DEVICE "\\\\.\\PhysicalDrive#"
#else
#define SAMPLE_DEVICE "/dev/sdX"
#endif
    printf("Usage:\n");
    printf(" %s " SAMPLE_DEVICE " [-upload] pic.png\n",argv[0]);
    printf("  sends a picture to the screen\n");
    printf(" %s " SAMPLE_DEVICE " -backlight on|off\n",argv[0]);
    printf("  for backlight control\n");
    printf(" %s " SAMPLE_DEVICE " -lcd sleep|wake\n",argv[0]);
    printf("  for LCD control\n");
#ifndef _WIN32
    printf(" %s " SAMPLE_DEVICE " -test pic.png\n",argv[0]);
    printf("  Test-mode: interactively find out values for the spec-file\n");
#endif
    exit(0);
    }

    h=st2205_open(argv[1]);
    if (h==NULL) {
    printf("Open failed!\n");
    exit(1);
    }
    printf("Found device: %ix%i, %i bpp\n",h->width,h->height,h->bpp);

    if (strcmp(argv[2],"-backlight")==0) {
        if (strcmp(argv[3],"off")==0) {
            st2205_backlight(h, 0);
        } else if (strcmp(argv[3],"on")==0) {
            st2205_backlight(h, 1);
        }
    } else if (strcmp(argv[2],"-lcd")==0) {
        if (strcmp(argv[3],"sleep")==0) {
            st2205_lcd_sleep(h, 1);
        } else if (strcmp(argv[3],"wake")==0) {
            st2205_lcd_sleep(h, 0);
        }
    } else if (strcmp(argv[2],"-upload")==0) {
        sendthis(h,argv[3]);
#ifndef _WIN32
    } else if (strcmp(argv[2],"-test")==0) {
        testpic(h,argv[3]);
#endif
    } else {
        //argument isn't recognized; try to send it as a file or dir.
        sendthis(h,argv[2]);
    }
    st2205_close(h);
    return(0);
}

