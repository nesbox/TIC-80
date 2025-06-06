curl -L https://dl.google.com/android/repository/sdk-tools-darwin-3859397.zip -o sdk.zip
unzip sdk.zip -d .

tools/bin/sdkmanager --verbose "platforms;android-28"
tools/bin/sdkmanager --verbose "build-tools;29.0.3"
tools/bin/sdkmanager --verbose platform-tools
tools/bin/sdkmanager --verbose ndk-bundle
