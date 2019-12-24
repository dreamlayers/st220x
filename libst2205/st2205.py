from ctypes import *
from PIL import Image

# Handle definition for the st2205_* routines
class st2205_handle(Structure):
    _fields_ = [("fd", c_int),
                ("width", c_uint),
                ("height", c_uint),
                ("bpp", c_int),
                ("proto", c_int),
                ("buff", POINTER(c_char)),
                ("oldpix", POINTER(c_ubyte)),
                ("offx", c_int),
                ("offy", c_int)
               ]

l = cdll.LoadLibrary("libst2205.so.2")

st2205_open = CFUNCTYPE(POINTER(st2205_handle), c_char_p) \
              (("st2205_open", l))

st2205_close = CFUNCTYPE(None, POINTER(st2205_handle)) \
               (("st2205_close", l))

st2205_send_data = CFUNCTYPE(None, POINTER(st2205_handle), POINTER(c_ubyte)) \
                   (("st2205_send_data", l))

st2205_send_partial = CFUNCTYPE(None, POINTER(st2205_handle), POINTER(c_ubyte),
                                c_int, c_int, c_int, c_int) \
                      (("st2205_send_partial", l))

st2205_backlight = CFUNCTYPE(None, POINTER(st2205_handle), c_int) \
                   (("st2205_backlight", l))

st2205_lcd_sleep = CFUNCTYPE(None, POINTER(st2205_handle), c_int) \
                   (("st2205_lcd_sleep", l))

class ST2205:
    def __init__(self, dev = '/dev/disk/by-id/usb-SITRONIX_MULTIMEDIA-0:0'):
        self.h = st2205_open(dev)
        self.i = None
        assert self.h

    def close(self):
        st2205_close(self.h)
        self.h = None

    def backlight(self, on):
        st2205_backlight(self.h, 1 if on else 0)

    def lcd_sleep(self, sleep):
        st2205_lcd_sleep(self.h, 1 if sleep else 0)

    def get_image(self):
        if self.i is None:
            self.i = Image.new("RGB",
                               (self.h.contents.width, self.h.contents.height),
                               "black")
        return self.i

    def update_rgba(self, data):
        b = (c_ubyte * (self.h.contents.width * self.h.contents.height * 3))()
        for i in range(0, self.h.contents.width * self.h.contents.height):
            b[i*3 + 0] = ord(data[i*4 + 2])
            b[i*3 + 1] = ord(data[i*4 + 1])
            b[i*3 + 2] = ord(data[i*4 + 0])
        st2205_send_data(self.h, cast(c_char_p(addressof(b)),
                                      POINTER(c_ubyte)))

    def update_rgba_part(self, data, xs, ys, xe, ye):
        b = (c_ubyte * (self.h.contents.width * self.h.contents.height * 3))()
        pixidx = self.h.contents.width * ys + xs
        pixskip = (self.h.contents.width - (xe - xs + 1))
        bidx = pixidx * 3
        bskip = pixskip * 3
        didx = pixidx * 4
        dskip = pixskip * 4
        for y in range(ys, ye + 1):
            for x in range(xs, xe + 1):
                b[bidx + 0] = ord(data[didx + 2])
                b[bidx + 1] = ord(data[didx + 1])
                b[bidx + 2] = ord(data[didx + 0])
                bidx += 3
                didx += 4
            bidx += bskip
            didx += dskip
        st2205_send_partial(self.h, cast(c_char_p(addressof(b)),
                                         POINTER(c_ubyte)),
                            xs, ys, xe, ye)

    def update(self):
        st2205_send_data(self.h, cast(c_char_p(self.i.tobytes()),
                                      POINTER(c_ubyte)))

    def update_part(self, xs, ys, xe, ye):
        st2205_send_partial(self.h, cast(c_char_p(self.i.tobytes()),
                                         POINTER(c_ubyte)),
                            xs, ys, xe, ye)
