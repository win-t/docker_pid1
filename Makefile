docker_pid1: main.c
	gcc -Os -fdata-sections -ffunction-sections -Wl,--gc-sections -static -o $@ $<
	strip -s $@

clean:
	rm -f docker_pid1

.PHONY: clean
