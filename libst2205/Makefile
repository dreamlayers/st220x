CC	=	gcc
SRC	=	st2205.c
OBJ	=	st2205.o
HEADERS	=	st2205.h
CFLAGS	=	-W -Wall -Wmissing-prototypes -g -fPIC -O2
LIBS	=	
TARGET	=	libst2205.so.2

PREFIX	=	/usr/local
LIBDIR	=	$(PREFIX)/lib
RUBYDIR	=	$(LIBDIR)/site_ruby
HDRDIR	=	$(PREFIX)/include

$(TARGET):	$(OBJ) $(SRC) $(HEADERS)
	ar -rv libst2205.a $(OBJ) 
	$(CC) $(LDFLAGS) -shared -Wl,-soname,$@ -o $@ $(OBJ) $(LIBS)

.PHONY : clean
clean:	
	rm -f $(OBJ) test *.a *.so

.PHONY : install
install: $(TARGET) $(HEADERS)
	test -z "$(LIBDIR)" || /bin/mkdir -p "$(LIBDIR)"
	test -z "$(HDRDIR)" || /bin/mkdir -p "$(HDRDIR)"
	test -z "$(RUBYDIR)" || /bin/mkdir -p "$(RUBYDIR)"
	install $(TARGET) $(LIBDIR)
	install $(HEADERS) $(HDRDIR)
	install st2205.rb $(RUBYDIR)
	install st2205_gd2.rb $(RUBYDIR)
	ldconfig
