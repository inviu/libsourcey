#!/bin/bash
rm -rf build
mkdir build
cd build
export webrtc_root_dir="/home/jayden/share/opensource/webrtc_m72/src/"
export webrtc_include_dir="/home/jayden/share/opensource/webrtc_m72/src/"
export webrtc_library_debug="/home/jayden/share/opensource/webrtc_m72/src/out/Debug/obj/libwebrtc.a"
export webrtc_library_release="/home/jayden/share/opensource/webrtc_m72/src/out/Release/obj/libwebrtc.a"
export ffmpeg_root_dir="/home/jayden/toolchains/vcpkg/installed/x64-linux-shared/"

cmake .. -DWITH_WEBRTC=ON -DBUILD_SHARED_LIBS=OFF  -DWEBRTC_ROOT_DIR=$webrtc_root_dir \
-DWEBRTC_INCLUDE_DIR=$webrtc_root_dir \
-DWEBRTC_LIBRARY_DEBUG=$webrtc_library_debug \
-DWEBRTC_LIBRARY_RELEASE=$webrtc_library_release -DWITH_FFMPEG=ON \
-DFFMPEG_ROOT_DIR=$ffmpeg_root_dir -DCMAKE_CXX_STANDARD=17 -DBUILD_SAMPLES=OFF \
-DBUILD_TESTS=OFF -DCMAKE_INSTALL_PREFIX=./stage
