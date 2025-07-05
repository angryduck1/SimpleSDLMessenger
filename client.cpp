#include <iostream>
#include <boost/asio.hpp>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_mixer.h>
#include <png++/png.hpp>
#include <chrono>
#include <thread>
#include "cipher.h"
#include "functions.h"

using namespace std;
using namespace boost::asio;

using ip::tcp;

string key;

constexpr int WIDTH = 500;
constexpr int HEIGHT = 800;

string create_room = "5000";
string join_room = "5001";
string big_chats = "5002";
string big_chats_end = "5003";

std::mutex chats_textures_messages_mutex;
vector<string> chats_textures_messages;

struct ChatMessage {
    string text;
    int yPos;
};

class Packets {
public:
    std::string recv_packet_with_timeout(XorCipher& cip, tcp::socket& client, boost::asio::io_context& io, int timeout_ms = 2000) {
        using namespace boost::asio;
        std::array<char, 1024> data;
        boost::system::error_code ec;
        size_t length = 0;
        bool data_received = false;
        bool timeout_occurred = false;

        steady_timer timer(io);
        timer.expires_after(std::chrono::milliseconds(timeout_ms));

        client.async_read_some(buffer(data), [&](const boost::system::error_code& error, size_t bytes_transferred) {
            if (!error) {
                length = bytes_transferred;
                data_received = true;
                timer.cancel();
            }
            else {
                ec = error;
                timer.cancel();
            }
        });

        timer.async_wait([&](const boost::system::error_code& error) {
            if (!error) {
                timeout_occurred = true;
                client.cancel();
            }
        });

        io.run_for(std::chrono::milliseconds(timeout_ms + 100));
        io.restart();

        if (timeout_occurred) {
            return "";
        }
        if (ec) {
            std::cout << "Error recv message from server: " << ec.value() << " " << ec.message() << std::endl;
            return "";
        }
        if (length == 0) {
            return "";
        }
        return cip.decrypt(std::string(data.data(), length), key, length);
    }


    bool send_packet(string message, XorCipher& cip, tcp::socket& client) {
        boost::system::error_code error;

        write(client, buffer(cip.cipher(message, key)), error);
        if (error) {
            cout << "Error send message to server: " << error.value() << " " << error.message() << endl;
            return 0;
        }
        else {
            return 1;
        }
    }

    string recv_packet(XorCipher& cip, tcp::socket& client) {
        array<char, 1024> data;
        boost::system::error_code error;

        size_t length = client.read_some(buffer(data), error);

        if (error) {
            if (error == boost::asio::error::would_block) {
                return "";
            }
            cout << "Error recv message from server: " << error.value() << " " << error.message() << endl;
        }
        if (length <= 0) {
            return "";
        }
        else {
            return cip.decrypt(string(data.data(), length), key, length);
        }
    }
};

string decrypt_message(tcp::socket& client) {
    const size_t EXPECTED_SIZE = 56;
    vector<char> buf(EXPECTED_SIZE);
    boost::system::error_code error;

    read(client, buffer(buf), transfer_exactly(EXPECTED_SIZE), error);

    if (error || buf.size() < EXPECTED_SIZE) {
        cerr << "Key read error: " << error.message() << "\n";
        return "";
    }
    return string(buf.begin() + 16, buf.end());
}

class Session {
public:
    std::atomic<bool> paused_{ false };

    Session(Packets& p, XorCipher& cip, shared_ptr<tcp::socket> server,
            atomic<bool>& running, vector<ChatMessage>& messages,
            mutex& msgMutex, int& totalHeight, io_context& io_ctx, SDL_Renderer* renderer, Mix_Chunk* send_sound)
        : p_(p), cip_(cip), server_(server), running_(running),
        messages_(messages), msgMutex_(msgMutex), totalHeight_(totalHeight), io_ctx_(io_ctx), renderer_(renderer), send_sound_(send_sound), thread_(&Session::run, this) {
    }

