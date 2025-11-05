if [ "$CYGWIN" == "y" ]; then
echo "jansson-2.7 for Cygwin"
echo TARGET_FS:$TARGET_FS
./configure --prefix=$TARGET_FS --enable-shared=yes --enable-static=yes
#./configure --host=$CROSS_HOST --prefix=$TARGET_FS --enable-shared=yes --enable-static=yes
else
aclocal && autoreconf --force --install && ./configure --host=$CROSS_HOST --prefix=$PKG_INSTALL_DIR --enable-shared=no --enable-static=yes
fi