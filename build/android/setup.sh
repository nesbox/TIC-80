set -e

curl -L https://builds.openlogic.com/downloadJDK/openlogic-openjdk/8u382-b05/openlogic-openjdk-8u382-b05-windows-x64.zip -o jdk.zip
unzip jdk.zip -d .
curl -L https://dl.google.com/android/repository/sdk-tools-windows-3859397.zip -o sdk.zip
unzip sdk.zip -d .

export JAVA_HOME=${PWD}/openlogic-openjdk-8u382-b05-windows-64

${JAVA_HOME}/bin/java -version
${JAVA_HOME}/bin/javac -version

tools/bin/sdkmanager.bat --verbose "platforms;android-28"
tools/bin/sdkmanager.bat --verbose "build-tools;29.0.3"
tools/bin/sdkmanager.bat --verbose platform-tools
tools/bin/sdkmanager.bat --verbose ndk-bundle
