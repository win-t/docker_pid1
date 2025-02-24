all: docker_pid1_amd64 docker_pid1_aarch64

docker_pid1_amd64: main.c Dockerfile.build
	docker build --platform linux/amd64 --pull -t docker_pid1_static_builder -f Dockerfile.build .
	docker run --rm docker_pid1_static_builder docker_pid1 | tar x
	docker rmi --no-prune docker_pid1_static_builder
	mv docker_pid1 docker_pid1_amd64
	rm -f docker_pid1

docker_pid1_aarch64: main.c Dockerfile.build
	docker build --platform linux/aarch64 --pull -t docker_pid1_static_builder -f Dockerfile.build .
	docker run --rm docker_pid1_static_builder docker_pid1 | tar x
	docker rmi --no-prune docker_pid1_static_builder
	mv docker_pid1 docker_pid1_aarch64
	rm -f docker_pid1

clean:
	rm -f docker_pid1_amd64 docker_pid1_aarch64

.PHONY: all clean
