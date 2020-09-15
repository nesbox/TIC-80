# USEFUL ADB COMMANDS

## BUILD IN DOCKER
```
winpty docker exec -it nice_goldwasser bash -c "cd /tic80/build/android && ./gradlew assembleRelease" && adb install -r app/build/outputs/apk/arm8/release/app-arm8-release.apk && adb shell am start -n com.nesbox.tic/com.nesbox.tic.TIC
```

## INSTALL APP AND RUN
```
release: adb install -r app/build/outputs/apk/arm8/release/app-arm8-release.apk && adb shell am start -n com.nesbox.tic/com.nesbox.tic.TIC

debug: adb install -r app/build/outputs/apk/arm8/debug/app-arm8-debug.apk && adb shell am start -n com.nesbox.tic/com.nesbox.tic.TIC
```

## VIEW APP LOG
```
adb logcat --pid `adb shell pidof com.nesbox.tic`
```

## STOP APP
```
adb shell am force-stop com.nesbox.tic
```
