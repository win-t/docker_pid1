all: docker_pid1 docker_pid1_static

docker_pid1: main.c
	gcc -Os -o $@ $<
	strip -s $@

docker_pid1_static: main.c
	gcc -Os -fdata-sections -ffunction-sections -Wl,--gc-sections -static -o $@ $<
	strip -s $@

clean:
	rm -f docker_pid1 docker_pid1_static

.PHONY: all clean
