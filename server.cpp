#include <iostream>
#include <boost/asio.hpp>
#include <thread>
#include <vector>
#include <mutex>
#include <fstream>
#include <map>
#include "cipher.h"

using namespace std;
using namespace boost::asio;

using ip::tcp;

const string key = "Hk8#2FpL9qR$5tW!zX*cV6%bU7nY4@eSdGfJhMjK";

vector<shared_ptr<tcp::socket>> clients;

std::mutex db_lock;
std::mutex clients_mutex;

string create_room = "5000";
string join_room = "5001";
string big_chats = "5002";
string big_chats_end = "5003";

map<string, vector<shared_ptr<tcp::socket>>> chats;
map<string, string> password;

class Packets {
public:
    bool send_packet(string message, XorCipher& cip, tcp::socket& client) {
        boost::system::error_code error;

        write(client, buffer(cip.cipher(message, key)), error);

        if (error) {
            cout << "Error send packet to client" << endl;
            return 0;
        } else {
            return 1;
        }
    }

    string recv_packet(XorCipher& cip, tcp::socket& client) {
        array<char, 1024> data;
        boost::system::error_code error;

        size_t length = client.read_some(buffer(data), error);

        if (length <= 0) {
            return "";
        } else {
            return cip.decrypt(string(data.data(), length), key, length);
        }
    }
};

class Session {
public:
    Session(Packets& p, XorCipher& cip, shared_ptr<tcp::socket> client) : p_(p), cip_(cip), socket_(client)  {
        while (true) {
            string message = p.recv_packet(cip, *client);

            int i = 0;
            bool is_you = false;

            if (message == big_chats) {
                vector<string> public_chats;

                for (auto& chat : chats) {
                    bool is_you = false;
                    auto& clients_vec = chat.second;

                    clients_vec.erase(
                        remove_if(clients_vec.begin(), clients_vec.end(),
                                  [&](const shared_ptr<tcp::socket>& s) {
                                      if (s == client) {
                                          is_you = true;
                                          return false;
                                      }
                                      return !s->is_open();
                                  }),
                        clients_vec.end()
                        );

                    if (!clients_vec.empty() &&
                        clients_vec.size() >= 2 &&
                        !is_you &&
                        password[chat.first] == " ") {
                        public_chats.push_back(chat.first +  " |" + " members: " + to_string(clients_vec.size()));
                    }
                }

                for (int i = 0; i < min(5, (int)public_chats.size()); i++) {
                    p.send_packet(public_chats[i], cip, *client);

                    while (p.recv_packet(cip, *client) != "1000") {
                        p.send_packet(public_chats[i], cip, *client);
                    }
                }
                p.send_packet(big_chats_end, cip, *client);

                while (p.recv_packet(cip, *client) != "1000") {
                    p.send_packet(big_chats_end, cip, *client);
                }

                continue;
            }

            if (message.empty()) {
                cout << "Client disconnected from server" << endl;
                break;
            }

            size_t pos = message.find("|");
            string activeChat;

            if (pos != string::npos) {
                activeChat = message.substr(pos + 1);
                cout << message << endl;
                message = message.substr(0, pos);
            } else {
                cerr << "Failed put message from client" << endl;
                continue;
            }

            vector<shared_ptr<tcp::socket>> recipients;
            {
                lock_guard<mutex> lock(clients_mutex);
                recipients = clients;
            }

            for (auto it = chats[activeChat].begin(); it != chats[activeChat].end(); it++) {
                if ((*it)->is_open() && (*it) != client) {
                    p.send_packet(message, cip, *(*it));
                }
            }

            for (auto it = chats.begin(); it != chats.end(); ) {
                auto& clients_vec = it->second;
                clients_vec.erase(
                    std::remove_if(clients_vec.begin(), clients_vec.end(),
                                   [](const shared_ptr<tcp::socket>& s) { return !s->is_open(); }),
                    clients_vec.end()
                    );

                if (clients_vec.empty()) {
                    it = chats.erase(it);
                } else {
                    ++it;
                }
            }
        }
    }

    ~Session() {
        if (receiver_.joinable()) receiver_.join();
        if (socket_->is_open()) socket_->close();
    }

private:
    Packets& p_;
    XorCipher& cip_;
    shared_ptr<tcp::socket> socket_;
    thread receiver_;
};

class Login {
public:
    bool login(const std::string& filename, Packets& p, XorCipher& cip, shared_ptr<tcp::socket> client) {
        lock_guard<std::mutex> lock(db_lock);

        ifstream file(filename);
        if (!file.is_open()) {
            cerr << "Failed to open database to reading" << endl;
            return 0;
        }
        string login = p.recv_packet(cip, *client);
        p.send_packet("1000", cip, *client);
        string password = p.recv_packet(cip, *client);

        string line;
        while (getline(file, line)) {
            if (line == login + " " + password) {
                return 1;
            }
        }
        cerr << login << " " << password << " is incorrect" << endl;
        return 0;
    }


    bool registration(const std::string& filename, Packets& p, XorCipher& cip, shared_ptr<tcp::socket> client) {
        lock_guard<std::mutex> lock(db_lock);

        ifstream file_in(filename);
        string login = p.recv_packet(cip, *client);
        p.send_packet("1000", cip, *client);
        string password = p.recv_packet(cip, *client);

        string line;
        while (getline(file_in, line)) {
            if (line.find(login + " ") == 0) {
                cerr << "Login " << login << " already exists" << endl;
                return 0;
            }
        }
        file_in.close();

        ofstream file_out(filename, ios::app);
        if (!file_out.is_open()) {
            cerr << "Failed to open database to writing" << endl;
            return 0;
        }
        file_out << login << " " << password << '\n';
        file_out.close();
        return 1;
    }
};

