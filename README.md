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
