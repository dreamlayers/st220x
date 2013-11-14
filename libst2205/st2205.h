#ifndef _ST2205_H_
#define _ST2205_H_

//Handle definition for the st2205_* routines
typedef struct {
       int fd;
       unsigned int width;
       unsigned int height;
       int bpp;
       int proto;
       char* buff;
       unsigned char* oldpix;
       int offx;
       int offy;
} st2205_handle;

/*
 Opens the device pointed to by dev (which is /dev/sdX) and reads its
 capabilities. Returns handle.
 */
st2205_handle *st2205_open(const char *dev);


/*
    Close and free the info associated with h
*/
void st2205_close(st2205_handle *h);

/*
 Send an array of h->width*h->height r,g,b triplets.
 */
void st2205_send_data(st2205_handle *h, unsigned char *pixinfo);

/*
 Send part of an array of h->width*h->height r,g,b triplets.
 */
void st2205_send_partial(st2205_handle *h, unsigned char *pixinfo, int xs, int ys, int xe, int ye);

/*
Turn the backlight on or off
*/
void st2205_backlight(st2205_handle *h, int on);

/*
Put the LCD to sleep or wake it
*/
void st2205_lcd_sleep(st2205_handle *h, int sleep);

#endif /* #ifndef _ST2205_H_ */
