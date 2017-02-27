	
# define the C compiler to use
CC = gcc

# define any compile-time flags
CFLAGS = -Wall -ggdb

# define any directories containing header files other than /usr/include
INCLUDES = -I../include

# define library paths in addition to /usr/lib
LFLAGS =  -L../lib

# define any libraries to link into executable:
LIBS = -lczmq -lzmq -lzyre -ljansson

# define the C source files
SRCS = file_transfer_example.c

# define the C object files 
OBJS = $(SRCS:.c=.o)

# define the executable file 
MAIN = file_transfer_example


.PHONY: depend clean

all:    $(MAIN)
	@echo file_transfer_example has been compiled

$(MAIN): $(OBJS) 
	$(CC) $(CFLAGS) $(INCLUDES) -o $(MAIN) $(OBJS) $(LFLAGS) $(LIBS)

# this is a suffix replacement rule for building .o's from .c's
.c.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c $<  -o $@

clean:
	$(RM) *.o *~ $(MAIN)

depend: $(SRCS)
	makedepend $(INCLUDES) $^

# DO NOT DELETE THIS LINE -- make depend needs it