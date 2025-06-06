set -e

EXE=""
BAT=""
NAME=tic80
ABI=arm64-v8a
VERSION=28

RT_JAR="${JAVA_HOME}/jre/lib/rt.jar"
BUILD_TOOLS=${ANDROID_HOME}/build-tools/29.0.3/
SRC_DIR=src/com/nesbox/${NAME}

AAPT=${BUILD_TOOLS}aapt${EXE}
DX=${BUILD_TOOLS}dx${BAT}
ZIPALIGN=${BUILD_TOOLS}zipalign${EXE}
APKSIGNER=${BUILD_TOOLS}apksigner${BAT}

RELEASE_STORE_FILE=release.keystore
RELEASE_KEY_ALIAS=alias_name

mkdir -p dex
mkdir -p obj
mkdir -p lib/${ABI}
mkdir -p src/com/nesbox/${NAME}

cp bin/${NAME}.so lib/${ABI}/lib${NAME}.so

${AAPT} package -v -f -m -S android/res -J src -M android/AndroidManifest.xml -I ${ANDROID_HOME}/platforms/android-${VERSION}/android.jar
${JAVA_HOME}/bin/javac -d ./obj -source 1.7 -target 1.7 -sourcepath src -bootclasspath "${RT_JAR}" "${SRC_DIR}/R.java"
${DX} --verbose --dex --output=dex/classes.dex ./obj
${AAPT} package -v -f -S android/res -M android/AndroidManifest.xml -I ${ANDROID_HOME}/platforms/android-${VERSION}/android.jar -F ${NAME}-unaligned.apk dex
${AAPT} add -v ${NAME}-unaligned.apk lib/${ABI}/lib${NAME}.so
${ZIPALIGN} -f 4 ${NAME}-unaligned.apk ${NAME}.apk

if [ ! -f ${RELEASE_STORE_FILE} ]; then
	${JAVA_HOME}/bin/keytool -genkeypair -keystore ${RELEASE_STORE_FILE} -storepass ${RELEASE_STORE_PASSWORD} -alias ${RELEASE_KEY_ALIAS} -keypass ${RELEASE_KEY_PASSWORD} -keyalg RSA -validity 10000 -dname CN=,OU=,O=,L=,S=,C=
fi

${APKSIGNER} sign -v --ks ${RELEASE_STORE_FILE} --ks-pass pass:${RELEASE_STORE_PASSWORD} --key-pass pass:${RELEASE_KEY_PASSWORD} --ks-key-alias ${RELEASE_KEY_ALIAS} ${NAME}.apk
${APKSIGNER} verify -v ${NAME}.apk
