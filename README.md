# 💬 Creatorgram — SimpleSDLMessenger

> Real-time messenger with SDL2 GUI, room system, user authentication and XOR message encryption — built with **C++17** and **Boost.Asio**.

![C++](https://img.shields.io/badge/C++-17-blue?logo=cplusplus&logoColor=white)
![SDL2](https://img.shields.io/badge/SDL2-2.x-orange)
![Boost.Asio](https://img.shields.io/badge/Boost.Asio-1.74+-green)
![Platform](https://img.shields.io/badge/platform-Linux%20%7C%20macOS%20%7C%20Windows-lightgrey)

---

## 📖 About

**Creatorgram** is a pet project — a desktop chat client with a server, written from scratch in C++. Users can register/login, create password-protected chat rooms or join existing public ones, and exchange messages in real time. All traffic is encrypted with a symmetric XOR cipher.

---

## ✨ Features

- 🔐 **Registration & Login** — accounts are stored in a flat-file database on the server
- 🏠 **Room system** — create a named room with an optional password, or join an existing one
- 📋 **Room browser** — request a list of active public rooms (up to 5) directly from the UI
- 💬 **Real-time messaging** — async receiving via a background thread, rendered in an SDL2 window
- 🔒 **XOR encryption** — every packet is encrypted/decrypted with a symmetric key exchanged on connect
- 🔊 **Sound effects** — button clicks and incoming messages play WAV/MP3 sounds via SDL_mixer
- 🖼️ **Custom window icon & background** — loaded from PNG assets with SDL_image

---

## 🗂️ Project Structure

```
SimpleSDLMessenger/
├── server.cpp      # TCP server: auth, room management, message broadcast
├── client.cpp      # SDL2 client: UI screens, input, async Session thread
├── cipher.h        # XorCipher — symmetric encrypt / decrypt
└── functions.h     # Helpers: renderText(), clear(), Menu class
```

---

## 🏗️ Architecture

```
┌─────────────────────────────┐        TCP :8088        ┌──────────────────────────────┐
│           CLIENT            │ ◄─────────────────────► │            SERVER            │
│                             │                         │                              │
│  SDL2 render loop (main)    │   XOR-encrypted packets │  acceptor loop               │
│  Session thread (recv)      │ ──────────────────────► │  session_thread per client   │
│                             │                         │                              │
│  Screens:                   │   Key exchange (56 B)   │  Login / Registration        │
│   • Auth (login / register) │ ◄────────────────────── │  Room: create / join         │
│   • Room select             │                         │  Broadcast to room members   │
│   • Chat (scroll, input)    │                         │  Room browser (big_chats)    │
│   • Room browser            │                         │                              │
└─────────────────────────────┘                         └──────────────────────────────┘
```

### Protocol codes

| Code   | Direction       | Meaning                   |
|--------|-----------------|---------------------------|
| `1000` | both            | ACK / OK                  |
| `999`  | server → client | Error / reject            |
| `5000` | client → server | Create room               |
| `5001` | client → server | Join room                 |
| `5002` | client → server | Request room list         |
| `5003` | server → client | End of room list          |

---

## 🛠️ Dependencies

| Library | Purpose |
|---|---|
| [Boost.Asio](https://www.boost.org/) | Async TCP networking |
| [SDL2](https://www.libsdl.org/) | Window, rendering, keyboard input |
| [SDL2_ttf](https://github.com/libsdl-org/SDL_ttf) | Font rendering |
| [SDL2_image](https://github.com/libsdl-org/SDL_image) | PNG textures & window icon |
| [SDL2_mixer](https://github.com/libsdl-org/SDL_mixer) | WAV/MP3 sound effects |

---

## 🚀 Build & Run

### Install dependencies

**Ubuntu / Debian**
```bash
sudo apt install libsdl2-dev libsdl2-ttf-dev libsdl2-image-dev libsdl2-mixer-dev \
                 libboost-all-dev g++
```

**macOS (Homebrew)**
```bash
brew install sdl2 sdl2_ttf sdl2_image sdl2_mixer boost
```

---

### Compile

**Server:**
```bash
g++ server.cpp -o server \
    -lboost_system -lpthread -std=c++17
```

**Client:**
```bash
g++ client.cpp -o client \
    -lSDL2 -lSDL2_ttf -lSDL2_image -lSDL2_mixer \
    -lboost_system -lpthread -std=c++17
```

---

### Assets

The client expects these files in the working directory:

```
background.png   icon.png   logo.png
log.png   reg.png   create.png   join.png   games.png   chat.png
button.wav   send.wav   menu.mp3
SigmarOne-Regular.ttf   arialmt.ttf
```

---

### Run

1. Start the server (listens on port **8088**):
   ```bash
   ./server
   ```

2. Launch one or more clients:
   ```bash
   ./client
   ```

3. Register or log in → create / join a room → chat.

---

## 🔐 Encryption

All packets are encrypted with **XOR** using a symmetric key. On connect the server sends a 56-byte blob; the client extracts the key from bytes 16–55. Every subsequent message is passed through `XorCipher::cipher()` before `write()` and through `XorCipher::decrypt()` after `read_some()`.

```cpp
// cipher.h
result[i] = message[i] ^ key[i % key.size()];
```

> ⚠️ XOR cipher provides obfuscation, not cryptographic security. For a production app consider replacing it with TLS or AES.

---

## 📌 Known Limitations

- Server database is a plain-text file with `login password` lines — path is hardcoded in `server.cpp`
- XOR key is a static compile-time constant on the server side
- No message history persistence on the client
- Up to 5 public rooms are shown in the room browser at a time

---

## 📄 License

This project is open source. Feel free to fork and experiment.