int main()
{
    try {
        io_context io_ctx;

        tcp::endpoint endpoint(ip::address_v4::any(), 8088);

        tcp::acceptor acceptor(io_ctx, endpoint);

        XorCipher cip;
        Packets p;
        Login join;

        cout << "Server listen port 8088" << endl;

        while (true) {
            tcp::socket socket(io_ctx);

            acceptor.accept(socket);

            auto sock_ptr = make_shared<tcp::socket>(std::move(socket));

            std::thread session_thread([&, sock_ptr]() {
                string ack = p.recv_packet(cip, *sock_ptr);

                p.send_packet("1000", cip, *sock_ptr);

                if (ack == "login") {
                    bool result = join.login("/Users/gooseexet/BoostServer/build/Desktop-Debug/bd.db.rtf", p, cip, sock_ptr);

                    if (result == 0) {
                        cerr << "Failed to login in account from client" << endl;
                        p.send_packet("999", cip, *sock_ptr);
                        return;
                    } else if (result == 1) {
                        cout << "Client succesfull login in account" << endl;
                        p.send_packet("1000", cip, *sock_ptr);
                    }
                } else if (ack == "register") {
                    bool result = join.registration("/Users/gooseexet/BoostServer/build/Desktop-Debug/bd.db.rtf", p, cip, sock_ptr);

                    if (result == 0) {
                        cerr << "Failed to registration in account from client" << endl;
                        p.send_packet("999", cip, *sock_ptr);
                        return;
                    } else if (result == 1) {
                        cout << "Client succesfull registration in account" << endl;
                        p.send_packet("1000", cip, *sock_ptr);
                    }
                } else {
                    cerr << "Unknown ack for login or regist from client" << endl;
                    return;
                }

                {
                    lock_guard<mutex> lock(clients_mutex);
                    clients.push_back(sock_ptr);
                }

                for (auto it = chats.begin(); it != chats.end(); ) {
                    auto& clients_vec = it->second;
                    clients_vec.erase(
                        std::remove_if(clients_vec.begin(), clients_vec.end(),
                                       [](const shared_ptr<tcp::socket>& s) { return !s->is_open(); }),
                        clients_vec.end()
                        );

                    if (clients_vec.empty()) {
                        it = chats.erase(it);
                    } else {
                        ++it;
                    }
                }

                {
                    lock_guard<mutex> lock(clients_mutex);
                    p.send_packet(to_string(chats.size()), cip, *sock_ptr);
                }

                ack = p.recv_packet(cip, *sock_ptr);

                if (ack == create_room) {
                    p.send_packet("1000", cip, *sock_ptr);

                    string name = p.recv_packet(cip, *sock_ptr);

                    p.send_packet("1000", cip, *sock_ptr);

                    auto it = chats.find(name);

                    if (it == chats.end()) {
                        if (it == chats.end()) {
                            {
                                lock_guard<mutex> lock(clients_mutex);
                                chats[name].push_back(sock_ptr);
                            }

                            string password_name = p.recv_packet(cip, *sock_ptr);

                            {
                                lock_guard<mutex> lock(clients_mutex);
                                password[name] = password_name;
                            }

                            p.send_packet("1000", cip, *sock_ptr);

                            Session sess(p, cip, sock_ptr);
                        }
                    } else {
                        p.send_packet("999", cip, *sock_ptr);
                        cerr << "Failed create room" << endl;
                    }
                } else if (ack == join_room) {
                    p.send_packet("1000", cip, *sock_ptr);

                    string name = p.recv_packet(cip, *sock_ptr);

                    p.send_packet("1000", cip, *sock_ptr);

                    auto it = chats.find(name);

                    if (it != chats.end()) {
                        string password_name = p.recv_packet(cip, *sock_ptr);

                        if (password.find(name) != password.end()) {
                            if (password[name] == password_name) {
                                p.send_packet("1000", cip, *sock_ptr);

                                {
                                    lock_guard<mutex> lock(clients_mutex);
                                    chats[name].push_back(sock_ptr);
                                }

                                Session sess(p, cip, sock_ptr);
                            } else {
                                cerr << "Incorrect password, failed join to room" << endl;
                                p.send_packet("999", cip, *sock_ptr);
                            }
                        } else {
                            cerr << "Name is not in passwords, failed join to room" << endl;
                            p.send_packet("999", cip, *sock_ptr);
                        }
                    } else {
                        p.send_packet("999", cip, *sock_ptr);
                        cerr << "Uknown name room, failed join to room" << endl;
                    }
                } else {
                    cerr << "Failed create or join to room" << endl;

                    sock_ptr->close();

                    {
                        lock_guard<mutex> lock(clients_mutex);
                        auto it = std::find(clients.begin(), clients.end(), sock_ptr);
                        if (it != clients.end()) {
                            clients.erase(it);
                        }
                    }

                    return;
                }
            });

            session_thread.detach();
        }

    } catch (const system_error& e) {
        cerr << "Server Error: " << e.what() << endl;
    }

    return 0;
}
