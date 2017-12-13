/* gcc -Wall -O3 st2205.c benchmark.c -o benchmark && time ./benchmark */

#include <stdlib.h>
#include <stdio.h>
#include "st2205.h"

int main(void)
{
    st2205_handle *h = NULL;
    unsigned char pixdata[320*240*3];
    int i;
    
    h = st2205_open("/dev/disk/by-id/usb-SITRONIX_MULTIMEDIA-0:0");
    if (h == NULL) {
        fprintf(stderr, "Error opening device\n");
        return -1;
    }

    for (i = 0; i < 10; i++) {
        st2205_send_partial(h, pixdata, 0, 0, 319, 239);
    }
    
    st2205_close(h);
    return 0;
}
