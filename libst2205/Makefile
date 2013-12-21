CC	=	gcc
SRC	=	st2205.c
OBJ	=	st2205.o
HEADERS	=	st2205.h
CFLAGS	=	-W -Wall -Wmissing-prototypes -g -fPIC -O2
LIBS	=	
TARGET	=	libst2205.so.2
LNNAME	=	libst2205.so

PREFIX	=	/usr/local
LIBDIR	=	$(PREFIX)/lib
RUBYDIR	=	$(LIBDIR)/site_ruby
HDRDIR	=	$(PREFIX)/include

$(TARGET):	$(OBJ) $(SRC) $(HEADERS)
	ar -rv libst2205.a $(OBJ) 
	$(CC) $(LDFLAGS) -shared -Wl,-soname,$@ -o $@ $(OBJ) $(LIBS)
	rm -f $(LNNAME)
	ln -s $(TARGET) $(LNNAME)

.PHONY : clean
clean:	
	rm -f $(OBJ) libst2205.a $(LNNAME) $(TARGET)

.PHONY : install
install: $(TARGET) $(HEADERS)
	test -z "$(LIBDIR)" || /bin/mkdir -p "$(LIBDIR)"
	test -z "$(HDRDIR)" || /bin/mkdir -p "$(HDRDIR)"
	test -z "$(RUBYDIR)" || /bin/mkdir -p "$(RUBYDIR)"
	install $(TARGET) $(LIBDIR)
	rm -f $(LIBDIR)/$(LNNAME)
	ln -s $(TARGET) $(LIBDIR)/$(LNNAME)
	install $(HEADERS) $(HDRDIR)
	install -m 644 st2205.rb $(RUBYDIR)
	install -m 644 st2205_gd2.rb $(RUBYDIR)
	ldconfig
