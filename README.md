Simple shape matching game for kids written using SDL3 and OpenGL ES.

![screenshot](screenshot.png)

# Binary release
See the Github releases page for Linux, Windows, Android and web binaries.

# Building from source
## Prerequisite
This repo uses git LFS for the assets. Install it before cloning, e.g. ```sudo apt install git-lfs```.
You'll also need to have Docker installed, e.g. ```sudo apt install docker-ce```.

## Linux
The default Linux target is Debian 12.8. Edit Dockerfile.linux to match your distro if you run into problems with the binary.

```
docker build -f Dockerfile.linux -t shape_game:linux .
docker run --rm -it --network=host shape_game:linux
```
Go to http://localhost:8000 to download the release package.

The tarball comes with SDL3 shared library bundled. Run the binary by calling
```
LD_LIBRARY_PATH=. ./shape_game
```

## Windows
```
docker build -f Dockerfile.windows -t shape_game:windows .
docker run --rm -it --network=host shape_game:windows
```

Point your browser to http://localhost:8000 to download the release package.

## Android
The APK is targeted at Android 9 (API Level 28) and above.

```
docker build -f Dockerfile.android -t shape_game:android .
docker run --rm -it --network=host shape_game:android
```

Point your Android web browser to http://localhost:8000 to download the APK.

## Web
```
docker build -f Dockerfile.wasm -t shape_game:wasm .
docker run --rm -it --network=host shape_game:wasm
```

Point your browser to http://localhost:8000 to download the release package.
If you have Python installed you can run the app by calling

```
python3 -m http.server
```

inside the extracted folder and point your browser to http://localhost:8000.


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
