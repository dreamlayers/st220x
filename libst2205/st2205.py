from ctypes import *

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

st2205_rgba = CFUNCTYPE(None, POINTER(st2205_handle), POINTER(c_ubyte)) \
              (("st2205_rgba", l))

st2205_rgba_partial = CFUNCTYPE(None, POINTER(st2205_handle), POINTER(c_ubyte),
                      c_int, c_int, c_int, c_int) \
                      (("st2205_rgba_partial", l))

st2205_backlight = CFUNCTYPE(None, POINTER(st2205_handle), c_int) \
                   (("st2205_backlight", l))

st2205_lcd_sleep = CFUNCTYPE(None, POINTER(st2205_handle), c_int) \
                   (("st2205_lcd_sleep", l))

class ST2205:
    def __init__(self, dev = '/dev/disk/by-id/usb-SITRONIX_MULTIMEDIA-0:0'):
        self.h = st2205_open(dev.encode('utf-8'))
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
        from PIL import Image
        if self.i is None:
            self.i = Image.new("RGB",
                               (self.h.contents.width, self.h.contents.height),
                               "black")
        return self.i

    def update_rgba(self, data):
        st2205_rgba(self.h, cast(c_char_p(data), POINTER(c_ubyte)))

    def update_rgba_part(self, data, xs, ys, xe, ye):
        st2205_rgba_partial(self.h, cast(c_char_p(data), POINTER(c_ubyte)),
                            xs, ys, xe, ye)

    def update(self):
        st2205_send_data(self.h, cast(c_char_p(self.i.tobytes()),
                                      POINTER(c_ubyte)))

    def update_part(self, xs, ys, xe, ye):
        st2205_send_partial(self.h, cast(c_char_p(self.i.tobytes()),
                                         POINTER(c_ubyte)),
                            xs, ys, xe, ye)