    ~Session() {
        thread_.join();
    }

private:
    void run() {
        while (running_) {
            string msg = p_.recv_packet_with_timeout(cip_, *server_, io_ctx_, 1000);

            if (paused_) {
                {
                lock_guard<mutex> lock(chats_textures_messages_mutex);
                while (chats_textures_messages.size() != 0) {
                    chats_textures_messages.pop_back();
                }
                }

                p_.send_packet(big_chats, cip_, *server_);

                string response;
                bool received_end = false;

                while (!received_end) {

                    cout << "in chat" << endl;

                    response = p_.recv_packet(cip_, *server_);

                    p_.send_packet("1000", cip_, *server_);

                    if (response == big_chats_end) {
                        cout << "succesfful end" << endl;
                        received_end = true;
                        continue;
                    }

                    if (!response.empty()) {
                        cout << response << endl;

                        {
                        lock_guard<mutex> lock(chats_textures_messages_mutex);
                        chats_textures_messages.push_back(response);
                        }
                    }

                    continue;
                }

                paused_.store(false);

                continue;
            }

            if (msg != create_room && msg != join_room && msg != big_chats && msg != big_chats_end) {
                if (!msg.empty()) {
                    Mix_PlayChannel(-1, send_sound_, 0);
                    lock_guard<mutex> lock(msgMutex_);
                    messages_.insert(messages_.begin(), { msg, 0 });
                    totalHeight_ += 20;
                }
                else {
                    this_thread::sleep_for(std::chrono::milliseconds(10));
                }
            }
        }
    }

    Packets& p_;
    XorCipher& cip_;
    shared_ptr<tcp::socket> server_;
    atomic<bool>& running_;
    vector<ChatMessage>& messages_;
    mutex& msgMutex_;
    int& totalHeight_;
    thread thread_;
    io_context& io_ctx_;
    SDL_Renderer* renderer_;
    Mix_Chunk* send_sound_;
};

