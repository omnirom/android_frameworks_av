## Media Testing ##
---
#### ESDS Unit Test :
The ESDS Unit Test Suite validates the ESDS class available in libstagefright.

Run the following steps to build the test suite:
```
m ESDSTest
```

The 32-bit binaries will be created in the following path : ${OUT}/data/nativetest/

The 64-bit binaries will be created in the following path : ${OUT}/data/nativetest64/

To test 64-bit binary push binaries from nativetest64.
```
adb push ${OUT}/data/nativetest64/ESDSTest/ESDSTest /data/local/tmp/
```

To test 32-bit binary push binaries from nativetest.
```
adb push ${OUT}/data/nativetest/ESDSTest/ESDSTest /data/local/tmp/
```

The resource file for the tests is taken from [here](https://dl.google.com/android-unittest/media/frameworks/av/media/module/esds/tests/ESDSTestRes-1.1.zip)
Download, unzip and push these files into device for testing.

```
adb push ESDSTestRes-1.1 /data/local/tmp/
```

usage: ESDSTest -P \<path_to_folder\>
```
adb shell /data/local/tmp/ESDSTest -P /data/local/tmp/ESDSTestRes-1.1/
```
Alternatively, the test can also be run using atest command.

```
atest ESDSTest -- --enable-module-dynamic-download=true
```
