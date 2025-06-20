#ifndef CIPHER_H
#define CIPHER_H

#include <string>

using namespace std;

class XorCipher {
public:
    string cipher(string message, string key) {
        string result;

        if (key.size() == 0) {
            return message;
        }

        result.resize(message.size());

        for (int i = 0; i < message.size(); ++i) {
            result[i] = message[i] ^ key[i % key.size()];
        }

        return result;
    }

    string decrypt(string cipher_message, string key, ssize_t nread) {
        string result;

        if (key.size() == 0) {
            return cipher_message;
        }

        result.resize(nread);

        for (int i = 0; i < nread; ++i) {
            result[i] = cipher_message[i] ^ key[i % key.size()];
        }

        return result;
    }
};

#endif // CIPHER_H
