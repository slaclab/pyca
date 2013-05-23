libnames := pyca xtcrdr

libsrcs_pyca := pyca.cc
libincs_pyca := python/include/python2.5 \
		python/include/python2.7 \
                epics/include \
                epics/include/os/Linux
liblibs_pyca := epics/ca \
                epics/Com

libsrcs_xtcrdr := xtcrdr.cc
libincs_xtcrdr := python/include/python2.5 \
		  python/include/python2.7 \
		  daq \
                  epics/include \
                  epics/include/os/Linux
liblibs_xtcrdr := epics/ca \
                  epics/Com \
	  	  daq/xtcdata
