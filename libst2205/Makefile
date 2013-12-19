CC	=	gcc
SRC	=	st2205.c
OBJ	=	st2205.o
HEADERS	=	st2205.h
CFLAGS	=	-W -Wall -Wmissing-prototypes -g -fPIC -O2
LIBS	=	
TARGET	=	libst2205.so.2

$(TARGET):	$(OBJ) $(SRC) $(HEADERS)
	ar -rv libst2205.a $(OBJ) 
	$(CC) $(LDFLAGS) -shared -Wl,-soname,$@ -o $@ $(OBJ) $(LIBS)

clean:	
	rm -f $(OBJ) test *.a *.so

install: $(TARGET) $(HEADERS)
	test -z "/usr/local/lib" || /bin/mkdir -p "/usr/local/lib"
	test -z "/usr/local/include" || /bin/mkdir -p "/usr/local/include"
	install $(TARGET) /usr/local/lib
	install $(HEADERS) /usr/local/include
	ldconfig
