all: docker_pid1 ignoresigchld

docker_pid1: main.c Dockerfile.build
	docker build --pull -t docker_pid1_static_builder -f Dockerfile.build .
	docker run --rm docker_pid1_static_builder docker_pid1 | tar x
	docker rmi --no-prune docker_pid1_static_builder

ignoresigchld: ignoresigchld.c Dockerfile.build
	docker build --pull -t docker_pid1_static_builder -f Dockerfile.build .
	docker run --rm docker_pid1_static_builder ignoresigchld | tar x
	docker rmi --no-prune docker_pid1_static_builder

clean:
	rm -f docker_pid1 ignoresigchld

.PHONY: all clean
