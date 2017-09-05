all: docker_pid1 docker_pid1_static

docker_pid1: main.c
	gcc -Os -o $@ $<
	strip -s $@

docker_pid1_static: main.c Dockerfile.build
	docker build -t docker_pid1_static_builder -f Dockerfile.build .
	docker run --rm docker_pid1_static_builder | tar x

clean:
	rm -f docker_pid1 docker_pid1_static

.PHONY: all clean
