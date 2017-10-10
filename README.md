# Memcached-SHARDS

This is a modified version of Memcached that uses SHARDS (https://github.com/jorgermurillo/SHARDS-C) to monitor the MRC of each slab.

This work was done jointly with Gustavo Totoy (https://github.com/gtotoy) as part of a larger project for Dr. Cristina Abad (https://sites.google.com/site/cristinaabad/). 

 :rotating_light:  :rotating_light:  :rotating_light: 
 ## WARNING  
 :rotating_light:  :rotating_light:  :rotating_light: 

 This repository might be merged with Gustavo's fork of Memcached (https://github.com/gtotoy/memcached) in the future.


After installing zeromq, if you get an error like:

	Error: libzmq.so.5: cannot open shared object file: No such file or directory

try typing

 	$ export LD_LIBRARY_PATH=/usr/local/lib

 on the terminal.

## Referencing our work
If you found our tool useful and use it in research, please cite our work as follows:

Instrumenting cloud caches for online workload monitoring
Jorge R. Murillo, Gustavo Totoy, Cristina L. Abad
16th Workshop on Adaptive and Reflective Middleware (ARM), co-located with ACM/IFIP/USENIX Middleware, 2017.

## Acknowledgements

This work was funded in part by a Google Faculty Research Award.

This work was possible thanks to the Amazon Web Services Cloud Credits for Research program.
