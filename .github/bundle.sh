PATH="$PATH:/usr/libexec"


mkdir -p ShitStation.app/Contents/MacOS/Libraries
mv build/ShitStation ShitStation.app/Contents/MacOS
chmod 777 ShitStation.app/Contents/MacOS/ShitStation

PlistBuddy ${APPROOT}/Contents/Info.plist -c "add CFBundleDisplayName string ShitStation"
PlistBuddy ${APPROOT}/Contents/Info.plist -c "add CFBundleIconName string AppIcon"
PlistBuddy ${APPROOT}/Contents/Info.plist -c "add CFBundleIconFile string AppIcon"
PlistBuddy ${APPROOT}/Contents/Info.plist -c "add NSHighResolutionCapable bool true"
PlistBuddy ${APPROOT}/Contents/version.plist -c "add ProjectName string ShitStation"

dylibbundler -od -b -x ShitStation.app/Contents/MacOS/ShitStation -d ShitStation.app/Contents/Frameworks/ -p @rpath
install_name_tool -add_rpath @loader_path/../Frameworks ShitStation.app/Contents/MacOS/ShitStation

#
## Create bundle filesystem
#mkdir -p psxe.app/Contents/MacOS/Libraries
#
## Move executable to folder
#mv bin/psxe psxe.app/Contents/MacOS
#
## Make executable
#chmod 777 psxe.app/Contents/MacOS/psxe
#
## Bundle required dylibs
#dylibbundler -b -x ./psxe.app/Contents/MacOS/psxe -d ./psxe.app/Contents/Libraries/ -p @executable_path/../Libraries/ -cd
#
## Move plist to Contents folder
#mv Info.plist psxe.app/Contents/Info.plist
