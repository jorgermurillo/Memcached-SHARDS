# Memcached-SHARDS

This is a modified version of Memcached that uses SHARDS (https://github.com/jorgermurillo/SHARDS-C) to monitor the MRC of each slab.

This work was done jointly with Gustavo Totoy (https://github.com/gtotoy) as part of a larger project for Dr. Cristina Abad (https://sites.google.com/site/cristinaabad/). 

 :rotating_light:  :rotating_light:  :rotating_light: ## WARNING  :rotating_light:  :rotating_light:  :rotating_light: This repository might be merged with Gustavo's fork of Memcached (https://github.com/gtotoy/memcached) in the future.

This work was funded in part by a Google Faculty Research Award.

After installing zeromq, if you get an error like:

	Error: libzmq.so.5: cannot open shared object file: No such file or directory

try typing

 	$ export LD_LIBRARY_PATH=/usr/local/lib

 on the terminal.

## Dependencies

* libevent, http://www.monkey.org/~provos/libevent/ (libevent-dev)

## Environment

### Linux

If using Linux, you need a kernel with epoll.  Sure, libevent will
work with normal select, but it sucks.

epoll isn't in Linux 2.4, but there's a backport at:

    http://www.xmailserver.org/linux-patches/nio-improve.html

You want the epoll-lt patch (level-triggered).

