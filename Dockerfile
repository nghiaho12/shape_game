FROM debian:12.8

RUN apt update 
RUN apt install -y g++ git cmake libx11-dev libxext-dev libgl1-mesa-dev pkg-config python3 xz-utils

RUN git clone -b 3.1.73 --depth 1 https://github.com/emscripten-core/emsdk.git
RUN cd emsdk && \
    ./emsdk install latest 

RUN git clone -b preview-3.1.6 --depth 1 https://github.com/libsdl-org/SDL.git

RUN cd SDL && \
    mkdir build && \
    cd build && \
    cmake -DCMAKE_BUILD_TYPE=Release .. && \
    cmake --build . --config Release --parallel && \
    cmake --install . --config Release

RUN cd /emsdk&& \
    ./emsdk activate latest && \
    . ./emsdk_env.sh && \
    cd /SDL && \
    mkdir buildem && \
    cd buildem && \
    emcmake cmake -DCMAKE_BUILD_TYPE=Release .. && \
    emmake make -j4 && \
    emmake make install

RUN git clone -b 1.0.1 --depth 1 https://github.com/g-truc/glm.git
RUN cd glm && \
    cmake -DGLM_BUILD_TESTS=OFF -DBUILD_SHARED_LIBS=OFF -B build . && \
    cmake --build build -- all && \
    cmake --build build -- install

RUN cd /emsdk&& \
    ./emsdk activate latest && \
    . ./emsdk_env.sh && \
    cd /glm && \
    mkdir buildem && \
    cd buildem && \
    emcmake cmake -DGLM_BUILD_TESTS=OFF -DBUILD_SHARED_LIBS=OFF .. && \
    emmake make && \
    emmake make install

