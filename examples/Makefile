all: encoder observer line_controller

% : %.c
	gcc -ggdb $< -I../rfsmbinding -I/usr/include/lua5.1 -L../install/lib -lczmq -lzmq -lzyre -lrfsmbinding -llua5.1 -ljansson -lcurl -o $@
