PATH="$PATH:/usr/libexec"

mkdir -p ShitStation.app/Contents/MacOS/Libraries
mv build/ShitStation ShitStation.app/Contents/MacOS
chmod a+x ShitStation.app/Contents/MacOS/ShitStation

PlistBuddy ShitStation.app/Contents/Info.plist -c "add CFBundleDisplayName string ShitStation"
PlistBuddy ShitStation.app/Contents/Info.plist -c "add CFBundleIconName string AppIcon"
PlistBuddy ShitStation.app/Contents/Info.plist -c "add CFBundleIconFile string AppIcon"
PlistBuddy ShitStation.app/Contents/Info.plist -c "add NSHighResolutionCapable bool true"
PlistBuddy -c "Set :LSMinimumSystemVersion 10.15" ShitStation.app/Contents/Info.plist
PlistBuddy ShitStation.app/Contents/version.plist -c "add ProjectName string ShitStation"

dylibbundler -od -b -x ShitStation.app/Contents/MacOS/ShitStation -d ShitStation.app/Contents/Frameworks/ -p @rpath
install_name_tool -add_rpath @loader_path/../Frameworks ShitStation.app/Contents/MacOS/ShitStation

