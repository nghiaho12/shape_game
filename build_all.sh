#!/bin/bash

set -e

docker build -f Dockerfile.linux -t shape_game:linux .
docker run --rm -it -v /tmp:/tmp --network=host shape_game:linux /bin/bash -c 'cp /shape_game/build/release/* /tmp'

docker build -f Dockerfile.windows -t shape_game:windows .
docker run --rm -it -v /tmp:/tmp --network=host shape_game:windows /bin/bash -c 'cp /shape_game/build/release/* /tmp'

docker build -f Dockerfile.wasm -t shape_game:wasm .
docker run --rm -it -v /tmp:/tmp --network=host shape_game:wasm /bin/bash -c 'cp /shape_game/build/release/* /tmp'

docker build -f Dockerfile.android -t shape_game:android .
docker run --rm -it -v /tmp:/tmp --network=host shape_game:android /bin/bash -c 'cp /SDL/build/org.libsdl.shape_game/app/build/outputs/apk/debug/*.apk /tmp'

