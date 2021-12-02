ARG BASE_IMAGE
FROM $BASE_IMAGE

# GCC
ARG GCC_VERSION=9

RUN apt-add-repository -y ppa:ubuntu-toolchain-r/test \
 && apt-get update \
 && apt-get install -y --no-install-recommends \
    gcc-$GCC_VERSION \
    g++-$GCC_VERSION \
    gdb \
    linux-tools-generic \
 && apt-get clean \
 && rm -rf /var/lib/apt/lists/* \
 && ln -sf /usr/bin/g++-$GCC_VERSION /usr/bin/g++ \
 && ln -sf /usr/bin/gcc-$GCC_VERSION /usr/bin/gcc

ENV CC gcc
ENV CXX g++

# CMake
#ARG CMAKE_MINOR_VERSION=3.19
ARG CMAKE_VERSION=3.19.5
#ARG CMAKE-ARCH="Linux-x86_64"

#&& wget -q https://cmake.org/files/v$MINOR_VERSION/cmake-$VERSION-$ARCH.tar.gz \
RUN apt-get update && apt-get install -y libssl-dev libnuma-dev\
 && cd opt \
 && wget -q https://github.com/Kitware/CMake/releases/download/v$CMAKE_VERSION/cmake-$CMAKE_VERSION.tar.gz \
 && tar xf cmake-$CMAKE_VERSION.tar.gz \
 && rm cmake-$CMAKE_VERSION.tar.gz \
 && cd cmake-$CMAKE_VERSION \
 && ./bootstrap --parallel=5 \
 && make -j \
 && make install

# && ln -sf /opt/cmake-$VERSION-$ARCH/bin/ccmake /usr/bin/ccmake \
# && ln -sf /opt/cmake-$VERSION-$ARCH/bin/cmake /usr/bin/cmake \
# && ln -sf /opt/cmake-$VERSION-$ARCH/bin/cmake-gui /usr/bin/cmake-gui \
# && ln -sf /opt/cmake-$VERSION-$ARCH/bin/cpack /usr/bin/cpack \
# && ln -sf /opt/cmake-$VERSION-$ARCH/bin/ctest /usr/bin/ctest