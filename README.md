Simple shape matching game for kids written using SDL3 and OpenGL ES.
It runs on Linux, Android and the web.

# Prerequisite
Install git-lfs eg.
```
sudo apt install git-lfs
```

You should see a shape_game/assets folder with some sound files.

Install Docker if you want to build for Android or web eg.
```
sudo apt install docker-ce
```

# Linux
```
mkdir build
cd build
cmake ..
make
cd ..
./build/shape_game
```

Hit ESC to quit.

# WebAsembly 
```
docker build -f Dockerfile.wasm -t shape_game_wasm .
docker run --rm --network=host shape_game_wasm
```

Point your browser to http://localhost:8000.

# Android
```
docker build -f Dockerfile.android -t shape_game_android .
docker run --rm --network=host shape_game_android
```

Point your browser to http://localhost:8000 on your Android device and download the APK.

# Credits
Assets 
- https://opengameart.org/content/fun-a-bgm-track
- https://opengameart.org/content/6-user-interface-ding-clicks

Ogg Vorbis decoder
 - https://github.com/nothings/stb
