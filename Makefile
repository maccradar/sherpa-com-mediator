all: proxy

% : %.c
	gcc -ggdb $< -Irfsmbinding -I/usr/include/lua5.1 -Linstall/lib -lczmq -lzmq -lzyre -lrfsmbinding -llua5.1 -ljansson -lcurl -o $@
