# USEFUL ADB COMMANDS

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
