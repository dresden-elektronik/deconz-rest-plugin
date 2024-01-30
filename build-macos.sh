#!/usr/bin/env sh

# Helper script to build libde_rest_plugin.dylib
# that can be copied into deCONZ.app/Contents/Plugins.

MACHINE=$(uname -m)
QTLOC=/usr/local/opt/qt@5

if [[ "$MACHINE" != "x86_64" ]]; then
	echo "building currently only supported on x86_64"
	exit 1
fi

if [ ! -d "$QTLOC" ]; then
	echo "Homebrew (x86_64) or Qt5 not installed in $QTLOC"
	exit 1
fi

rm -fr build

cmake -Wno-dev -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=$QTLOC -G Ninja -B build .
cmake --build build

pushd build

for i in `otool -L libde_rest_plugin.dylib | grep /opt/qt | cut -f1 -d' '`
do
	# fixup framework paths
	# /usr/local/opt/qt@5/lib/         QtWidgets.framework/Versions/5/QtWidgets
    # @executable_path/../Frameworks/  QtWidgets.framework/Versions/5/QtWidgets
	newpath=`echo $i | awk '{gsub(/.*qt.*\/lib/,"@executable_path/../Frameworks");}1'`
	install_name_tool -change $i $newpath libde_rest_plugin.dylib
done

popd
