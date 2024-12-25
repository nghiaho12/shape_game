Simple shape matching game for kids written using SDL3 and OpenGL ES.
It rus on Ubuntu 24.04, Android 14, Firefox 130.0 and Chromimum 131.0.

![screenshot](screenshot.png)

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

For Linux, install SDL 3 (https://github.com/libsdl-org/SDL/).

## Linux
```
cmake -B build
cmake --build build
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

Point your Android web browser to http://[IP of host]:8000. Download and install the APK.

# Credits
Sound assets 
- https://opengameart.org/content/fun-a-bgm-track
- https://opengameart.org/content/6-user-interface-ding-clicks
- https://opengameart.org/content/well-done

Ogg Vorbis decoder
- https://github.com/nothings/stb

Signed Distance Field (SDF) font
- https://github.com/Chlumsky/msdfgen

# Contact
nghiaho12@yahoo.com
