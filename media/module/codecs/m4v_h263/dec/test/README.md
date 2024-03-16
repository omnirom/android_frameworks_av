## Media Testing ##
---
#### Mpeg4H263Decoder :
The Mpeg4H263Decoder Test Suite validates the Mpeg4 and H263 decoder available in libstagefright.

Run the following steps to build the test suite:
```
m Mpeg4H263DecoderTest
```

The 32-bit binaries will be created in the following path : ${OUT}/data/nativetest/

The 64-bit binaries will be created in the following path : ${OUT}/data/nativetest64/

To test 64-bit binary push binaries from nativetest64.
```
adb push ${OUT}/data/nativetest64/Mpeg4H263DecoderTest/Mpeg4H263DecoderTest /data/local/tmp/
```

To test 32-bit binary push binaries from nativetest.
```
adb push ${OUT}/data/nativetest/Mpeg4H263DecoderTest/Mpeg4H263DecoderTest /data/local/tmp/
```

The resource file for the tests is taken from [here](https://dl.google.com/android-unittest/media/frameworks/av/media/module/codecs/m4v_h263/dec/test/Mpeg4H263Decoder-1.2.zip).
Download, unzip and push these files into device for testing.

```
adb push Mpeg4H263Decoder-1.2 /data/local/tmp/
```

usage: Mpeg4H263DecoderTest -P \<path_to_folder\>
```
adb shell /data/local/tmp/Mpeg4H263DecoderTest -P /data/local/tmp/Mpeg4H263Decoder-1.2/
```
Alternatively, the test can also be run using atest command.

```
atest Mpeg4H263DecoderTest -- --enable-module-dynamic-download=true
```