int main() {
    atomic<bool> running(true);
    vector<ChatMessage> messages;
    mutex msgMutex;
    int scrollOffset = 0;
    int totalHeight = 0;
    const int MESSAGE_HEIGHT = 20;
    string activeChat;
    string count_chats;
    string username;
    bool is_chats = false;

    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;


    if (SDL_Init(SDL_INIT_EVERYTHING) < 0) {
        std::cerr << "Error Inititialization EVERYTHING: " << SDL_GetError();
        SDL_Quit();
        return 1;
    }

    Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 4096);

    if (TTF_Init() < 0) {
        std::cerr << "Error Inititialization TTF: " << TTF_GetError();
        SDL_Quit();
        return 1;
    }

    if ((IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG) != IMG_INIT_PNG) {
        std::cerr << "Error Inititialization TTF: " << TTF_GetError();
        SDL_Quit();
        return 1;
    }

    window = SDL_CreateWindow("Creatorgram", 100, 100, WIDTH, HEIGHT, SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI);

    if (window == nullptr) {
        std::cerr << "Error create window: " << SDL_GetError();
        SDL_Quit();
        return 1;
    }

    renderer = SDL_CreateRenderer(window, -1, 0);
    SDL_RenderSetLogicalSize(renderer, WIDTH, HEIGHT);

    SDL_Surface* icon = IMG_Load("icon.png");
    SDL_SetWindowIcon(window, icon);
    SDL_FreeSurface(icon);

    SDL_Event event;
    bool quit = false;
    bool join = false;
    bool join_login = false;
    bool join_create_room = false;
    bool active_chats = false;

    string ack = "login";
    string login;
    string password;
    string buf;

    SDL_Rect board_messages = { 30, 100, 440, 600 };

    SDL_Rect login_board = { 40, 320, 420, 80 };
    SDL_Rect password_board = { 40, 460, 420, 80 };

    SDL_Texture* login_button = IMG_LoadTexture(renderer, "log.png");
    SDL_Texture* register_button = IMG_LoadTexture(renderer, "reg.png");
    SDL_Texture* create_button = IMG_LoadTexture(renderer, "create.png");
    SDL_Texture* join_button = IMG_LoadTexture(renderer, "join.png");
    SDL_Texture* games_button = IMG_LoadTexture(renderer, "games.png");
    SDL_Texture* chat_button = IMG_LoadTexture(renderer, "chat.png");

    Mix_Chunk* button_sound = Mix_LoadWAV("button.wav");
    Mix_Chunk* send_sound = Mix_LoadWAV("send.wav");
    Mix_Music* menu_sound = Mix_LoadMUS("menu.mp3");

    io_context io_ctx;

    tcp::endpoint endpoint(ip::make_address("5.35.102.132"), 8088);

    tcp::socket socket(io_ctx);

    socket.connect(endpoint);

    XorCipher cip;
    Packets p;

    SDL_Texture* background = IMG_LoadTexture(renderer, "background.png");

    SDL_Texture* logo = IMG_LoadTexture(renderer, "logo.png");

    SDL_Rect dstrect = { -50, -60, 600, 350 };
    SDL_Rect dstrect_chat = { 70, -37, 340, 190 };

    std::string* activeInput = &login;

    const int centerX = WIDTH / 2;
    const int startY = HEIGHT / 2.5;
    const int lineSpacing = 50;
    const int inputOffset = 300;

    const SDL_Point positions[] = {
        {centerX - 200, startY},
        {centerX - 154 + inputOffset, startY},
        {centerX - 200, startY + lineSpacing},
        {centerX - 415 + inputOffset, startY + lineSpacing},
        {centerX - 200, startY + 2 * lineSpacing},
        {centerX - 355 + inputOffset, startY + 2 * lineSpacing}
    };

    shared_ptr<tcp::socket> server = make_shared<tcp::socket>(std::move(socket));

    Menu login_obj;
    Menu register_obj;

    SDL_Rect register_obj_d = { 100, 570, 300, 100 };
    SDL_Rect login_obj_d = { 100, 680, 300, 100 };

    register_obj.InititTexture(register_button, register_obj_d);
    login_obj.InititTexture(login_button, login_obj_d);

    Menu join_obj;
    Menu create_obj;

    SDL_Rect join_obj_d = { 250, 680, 250, 60 };
    SDL_Rect create_obj_d = { 0, 680, 250, 60 };

    join_obj.InititTexture(join_button, join_obj_d);
    create_obj.InititTexture(create_button, create_obj_d);

    Menu games_obj;
    Menu chat_obj;

    SDL_Rect games_obj_d = { 10, 720, 230, 60 };
    SDL_Rect chat_obj_d = { 250, 720, 230, 60 };

    games_obj.InititTexture(games_button, games_obj_d);
    chat_obj.InititTexture(chat_button, chat_obj_d);

    key = decrypt_message(*server);

    while (!quit) {
        try {
            while (!join) {
                while (!join_login) {
                    SDL_StartTextInput();
                    while (SDL_PollEvent(&event)) {
                        if (event.type == SDL_QUIT) {
                            running = false;
                            clear(renderer, window);
                            quit = true;
                            join_login = true;

                            SDL_DestroyTexture(login_button);
                            SDL_DestroyTexture(register_button);
                            SDL_DestroyTexture(create_button);
                            SDL_DestroyTexture(join_button);
                            SDL_DestroyTexture(games_button);
                            SDL_DestroyTexture(chat_button);
                            SDL_DestroyTexture(background);
                            SDL_DestroyTexture(logo);

                            Mix_FreeChunk(button_sound);
                            Mix_FreeChunk(send_sound);
                            Mix_FreeMusic(menu_sound);

                            return 0;
                        }
                        else if (event.type == SDL_TEXTINPUT) {
                            if (activeInput) {
                                activeInput->append(event.text.text);
                            }
                        }
                        else if (event.type == SDL_KEYDOWN) {
                            if (activeInput && event.key.keysym.sym == SDLK_BACKSPACE && !activeInput->empty()) {
                                activeInput->pop_back();
                            }
                            else if (event.key.keysym.sym == SDLK_TAB) {
                                if (activeInput == &login) activeInput = &password;
                                else if (activeInput == &password) activeInput = &login;
                            }
                        }
                        else if (event.type == SDL_MOUSEBUTTONDOWN) {
                            if (register_obj.PressToPlay() == 0) {
                                if (!login.empty() && !password.empty()) {
                                    ack = "register";
                                    Mix_PlayChannel(-1, button_sound, 0);
                                    join_login = true;
                                }
                            }
                            else if (login_obj.PressToPlay() == 0) {
                                if (!login.empty() && !password.empty()) {
                                    ack = "login";
                                    Mix_PlayChannel(-1, button_sound, 0);
                                    join_login = true;
                                }
                            }
                        }
                    }

                    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
                    SDL_RenderClear(renderer);

                    SDL_RenderCopy(renderer, background, NULL, NULL);

                    SDL_RenderCopy(renderer, logo, nullptr, &dstrect);

                    SDL_Color color = { 255, 255, 255, 255 };

                    renderText("AUTORISATION", "SigmarOne-Regular.ttf", color, 45, renderer, positions[0].x - 5, positions[0].y - 140);

                    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
                    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 80);
                    SDL_RenderFillRect(renderer, &login_board);

                    SDL_RenderFillRect(renderer, &password_board);

                    SDL_RenderCopy(renderer, register_button, nullptr, &register_obj_d);
                    SDL_RenderCopy(renderer, login_button, nullptr, &login_obj_d);

                    if (!login.empty()) {
                        renderText(login, "arialmt.ttf", color, 30, renderer, positions[3].x - 80, positions[3].y - 25);
                    }
                    if (!password.empty()) {
                        renderText(std::string(password.size(), '*'), "arialmt.ttf", color, 24, renderer, positions[3].x - 80, positions[5].y + 70);
                    }

                    SDL_RenderPresent(renderer);

                    SDL_StopTextInput();
                }

                p.send_packet(ack, cip, *server);

                if (p.recv_packet(cip, *server) == "1000") {
                    p.send_packet(login, cip, *server);
                }
                if (p.recv_packet(cip, *server) == "1000") {
                    p.send_packet(password, cip, *server);
                }

                string answ_server = p.recv_packet(cip, *server);

                if (answ_server == "999") {
                    cout << "Failed to register or login in account" << endl;
                    running = false;
                    clear(renderer, window);
                    quit = true;
                    join_login = true;
                    return 1;
                }
                else if (answ_server == "1000") {
                    cout << "Succesfull login or register in account" << endl;
                }

                join = true;

                dstrect.x = -50;
                dstrect.y = -50;

                dstrect.w = 600;
                dstrect.h = 350;
            }

            username = login;

            count_chats = p.recv_packet(cip, *server);

            while (!join_create_room) {
                SDL_StartTextInput();

                while (SDL_PollEvent(&event)) {
                    if (event.type == SDL_QUIT) {
                        running = false;
                        clear(renderer, window);
                        quit = true;
                        join_create_room = true;

                        SDL_DestroyTexture(login_button);
                        SDL_DestroyTexture(register_button);
                        SDL_DestroyTexture(create_button);
                        SDL_DestroyTexture(join_button);
                        SDL_DestroyTexture(games_button);
                        SDL_DestroyTexture(chat_button);
                        SDL_DestroyTexture(background);
                        SDL_DestroyTexture(logo);

                        Mix_FreeChunk(button_sound);
                        Mix_FreeChunk(send_sound);
                        Mix_FreeMusic(menu_sound);

                        return 0;
                    }
                    else if (event.type == SDL_TEXTINPUT) {
                        if (activeInput) {
                            activeInput->append(event.text.text);
                        }
                    }
                    else if (event.type == SDL_KEYDOWN) {
                        if (activeInput && event.key.keysym.sym == SDLK_BACKSPACE && !activeInput->empty()) {
                            activeInput->pop_back();
                        }
                        else if (event.key.keysym.sym == SDLK_TAB) {
                            if (activeInput == &login) activeInput = &password;
                            else if (activeInput == &password) activeInput = &login;
                        }
                    }
                    else if (event.type == SDL_MOUSEBUTTONDOWN) {
                        if (create_obj.PressToPlay() == 0) {
                            if (!login.empty()) {
                                Mix_PlayChannel(-1, button_sound, 0);

                                join_create_room = true;

                                activeChat = login;

                                p.send_packet(create_room, cip, *server);

                                p.recv_packet(cip, *server);

                                p.send_packet(activeChat, cip, *server);

                                p.recv_packet(cip, *server);

                                if (password == "") {
                                    p.send_packet(" ", cip, *server);
                                }
                                else {
                                    p.send_packet(password, cip, *server);
                                }

                                ack = p.recv_packet(cip, *server);

                                if (ack == "1000") {
                                    cout << "Succesfful create room " << activeChat << endl;
                                }
                                else {
                                    cout << "Failed to create room " << activeChat << endl;
                                    running = false;
                                    clear(renderer, window);
                                    quit = true;
                                    return 0;
                                }
                            }
                        }
                        else if (join_obj.PressToPlay() == 0) {
                            if (!login.empty()) {
                                Mix_PlayChannel(-1, button_sound, 0);

                                join_create_room = true;

                                p.send_packet(join_room, cip, *server);

                                string ack = p.recv_packet(cip, *server);

                                p.send_packet(login, cip, *server);

                                ack = p.recv_packet(cip, *server);

                                if (ack == "1000") {
                                    if (password == "") {
                                        p.send_packet(" ", cip, *server);
                                    }
                                    else {
                                        p.send_packet(password, cip, *server);
                                    }
                                    ack = p.recv_packet(cip, *server);

                                    if (ack == "1000") {
                                        cout << "Succesfful join room " << activeChat << endl;
                                        activeChat = login;
                                    }
                                    else {
                                        cout << "Failed to join room " << activeChat << endl;
                                        running = false;
                                        clear(renderer, window);
                                        quit = true;
                                        return 0;
                                    }
                                }
                                else {
                                    cout << "Failed to join room " << activeChat << endl;
                                    running = false;
                                    clear(renderer, window);
                                    quit = true;
                                    return 0;
                                }
                            }
                        }
                    }
                }

                SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
                SDL_RenderClear(renderer);

                SDL_RenderCopy(renderer, background, NULL, NULL);

                SDL_RenderCopy(renderer, logo, nullptr, &dstrect);

                SDL_Color color = { 255, 255, 255, 255 };

                renderText("Chat: " + count_chats + "/100", "SigmarOne-Regular.ttf", color, 30, renderer, positions[0].x - 5, positions[0].y - 70);

                SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
                SDL_SetRenderDrawColor(renderer, 0, 0, 0, 80);
                SDL_RenderFillRect(renderer, &login_board);

                SDL_RenderFillRect(renderer, &password_board);

                SDL_RenderCopy(renderer, create_button, nullptr, &create_obj_d);
                SDL_RenderCopy(renderer, join_button, nullptr, &join_obj_d);

                if (!login.empty()) {
                    renderText(login, "arialmt.ttf", color, 30, renderer, positions[3].x - 80, positions[3].y - 25);
                }
                if (!password.empty()) {
                    renderText(std::string(password.size(), '*'), "arialmt.ttf", color, 24, renderer, positions[3].x - 80, positions[5].y + 70);
                }

                SDL_RenderPresent(renderer);

                SDL_StopTextInput();
            }

            Session session(p, cip, server, running, messages, msgMutex, totalHeight, io_ctx, renderer, send_sound);

            while (true) {
                SDL_StartTextInput();
                while (SDL_PollEvent(&event)) {
                    if (event.type == SDL_QUIT) {
                        running = false;
                        clear(renderer, window);
                        quit = true;
                        join_login = true;

                        SDL_DestroyTexture(login_button);
                        SDL_DestroyTexture(register_button);
                        SDL_DestroyTexture(create_button);
                        SDL_DestroyTexture(join_button);
                        SDL_DestroyTexture(games_button);
                        SDL_DestroyTexture(chat_button);
                        SDL_DestroyTexture(background);
                        SDL_DestroyTexture(logo);

                        Mix_FreeChunk(button_sound);
                        Mix_FreeChunk(send_sound);
                        Mix_FreeMusic(menu_sound);

                        return 0;
                    }
                    else if (event.type == SDL_TEXTINPUT && !is_chats) {
                        buf.append(event.text.text);
                    }
                    else if (event.type == SDL_KEYDOWN) {
                        if (event.key.keysym.sym == SDLK_BACKSPACE && !buf.empty()) {
                            buf.pop_back();
                        }
                        else if (event.key.keysym.sym == SDLK_RETURN) {
                            Mix_PlayChannel(-1, send_sound, 0);
                            if (!buf.empty() && !is_chats) {
                                buf += " :" + username;

                                cout << buf << endl;

                                p.send_packet(buf + " |" + activeChat, cip, *server);

                                {
                                    lock_guard<mutex> lock(msgMutex);
                                    messages.insert(messages.begin(), { buf, 0 });
                                    totalHeight += MESSAGE_HEIGHT;
                                }

                                buf.clear();
                            }
                        }
                    }
                    else if (event.type == SDL_MOUSEWHEEL) {
                        scrollOffset += event.wheel.y * 30;

                        int maxScroll = max(0, totalHeight - board_messages.h);
                        scrollOffset = max(0, min(maxScroll, scrollOffset));
                    }
                    else if (event.type == SDL_MOUSEBUTTONDOWN) {
                        if (games_obj.PressToPlay() == 0) {
                            Mix_PlayChannel(-1, button_sound, 0);
                        }
                        else if (chat_obj.PressToPlay() == 0) {
                            Mix_PlayChannel(-1, button_sound, 0);
                            if (is_chats) {
                                is_chats = false;
                            }
                            else {
                                is_chats = true;
                                session.paused_.store(true);
                            }
                        }

                    }
                }

                SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
                SDL_RenderClear(renderer);

                SDL_RenderCopy(renderer, background, NULL, NULL);

                SDL_RenderCopy(renderer, logo, nullptr, &dstrect_chat);

                SDL_RenderCopy(renderer, games_button, nullptr, &games_obj_d);
                SDL_RenderCopy(renderer, chat_button, nullptr, &chat_obj_d);

                if (!is_chats) {

                    renderText(activeChat, "SigmarOne-Regular.ttf", { 255, 255, 255, 255 }, 15, renderer, 15, 30);

                    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
                    SDL_RenderFillRect(renderer, &board_messages);

                    SDL_Color color = { 255, 255, 255, 255 };
                    SDL_Color color_second = { 255, 255, 255, 180 };

                    if (!buf.empty()) {
                        renderText(buf, "arialmt.ttf",
                                   color, 20, renderer, 40, 650);
                    }
                    else {
                        renderText("Enter the text...", "arialmt.ttf",
                                   color_second, 20, renderer, 40, 650);
                    }

                    {
                        SDL_Color color = { 255, 255, 255, 255 };

                        lock_guard<mutex> lock(msgMutex);
                        int yPos = board_messages.y + board_messages.h - 80 - scrollOffset;

                        for (auto& msg : messages) {
                            msg.yPos = yPos;

                            if (yPos > board_messages.y && yPos < board_messages.y + board_messages.h) {
                                renderText(msg.text, "arialmt.ttf",
                                           color, 20, renderer, board_messages.x + 10, yPos);
                            }

                            yPos -= MESSAGE_HEIGHT;

                            if (yPos < board_messages.y - MESSAGE_HEIGHT) {
                                break;
                            }
                        }
                    }

                    SDL_RenderPresent(renderer);
                    SDL_StopTextInput();
                }
                else {
                    SDL_RenderFillRect(renderer, &board_messages);

                    SDL_Rect dstrect = {50, 110};

                    {
                    lock_guard<mutex> lock(chats_textures_messages_mutex);
                    for (auto& i : chats_textures_messages) {
                        renderText(i, "arialmt.ttf", {255, 255, 255, 255}, 20, renderer, dstrect.x, dstrect.y);
                        dstrect.y += 40;
                    }
                    }

                    SDL_RenderPresent(renderer);
                    SDL_StopTextInput();
                }
            }
        }
        catch (const system_error& e) {
            cerr << "Exception: " << e.what() << endl;
        }
    }

    return 0;
}
