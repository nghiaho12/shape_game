FROM debian:12.8

RUN apt update && \
    apt install -y g++ git cmake libx11-dev libxext-dev libgl1-mesa-dev pkg-config

ARG SDL_VER=preview-3.1.6
ARG GLM_VER=1.0.1

RUN git clone -b $SDL_VER --depth 1 https://github.com/libsdl-org/SDL.git

RUN cd SDL && \
    mkdir build && \
    cd build && \
    cmake -DCMAKE_BUILD_TYPE=Release .. && \
    cmake --build . --config Release --parallel && \
    cmake --install . --config Release

RUN git clone -b $GLM_VER --depth 1 https://github.com/g-truc/glm.git
RUN cd glm && \
    cmake -DGLM_BUILD_TESTS=OFF -DBUILD_SHARED_LIBS=OFF -B build . && \
    cmake --build build -- all && \
    cmake --build build -- install

WORKDIR /shape_game
COPY src/ /shape_game/src
COPY CMakeLists.txt /shape_game

RUN mkdir build && \
    cd build && \
    cmake .. && \
    make

CMD ["/shape_game/build/shape_game"]
