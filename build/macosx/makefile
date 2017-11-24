APPNAME=tic80
APPBUNDLE=$(APPNAME).app
APPBUNDLECONTENTS=$(APPBUNDLE)/Contents
APPBUNDLEEXE=$(APPBUNDLECONTENTS)/MacOS
APPBUNDLEICON=$(APPBUNDLECONTENTS)/Resources

all: dmg

dmg: appbundle
	hdiutil create -volname $(APPNAME) -srcfolder $(APPNAME).app -ov -format UDZO $(APPNAME).dmg

appbundle: $(APPNAME).icns
	rm -rf $(APPBUNDLE)
	mkdir $(APPBUNDLE)
	mkdir $(APPBUNDLE)/Contents
	mkdir $(APPBUNDLE)/Contents/MacOS
	mkdir $(APPBUNDLE)/Contents/Resources
	cp Info.plist $(APPBUNDLECONTENTS)/
	echo "APPL????" > $(APPBUNDLECONTENTS)/PkgInfo
	cp $(APPNAME).icns $(APPBUNDLEICON)/
	cp ../../bin/$(APPNAME) $(APPBUNDLEEXE)/$(APPNAME)

$(APPNAME).icns: $(APPNAME)Icon.png
	rm -rf $(APPNAME).iconset
	mkdir $(APPNAME).iconset
	sips -z 16 16     $(APPNAME)Icon.png --out $(APPNAME).iconset/icon_16x16.png
	sips -z 32 32     $(APPNAME)Icon.png --out $(APPNAME).iconset/icon_16x16@2x.png
	sips -z 32 32     $(APPNAME)Icon.png --out $(APPNAME).iconset/icon_32x32.png
	sips -z 64 64     $(APPNAME)Icon.png --out $(APPNAME).iconset/icon_32x32@2x.png
	sips -z 128 128   $(APPNAME)Icon.png --out $(APPNAME).iconset/icon_128x128.png
	sips -z 256 256   $(APPNAME)Icon.png --out $(APPNAME).iconset/icon_128x128@2x.png
	sips -z 256 256   $(APPNAME)Icon.png --out $(APPNAME).iconset/icon_256x256.png
	sips -z 512 512   $(APPNAME)Icon.png --out $(APPNAME).iconset/icon_256x256@2x.png
	sips -z 512 512   $(APPNAME)Icon.png --out $(APPNAME).iconset/icon_512x512.png
	cp $(APPNAME)Icon.png $(APPNAME).iconset/icon_512x512@2x.png
	iconutil -c icns -o $(APPNAME).icns $(APPNAME).iconset
	rm -r $(APPNAME).iconset