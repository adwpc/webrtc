# webrtc ios src
## cocoapods
Released	Oct 2019 (time maybe same as M78)
https://cocoapods.org/pods/GoogleWebRTC
```

```

## src
```
git clone https://webrtc.googlesource.com/src
git checkout 5963c7cf0a70e1abdf0e3f313c10cb8a161f6cdb (M78)
```
## Ref:
https://webrtc.github.io/webrtc-org/native-code/ios/
```
Create a working directory, enter it, and run:
fetch --nohooks webrtc_ios
gclient sync
git checkout remotes/branch-heads/m78
```
```
commit 0b2302e5e0418b6716fbc0b3927874fd3a842caf (HEAD, branch-heads/m78)
Author: Ilya Nikolaevskiy <ilnik@webrtc.org>
Date:   Thu Oct 10 13:32:28 2019 +0200

    Merge to M78: Count disabled due to low bw streams or layers as bw limited quality in GetStats

    Original Reviewed-on: https://webrtc-review.googlesource.com/c/src/+/155964
    (cherry picked from commit 5963c7cf0a70e1abdf0e3f313c10cb8a161f6cdb)

    Bug: webrtc:11015
    Change-Id: I1ccd7f9fc875ecec2de45e0d8f668f887f8be06f
    Reviewed-on: https://webrtc-review.googlesource.com/c/src/+/156306
    Reviewed-by: Niels Moller <nisse@webrtc.org>
    Reviewed-by: Henrik Boström <hbos@webrtc.org>
    Commit-Queue: Ilya Nikolaevskiy <ilnik@webrtc.org>
    Cr-Commit-Position: refs/branch-heads/m78@{#8}
    Cr-Branched-From: 5b728cca77c46ed47ae589acba676485a957070b-refs/heads/master@{#29078}
```

# Origin README

**WebRTC is a free, open software project** that provides browsers and mobile
applications with Real-Time Communications (RTC) capabilities via simple APIs.
The WebRTC components have been optimized to best serve this purpose.

**Our mission:** To enable rich, high-quality RTC applications to be
developed for the browser, mobile platforms, and IoT devices, and allow them
all to communicate via a common set of protocols.

The WebRTC initiative is a project supported by Google, Mozilla and Opera,
amongst others.

### Development

See http://www.webrtc.org/native-code/development for instructions on how to get
started developing with the native code.

[Authoritative list](native-api.md) of directories that contain the
native API header files.

### More info

 * Official web site: http://www.webrtc.org
 * Master source code repo: https://webrtc.googlesource.com/src
 * Samples and reference apps: https://github.com/webrtc
 * Mailing list: http://groups.google.com/group/discuss-webrtc
 * Continuous build: http://build.chromium.org/p/client.webrtc
 * [Coding style guide](style-guide.md)
 * [Code of conduct](CODE_OF_CONDUCT.md)
