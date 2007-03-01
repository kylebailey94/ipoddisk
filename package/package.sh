#!/bin/sh

set_window_design () {
osascript <<SETWINDOWDESIGN
set iPodDisk to "iPodDisk-$version"
tell application "Finder"
	set f to disk iPodDisk
	set w to container window of f
	set current view of w to icon view
	set opts to icon view options of w
	open w
	set toolbar visible of w to false
	set the icon size of opts to 88
	set arrangement of opts to not arranged
	set position of item "iPodDisk" of w to {139, 134}
	set position of item "Applications" of w to {378, 134}
	set position of item "README.txt" of w to {79, 330}
	set position of item "COPYING.txt" of w to {450, 330}
	set position of item "Contribute to iPodDisk.webloc" of w to {262, 330}
	set bounds of w to {310, 87, 838, 536}
end tell
SETWINDOWDESIGN
}

set_background_window_design () {
osascript <<SETDMGBACKGROUND 
	set iPodDisk to "iPodDisk-$version"
	tell application "Finder"
		set f to disk iPodDisk
		set w to container window of f
		set opts to icon view options of w
		set bgFile to "/Volumes/" & iPodDisk & "/.background/background.png"
		set background picture of opts to (get POSIX file bgFile as string) -- impossible to do that with Panther ...
	end tell	
SETDMGBACKGROUND
}

if [ -z "$1"  ]
then
	echo "ERROR : No iPodDisk version number"
	echo "Usage : bash <full_path_to_package.sh>/package.sh iPodDisk_version_number"
	exit -1
fi

version="$1" 
package_dir=`dirname "$0"`
build_dir="$package_dir/../build"

if [ -f "$build_dir/tmp.dmg" ] 
then
	tmp_mounted=`mount | grep "iPodDisk-$version" | awk '{print $1;}'`
	if [ ! -z "$tmp_mounted" ]
	then
		hdiutil detach -quiet "$tmp_mounted"
	fi
	echo "Delete the old tmp.dmg..."
	rm "$build_dir/tmp.dmg"
fi

if [ -f "$build_dir/iPodDisk-$version.dmg" ] 
then
	iPodDisk_mounted=`mount | grep "iPodDisk-$version" | awk '{print $1;}'`
	if [ ! -z "$iPodDisk_mounted" ]
	then
		hdiutil detach -quiet "$iPodDisk_mounted"
	fi
	echo "Delete the old iPodDisk-$version.dmg..."
	rm "$build_dir/iPodDisk-$version.dmg"
fi

echo "Making temp disk image..."
hdiutil create -quiet "$build_dir/tmp.dmg" -size 5m -fs HFS+ -volname "iPodDisk-$version"
dmgvol=`hdid "$build_dir/tmp.dmg" | grep Apple_HFS | awk '{print $3;}'`

echo "Copy files in the temp disk image..."
cp -R "$build_dir/iPodDisk.app" "$dmgvol/"
mkdir "$dmgvol/.background"
cp $package_dir/background.png "$dmgvol/.background/"
cp $package_dir/*.txt "$dmgvol/"
cp $package_dir/*.webloc "$dmgvol/"
ln -s /Applications "$dmgvol/Applications"

echo "Window design..."
set_window_design > /dev/null
# check OS X version because the Panther Finder Dictionnary doesn't set the bacground picture
#uname -a | grep 'Darwin Kernel Version 7.*'
sw_vers -productVersion | grep '^10\.4'  > /dev/null
if [ $? -eq 0 ]
then
	echo "Please set the background picture with Finder Presentation Options to .background/background.png"
	echo "To do this :"
	echo " 1. In Finder, go to the Finder Presentation Options (Action-J)"
	echo " 2. Select Picture, click on the Choose button."
	echo " 3. Use the \"Go to folder\" command (Action-Shift-G) and type (or copy) \"/Volumes/iPodDisk-$version/.background/\""
	echo " 4. Type on ENTER and click on the Go button"
	echo " 3. Select the background.png file and click on the select button (or type on ENTER)"
	echo -n "Press a touch when you are ready to continue ..."
	read input
else
	set_background_window_design > /dev/null
fi

echo "Fix iPodDisk.app permissions..."
app_dir=$dmgvol/iPodDisk.app
chmod -R a+r "$app_dir"
find -X "$app_dir" -type d | xargs chmod a+x
chmod a+rx $app_dir/Contents/MacOS/*
chmod a+rx $app_dir/Contents/Resources/Library/*

echo "Create the iPodDisk-$version compressed disk image..."
tmp_mounted=`mount | grep "iPodDisk-$version" | awk '{print $1;}'`
hdiutil detach -quiet "$tmp_mounted"
hdiutil convert -quiet $build_dir/tmp.dmg -format UDZO -o $build_dir/iPodDisk-$version.dmg
rm "$build_dir/tmp.dmg"
open "$build_dir/iPodDisk-$version.dmg"
