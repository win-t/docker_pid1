# docker_pid1
This program is PID 1 inside container (not only for docker)

## Why we need this kind of program ?
you can see the explanation [here](https://blog.phusion.nl/2015/01/20/docker-and-the-pid-1-zombie-reaping-problem/)

## Why I created a new one ?
Yes, there are other tools like [tini](https://github.com/krallin/tini), but that tool is too complex in my opinion. I just want to follow UNIX Philosophy *Do One Thing and Do It Well*

## How to use
Well, it's very simple, just run

    docker_pid1 <the_program> [arg1] [arg2] [... argN]
