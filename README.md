Simple shape matching game for kids written using SDL3 and OpenGL ES.
Dockerfile provided for Linux, WebAssembly and Android platform.

# Linux
```
docker build -t shape_game .
docker run --rm --network=host --ipc=host -e DISPLAY -u $(id -u) shape_game
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

Vorbis decoder
- https://github.com/edubart/minivorbis
