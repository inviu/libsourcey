# LibSourcey

##  install vcpkg 
git clone https://github.com/microsoft/vcpkg.git
bootstrap-vcpkg.bat

## add x64-windows-static-md triplets
create a x64-windows-static-md.cmake file at vcpkg\triplets directory

copy the following to x64-windows-static-md.cmake
set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)

## install ffmpeg via vcpkg
vcpkg install ffmpeg:x64-windows-static-md ffmpeg[avresample]:x64-windows-static-md ffmpeg[nonfree]:x64-windows-static-md

## delete build directory and run build.bat

