/*
    Simple, QaD tool to splice a file into another file
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
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

static char *progname = NULL;

static void pfatal(const char *msg) {
    fprintf(stderr, "%s: ", progname);
    perror(msg);
    exit(1);
}

static void fatal(const char *msg) {
    fprintf(stderr, "%s: %s\n", progname, msg);
    exit(1);
}

int main(int argc, char **argv) {
    int f1,f2,r;
    long size, size2;
    unsigned int offset;
    char *buff;
    struct stat sbuff;

    progname = argv[0];

    if (argc!=4) {
        printf("Usage: %s file1 file2 offset\n"
               "Overwrites the byte in file1 with"
               "those of file2, starting at offset.\n", argv[0]);
        exit(0);
    }

    r=stat(argv[1],&sbuff);
    if (r!=0)
        pfatal("Couldn't stat file1");

    size=sbuff.st_size;
    buff=malloc(size);
    if (buff==NULL)
        fatal("Couldn't malloc bytes for file1");

    f1=open(argv[1],O_RDONLY);
    if (f1<0)
        pfatal("Couldn't open file1");

    if (read(f1,buff,size) != size)
        pfatal("Couldn't read file1");

    if (close(f1) != 0)
        pfatal("Error while closing file1");

    offset= strtol(argv[3], (char **)NULL, 0);
    if (offset>size)
        fatal("Offset bigger than size of file!");
    printf("Splicing in at offset 0x%X.\n",offset);

    r=stat(argv[2],&sbuff);
    if (r!=0)
        pfatal("Couldn't stat file2");
    size2=sbuff.st_size;

    f2=open(argv[2],O_RDONLY);
    if (f2<0)
        pfatal("Couldn't open file2");

    if (read(f2,buff+offset,size) != size2)
        pfatal("Couldn't read file2");

    if (close(f2) != 0)
        pfatal("Error while closing file2");

    f2=open(argv[1],O_CREAT|O_TRUNC|O_WRONLY,444);
    if (f2<0)
        pfatal("Couldn't open file1 for writing");

    if (write(f2,buff,size) != size)
        pfatal("Couldn't write to file2");

    if (close(f2) != 0)
        pfatal("Error while closing file2 after writing");

    printf("All done. Thank you, come again.\n");
    exit(0);
}
