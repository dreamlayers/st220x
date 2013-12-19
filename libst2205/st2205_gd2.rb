require "ffi"
require "st2205"
require "gd2-ffij"
include GD2

class ExtLCD
    attr_accessor :image

    def initialize filename = "/dev/disk/by-id/usb-SITRONIX_MULTIMEDIA-0:0"
        @handle = ST2205.open filename
        if @handle.to_ptr == nil
            raise "Failed to open photo frame."
        end
        @image = Image::TrueColor.new(width, height)
        @buffer = FFI::MemoryPointer.new :uchar, width * height * 3
    end

    def close
        ST2205.close @handle
    end

    def width
        @handle[:width]
    end

    def height
        @handle[:height]
    end

    def backlight on
        if on
            ST2205.backlight @handle, 1
        else
            ST2205.backlight @handle, 0
        end
    end

    def update
        ptr = 0
        height.times do |i|
            width.times do |j|
                @buffer.put_uchar(ptr, @image[j,i].red)
                ptr += 1
                @buffer.put_uchar(ptr, @image[j,i].green)
                ptr += 1
                @buffer.put_uchar(ptr, @image[j,i].blue)
                ptr += 1
            end
        end
        ST2205.send_data @handle, @buffer
    end
end

# This is for testing only
if __FILE__ == $0
    lcd = ExtLCD.new
    lcd.backlight false
    puts lcd.width
    lcd.image.draw do |pen|
        pen.color = lcd.image.palette.resolve Color[1.0, 0.75, 0.5]
        pen.thickness = 2
        pen.move_to 25, 50
        pen.line_to 175, 50
        pen.move -150, 25
        pen.font = Font::TrueType['/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf', 20]
        pen.text 'Hello, world!'
    end
    lcd.update
    lcd.backlight true
    lcd.close
end
