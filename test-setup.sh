mkdir -p setup-py-tst
python setup.py build
LIB=`find build -name pyca.so`
LINK='setup-py-tst/pyca.so'
rm $LINK
ln -s $LIB $LINK
