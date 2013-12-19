require 'ffi'

module ST2205
  extend FFI::Library
  ffi_lib 'libst2205.so.2'

  class ST2205_handle < FFI::Struct
    layout :fd, :int,
      :width, :uint,
      :height, :uint,
      :bpp, :int,
      :proto, :int,
      :buff, :pointer,
      :oldpix, :pointer,
      :offx, :int,
      :offy, :int
  end

  # Opens the device pointed to by dev (which is /dev/sdX) and reads its
  # capabilities. Returns handle.
  attach_function :open, :st2205_open, [ :string ], ST2205_handle.by_ref

  # Close and free the info associated with h
  attach_function :close, :st2205_close, [ ST2205_handle.by_ref ], :void

  # Send an array of h->width*h->height r,g,b triplets.
  attach_function :send_data, :st2205_send_data,
                  [ ST2205_handle.by_ref, :buffer_in ], :void

  # Send part of an array of h->width*h->height r,g,b triplets.
  attach_function :send_partial, :st2205_send_partial,
                  [ ST2205_handle.by_ref, :buffer_in,
                  :int, :int, :int, :int ], :void

  # Turn the backlight on or off
  attach_function :backlight, :st2205_backlight,
                  [ ST2205_handle.by_ref, :int ], :void

  # Put the LCD to sleep or wake it
  attach_function :lcd_sleep, :st2205_lcd_sleep,
                  [ ST2205_handle.by_ref, :int ], :void
end

# This is for testing only
if __FILE__ == $0
  handle = ST2205.open "/dev/disk/by-id/usb-SITRONIX_MULTIMEDIA-0:0"
  ST2205.backlight handle, 0
  ST2205.backlight handle, 1
  puts handle[:width]
  ST2205.close handle
end
