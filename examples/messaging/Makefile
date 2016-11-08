	
# define the C compiler to use
CC = gcc

# define any compile-time flags
CFLAGS = -Wall -ggdb

# define any directories containing header files other than /usr/include
INCLUDES = -I../include

# define library paths in addition to /usr/lib
LFLAGS =  -L../lib

# define any libraries to link into executable:
LIBS = -lczmq -lzmq -lzyre -ljansson -luuid

# define the C source files
SRCS = messaging_example.c

# define the C object files 
OBJS = $(SRCS:.c=.o)

# define the executable file 
MAIN = messaging_example 


.PHONY: depend clean

all:    messaging_example 
	@echo all examples have been compiled

$(MAIN): $(OBJS) 
	$(CC) $(CFLAGS) $(INCLUDES) -o $(MAIN) $(OBJS) $(LFLAGS) $(LIBS)

#messaging_unknown_recipient: messaging_unknown_recipient.o
#	$(CC) $(CFLAGS) $(INCLUDES) -o $@ messaging_unknown_recipient.o $(LFLAGS) $(LIBS)

#messaging_unknown_recipient.o: messaging_unknown_recipient.c
#	$(CC) $(CFLAGS) $(INCLUDES) messaging_unknown_recipient.c $(LFLAGS) $(LIBS)

# this is a suffix replacement rule for building .o's from .c's
%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $<  -o $@

clean:
	$(RM) *.o *~ $(MAIN)

depend: $(SRCS)
	makedepend $(INCLUDES) $^

# DO NOT DELETE THIS LINE -- make depend needs it