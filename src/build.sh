#!/bin/sh

set -e

top_dir=`dirname "$0"`
build_dir=$top_dir/../build
app_dir=$build_dir/iPodDisk.app
library_dir=$app_dir/Contents/Resources/Library


echo "Preparing build directory..."
cd "$build_dir" && rm -rf * && cd -
mkdir "$app_dir"
ditto "$top_dir/iPodDisk.app" "$app_dir"
cd "$build_dir"
echo "Removing .svn directories..."
find iPodDisk.app -type d -name '*.svn' | xargs rm -rf --
mkdir tmp
cd -

cd "$top_dir"

echo "Building ipoddisk binary..."
make clean
make

echo "Changing library bindings..."
for i in libgobject-2.0.dylib libgpod.1.dylib libglib-2.0.dylib libintl.8.dylib libiconv.2.dylib
do
  install_name_tool -change /opt/local/lib/$i @executable_path/../Resources/Library/$i ipoddisk
  # note: $i is symlink, but 'cp' by default follows symlinks
  cp /opt/local/lib/$i .
  install_name_tool -id @executable_path/../Resources/Library/$i $i
  # the libs reference each other, fix these references too
  for j in libgobject-2.0.dylib libgpod.1.dylib libglib-2.0.dylib libintl.8.dylib libiconv.2.dylib
  do
    install_name_tool -change /opt/local/lib/$j @executable_path/../Resources/Library/$j $i
  done
  mv $i "../$library_dir"
done

cd -

cp "$top_dir/ipoddisk" "$app_dir/Contents/MacOS/ipoddiskfuse"
