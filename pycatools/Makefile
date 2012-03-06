all: carec carecdump buildev

carec: carec.c
	gcc -o carec -O3 carec.c -I /reg/g/pcds/package/epics/3.14/base/current/include \
	    -I /reg/g/pcds/package/epics/3.14/base/current/include/os/Linux \
	    -L /reg/g/pcds/package/epics/3.14/base/current/lib/linux-x86_64 -lca -lCom

buildev: buildev.c
	gcc -o buildev -g buildev.c -I /reg/g/pcds/package/epics/3.14/base/current/include \
	    -I /reg/g/pcds/package/epics/3.14/base/current/include/os/Linux \
	    -L /reg/g/pcds/package/epics/3.14/base/current/lib/linux-x86_64 -lca -lCom

carecdump: carecdump.c
	gcc -o carecdump -O3 carecdump.c -I /reg/g/pcds/package/epics/3.14/base/current/include \
	    -I /reg/g/pcds/package/epics/3.14/base/current/include/os/Linux \
	    -L /reg/g/pcds/package/epics/3.14/base/current/lib/linux-x86_64 -lca -lCom

clean:
	-rm -f carec carecdump buildev
	-rm -f *.o
	-rm -f *~
