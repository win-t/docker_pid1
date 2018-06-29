all: docker_pid1

docker_pid1: main.c Dockerfile.build
	docker build --pull -t docker_pid1_static_builder -f Dockerfile.build .
	docker run --rm docker_pid1_static_builder | tar x
	docker rmi --no-prune docker_pid1_static_builder

clean:
	rm -f docker_pid1

.PHONY: all clean
