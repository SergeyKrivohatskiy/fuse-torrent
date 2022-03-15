# FuseTorrent
A simple command line torrent client with FUSE file mapping

# Usage
TODO

# Build

Project is using **Cmake** (and **conan** that runs from cmake). See example build commands

## requirenemts
- \>= C++17 compatible compiler
    tested with
    - gcc9.4
    - Visual Studio 2019
- Cmake >= 3.16
- [conan](https://docs.conan.io/en/latest/installation.html)
- WinFsp (Windows only)
    - go to [winfsp](https://winfsp.dev/rel/) website
    - download winfsp installer
    - install `core` and `develop` winfsp components
- FUSE (Linux/OSX only)
    - `yum install fuse fuse-devel` for Centos
    - `apt install fuse libfuse-dev`
    - e.t.c

## Example Windows build commands:
    git clone https://github.com/SergeyKrivohatskiy/fuse-torrent.git
    mkdir fuse-torrent-build-dir
    cd fuse-torrent-build-dir
    cmake -G "Visual Studio 16 2019" ../fuse-torrent/fuse-torrent -DCMAKE_BUILD_TYPE=Release
    cmake --build . --config Release
