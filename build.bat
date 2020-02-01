md build
cd build
set webrtc_root_dir="G:\opensource\webrtc_win\src"
set webrtc_include_dir="G:\opensource\webrtc_win\src"
set webrtc_library_debug="G:\opensource\webrtc_win\src\out\Debug\obj\webrtc.lib"
set webrtc_library_release="G:\opensource\webrtc_win\src\out\Release\obj\webrtc.lib"
set ffmpeg_root_dir="E:\toolchains\vcpkg\installed\x64-windows-static-md"

cmake .. -G "Visual Studio 15 Win64" .. -DWITH_WEBRTC=ON  -DWEBRTC_ROOT_DIR=%webrtc_root_dir% -DWEBRTC_INCLUDE_DIR=%webrtc_root_dir% -DWEBRTC_LIBRARY_DEBUG=%webrtc_library_debug% -DWEBRTC_LIBRARY_RELEASE=%webrtc_library_release% -DWITH_FFMPEG=ON -DFFMPEG_ROOT_DIR=%ffmpeg_root_dir% -DCMAKE_CXX_STANDARD=17
