python setup.py build
LIB=`find build -name pyca.so`
LINK='pyca.so'
rm $LINK
if [ -z $LIB ]; then
  exit
fi
ln -s $LIB $LINK
