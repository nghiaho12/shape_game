Simple shape matching game for kids written using SDL3 and OpenGL ES.
Dockerfile provided for Linux, WebAssembly and Android platform.

# Linux
```
docker build -t shape_game .
docker run --network=host --ipc=host -e DISPLAY -u $(id -u) shape_game
```

Hit ESC to quit.

# WebAsembly 
```
docker build -f Dockerfile.wasm -t shape_game_wasm .
docker run --network=host shape_game_wasm
```

docker run --network=host shape_game_wasm
Point your browser to http://localhost:8000
