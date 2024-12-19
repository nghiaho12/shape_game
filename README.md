Simple shape matching game for kids written using SDL3 and OpenGL ES.
It has been tested on Ubuntu 24.04, Android 14 and web (Firefox 130.0).

You might find some useful code snippets in here for other stuff.
Here are some

- Signed Distance Field (SDF) Font
- Drawing polygons with arbitrary line thickness
- C++ memory managed OpenGL objects

# Install
## Prerequisite
This repo uses git LFS for the assets. Install it before cloning.
```
sudo apt install git-lfs
```

Install Docker if you want to build for Android or web.
```
sudo apt install docker-ce
```

## Linux
Install SDL 3 (https://github.com/libsdl-org/SDL/)

```
mkdir build
cd build
cmake ..
make
cd ..
./build/shape_game
```

Hit ESC to quit.

## Web
```
docker build -f Dockerfile.wasm -t shape_game_wasm .
docker run --rm --network=host shape_game_wasm
```

Point your browser to http://localhost:8000.

## Android
```
docker build -f Dockerfile.android -t shape_game_android .
docker run --rm --network=host shape_game_android
```

Point your browser to http://[IP of host]:8000 on your Android web browser and download the APK.

# Credits
Sound assets 
- https://opengameart.org/content/fun-a-bgm-track
- https://opengameart.org/content/6-user-interface-ding-clicks
- https://opengameart.org/content/well-done

Ogg Vorbis decoder
- https://github.com/nothings/stb

SDF font
- https://github.com/Chlumsky/msdfgen

# Contact
nghiaho12@yahoo.com
