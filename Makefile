all: sherpa_comm_mediator local_component

% : %.c
	gcc -ggdb $< -Linstall/lib -lczmq -lzmq -lzyre -ljansson -luuid -o $@
