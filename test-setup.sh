mkdir -p setup-py-test
python setup.py build
LIB=`find build -name pyca.so`
LINK='setup-py-test/pyca.so'
rm $LINK
ln -s $LIB $LINK
