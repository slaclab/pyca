libnames := pyca

libsrcs_pyca := pyca.cc
libincs_pyca := python/include/python2.5 \
		python/include/python2.7 \
                epics/include \
                epics/include/os/Linux
liblibs_pyca := epics/ca \
                epics/Com
