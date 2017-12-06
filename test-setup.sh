rm -rf build
python setup.py build
LIB=`find build -name pyca*.so`
LINK='pyca.so'
if [ -L $LINK ]; then
  rm $LINK
fi
if [ -z $LIB ]; then
  exit
fi
ln -s $LIB $LINK
