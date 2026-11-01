// Dictionary with Auto-Suggestions - C++ backend
// A Trie-based dictionary plus a tiny HTTP server (no external libraries).
// The HTML/CSS/JS frontend talks to it through a small JSON API.
//
// Build (Windows / MinGW): g++ dictionary.cpp -o dictionary -lws2_32
// Build (Mac / Linux)    : g++ dictionary.cpp -o dictionary
// Run                    : ./dictionary        (then open http://localhost:8080)

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <sstream>
#include <cstdio>
#include <cstdlib>
#include <cctype>
using namespace std;

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    typedef SOCKET socket_t;
    #define CLOSE_SOCKET closesocket
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <sys/time.h>
    #include <unistd.h>
    typedef int socket_t;
    #define INVALID_SOCKET (-1)
    #define CLOSE_SOCKET close
#endif

#ifdef MSG_NOSIGNAL
    #define SEND_FLAGS MSG_NOSIGNAL
#else
    #define SEND_FLAGS 0
#endif

// ======================= TRIE (the dictionary) =======================

struct TrieNode {
    TrieNode* children[26];
    bool isEnd;
    string meaning;          // meaning is stored on the node where a word ends

    TrieNode() {
        isEnd = false;

        for (int i = 0; i < 26; i++) {
            children[i] = NULL;
        }
    }
};

typedef vector< pair<string, string> > WordList;   // (word, meaning)

class Dictionary {
    TrieNode* root;

    void freeNode(TrieNode* node) {
        if (node == NULL) return;
        for (int i = 0; i < 26; i++) freeNode(node->children[i]);
        delete node;
    }

    // Walk down the trie following the prefix; NULL if the path does not exist
    TrieNode* walk(const string& prefix) {
        TrieNode* current = root;

        for (size_t i = 0; i < prefix.length(); i++) {
            int index = prefix[i] - 'a';

            if (current->children[index] == NULL) {
                return NULL;
            }

            current = current->children[index];
        }

        return current;
    }

    bool hasChildren(TrieNode* node) {
        for (int i = 0; i < 26; i++) {
            if (node->children[i] != NULL) return true;
        }
        return false;
    }

    // Recursive delete. Returns true if this node is now useless and can be freed.
    bool removeHelper(TrieNode* node, const string& word, size_t depth, bool& removed) {

        if (depth == word.length()) {
            if (!node->isEnd) return false;          // word was never inserted
            node->isEnd = false;                     // un-mark the word
            node->meaning = "";
            removed = true;
            return !hasChildren(node);               // free this node only if nothing hangs below it
        }

        int index = word[depth] - 'a';
        TrieNode* child = node->children[index];
        if (child == NULL) return false;             // path does not exist

        bool freeChild = removeHelper(child, word, depth + 1, removed);

        if (freeChild) {
            delete child;
            node->children[index] = NULL;
            return !node->isEnd && !hasChildren(node);   // keep going up while nodes are unused
        }
        return false;
    }

    // Collect suggestions using recursion
    void collect(TrieNode* current, string word, WordList& out) {

        if (current->isEnd) {
            out.push_back(make_pair(word, current->meaning));
        }

        for (int i = 0; i < 26; i++) {

            if (current->children[i] != NULL) {
                char ch = 'a' + i;
                collect(current->children[i], word + ch, out);
            }
        }
    }

public:

    Dictionary() { root = new TrieNode(); }
    ~Dictionary() { freeNode(root); }

    // Insert a word. Returns 1 = inserted, 2 = meaning updated, 0 = already existed
    int insert(const string& word, const string& meaning) {
        TrieNode* current = root;

        for (size_t i = 0; i < word.length(); i++) {
            int index = word[i] - 'a';

            if (current->children[index] == NULL) {
                current->children[index] = new TrieNode();
            }

            current = current->children[index];
        }

        bool isNew = !current->isEnd;
        current->isEnd = true;

        if (isNew) {
            current->meaning = meaning;
            return 1;
        }
        if (!meaning.empty() && meaning != current->meaning) {
            current->meaning = meaning;
            return 2;
        }
        return 0;
    }

    // Delete a word. Returns true if it existed and was removed.
    // Other words that share letters with it stay untouched.
    bool remove(const string& word) {
        bool removed = false;
        removeHelper(root, word, 0, removed);
        return removed;
    }

    // Search a word
    bool search(const string& word) {
        TrieNode* node = walk(word);
        return node != NULL && node->isEnd;
    }

    string getMeaning(const string& word) {
        TrieNode* node = walk(word);
        return (node != NULL && node->isEnd) ? node->meaning : "";
    }

    // Find all words with the given prefix (empty prefix = every word)
    WordList autoSuggest(const string& prefix) {
        WordList out;
        TrieNode* node = walk(prefix);
        if (node != NULL) collect(node, prefix, out);
        return out;
    }
};

// ======================= small helpers =======================

string jsonEscape(const string& s) {
    string out;
    for (size_t i = 0; i < s.size(); i++) {
        unsigned char c = (unsigned char)s[i];
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out += (char)c;
                }
        }
    }
    return out;
}

string urlDecode(const string& s) {
    string out;
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == '+') {
            out += ' ';
        } else if (s[i] == '%' && i + 2 < s.size() &&
                   isxdigit((unsigned char)s[i + 1]) && isxdigit((unsigned char)s[i + 2])) {
            out += (char)strtol(s.substr(i + 1, 2).c_str(), NULL, 16);
            i += 2;
        } else {
            out += s[i];
        }
    }
    return out;
}

map<string, string> parseQuery(const string& q) {
    map<string, string> params;
    size_t pos = 0;
    while (pos < q.size()) {
        size_t amp = q.find('&', pos);
        if (amp == string::npos) amp = q.size();
        string pair = q.substr(pos, amp - pos);
        size_t eq = pair.find('=');
        if (eq != string::npos) {
            params[urlDecode(pair.substr(0, eq))] = urlDecode(pair.substr(eq + 1));
        } else if (!pair.empty()) {
            params[urlDecode(pair)] = "";
        }
        pos = amp + 1;
    }
    return params;
}

string trim(const string& s) {
    size_t a = 0, b = s.size();
    while (a < b && isspace((unsigned char)s[a])) a++;
    while (b > a && isspace((unsigned char)s[b - 1])) b--;
    return s.substr(a, b - a);
}

string toLower(string s) {
    for (size_t i = 0; i < s.size(); i++) s[i] = (char)tolower((unsigned char)s[i]);
    return s;
}

// Words may only contain a-z (the trie has 26 slots per node)
bool isValidWord(const string& w) {
    if (w.empty() || w.size() > 45) return false;
    for (size_t i = 0; i < w.size(); i++) {
        if (w[i] < 'a' || w[i] > 'z') return false;
    }
    return true;
}

string wordListJson(const WordList& list) {
    string out = "[";
    for (size_t i = 0; i < list.size(); i++) {
        if (i > 0) out += ",";
        out += "{\"word\":\"" + jsonEscape(list[i].first) +
               "\",\"meaning\":\"" + jsonEscape(list[i].second) + "\"}";
    }
    return out + "]";
}

// ======================= request handling =======================

struct Response {
    int status;
    string type;
    string body;
};

Response jsonResponse(int status, const string& body) {
    Response r;
    r.status = status;
    r.type = "application/json; charset=utf-8";
    r.body = body;
    return r;
}

Response errorResponse(int status, const string& message) {
    return jsonResponse(status, "{\"error\":\"" + jsonEscape(message) + "\"}");
}

// API routes:
//   GET  /api/words                    -> every word with its meaning
//   GET  /api/suggest?prefix=app       -> words starting with the prefix
//   GET  /api/search?word=apple        -> is the word present, and its meaning
//   POST /api/insert?word=dog&meaning= -> insert a word (or update its meaning)
//   POST /api/delete?word=dog          -> delete a word
Response handleApi(const string& method, const string& path,
                   map<string, string>& params, Dictionary& dict) {

    if (path == "/api/words" && method == "GET") {
        return jsonResponse(200, wordListJson(dict.autoSuggest("")));
    }

    if (path == "/api/suggest" && method == "GET") {
        string prefix = toLower(trim(params["prefix"]));
        if (!isValidWord(prefix)) return errorResponse(400, "Prefix must contain letters a-z only.");
        return jsonResponse(200, "{\"prefix\":\"" + jsonEscape(prefix) + "\",\"words\":" +
                                 wordListJson(dict.autoSuggest(prefix)) + "}");
    }

    if (path == "/api/search" && method == "GET") {
        string word = toLower(trim(params["word"]));
        if (!isValidWord(word)) return errorResponse(400, "Word must contain letters a-z only.");
        bool found = dict.search(word);
        return jsonResponse(200, string("{\"word\":\"") + jsonEscape(word) +
                                 "\",\"found\":" + (found ? "true" : "false") +
                                 ",\"meaning\":\"" + jsonEscape(found ? dict.getMeaning(word) : "") + "\"}");
    }

    if (path == "/api/insert") {
        if (method != "POST") return errorResponse(405, "Use POST for /api/insert.");
        string word = toLower(trim(params["word"]));
        string meaning = trim(params["meaning"]);
        if (!isValidWord(word)) return errorResponse(400, "Word must contain letters a-z only.");
        if (meaning.size() > 300) meaning = meaning.substr(0, 300);

        int result = dict.insert(word, meaning);
        const char* status = result == 1 ? "inserted" : (result == 2 ? "updated" : "exists");
        return jsonResponse(200, string("{\"status\":\"") + status + "\",\"word\":\"" + jsonEscape(word) + "\"}");
    }

    if (path == "/api/delete") {
        if (method != "POST") return errorResponse(405, "Use POST for /api/delete.");
        string word = toLower(trim(params["word"]));
        if (!isValidWord(word)) return errorResponse(400, "Word must contain letters a-z only.");

        bool removed = dict.remove(word);
        return jsonResponse(200, string("{\"status\":\"") + (removed ? "deleted" : "notfound") +
                                 "\",\"word\":\"" + jsonEscape(word) + "\"}");
    }

    return errorResponse(404, "Unknown API route.");
}

// Read a frontend file. Only files on this whitelist can be served.
bool readFrontendFile(const string& name, string& content) {
    const char* folders[] = { "../frontend/", "frontend/", "./" };
    for (int i = 0; i < 3; i++) {
        ifstream file((string(folders[i]) + name).c_str(), ios::binary);
        if (file) {
            stringstream ss;
            ss << file.rdbuf();
            content = ss.str();
            return true;
        }
    }
    return false;
}

Response handleStatic(const string& path) {
    string file, type;
    if (path == "/" || path == "/index.html") { file = "index.html"; type = "text/html; charset=utf-8"; }
    else if (path == "/style.css")            { file = "style.css";  type = "text/css; charset=utf-8"; }
    else if (path == "/script.js")            { file = "script.js";  type = "text/javascript; charset=utf-8"; }
    else return errorResponse(404, "Not found.");

    Response r;
    r.type = type;
    if (!readFrontendFile(file, r.body)) {
        return errorResponse(404, "Frontend file not found. Run the server from the backend folder.");
    }
    r.status = 200;
    return r;
}

const char* statusText(int code) {
    switch (code) {
        case 200: return "OK";
        case 204: return "No Content";
        case 400: return "Bad Request";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        default:  return "Error";
    }
}

void sendAll(socket_t client, const string& data) {
    size_t sent = 0;
    while (sent < data.size()) {
        int n = send(client, data.c_str() + sent, (int)(data.size() - sent), SEND_FLAGS);
        if (n <= 0) break;
        sent += n;
    }
}

string readRequest(socket_t client) {
    string req;
    char buf[4096];
    while (req.find("\r\n\r\n") == string::npos && req.size() < 16384) {
        int n = recv(client, buf, sizeof(buf), 0);
        if (n <= 0) break;
        req.append(buf, n);
    }
    return req;
}

void handleClient(socket_t client, Dictionary& dict) {
    string req = readRequest(client);
    if (req.empty()) return;

    istringstream line(req.substr(0, req.find("\r\n")));
    string method, target, version;
    line >> method >> target >> version;

    string path = target, query;
    size_t q = target.find('?');
    if (q != string::npos) {
        path = target.substr(0, q);
        query = target.substr(q + 1);
    }
    map<string, string> params = parseQuery(query);

    Response res;
    if (method == "OPTIONS") {                       // CORS preflight
        res.status = 204;
    } else if (path.compare(0, 5, "/api/") == 0) {
        res = handleApi(method, path, params, dict);
    } else if (method == "GET") {
        res = handleStatic(path);
    } else {
        res = errorResponse(405, "Method not allowed.");
    }

    ostringstream out;
    out << "HTTP/1.1 " << res.status << " " << statusText(res.status) << "\r\n"
        << "Content-Type: " << (res.type.empty() ? "text/plain" : res.type) << "\r\n"
        << "Content-Length: " << res.body.size() << "\r\n"
        << "Access-Control-Allow-Origin: *\r\n"
        << "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
        << "Access-Control-Allow-Headers: Content-Type\r\n"
        << "Cache-Control: no-store\r\n"
        << "Connection: close\r\n\r\n"
        << res.body;
    sendAll(client, out.str());
}

// ======================= main =======================

int main(int argc, char* argv[]) {
    int port = 8080;
    const char* envPort = getenv("PORT");
    if (envPort != NULL) {
        port = atoi(envPort);
    }
    if (argc > 1) {
        port = atoi(argv[1]);
    }

    Dictionary dict;

    // Initial dictionary words
    dict.insert("apple", "A round fruit with red, green or yellow skin that grows on a tree.");
    dict.insert("application", "A formal request, or a software program designed for a particular task.");
    dict.insert("apply", "To make a formal request, or to put something to practical use.");
    dict.insert("appreciate", "To recognize the value of something, or to be thankful for it.");
    dict.insert("banana", "A long curved fruit with yellow skin and soft sweet flesh.");
    dict.insert("ball", "A round object used in games and sports.");
    dict.insert("bat", "A wooden club used to hit a ball, or a small flying mammal.");
    dict.insert("cat", "A small domesticated animal kept as a pet.");
    dict.insert("computer", "An electronic machine that stores and processes data.");
    dict.insert("code", "A set of instructions written for a computer, or a system of symbols.");

#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        cout << "Could not start Winsock.\n";
        return 1;
    }
#endif

    socket_t server = socket(AF_INET, SOCK_STREAM, 0);
    if (server == INVALID_SOCKET) {
        cout << "Could not create socket.\n";
        return 1;
    }

#ifndef _WIN32
    int yes = 1;
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR, (const char*)&yes, sizeof(yes));
#endif

    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons((unsigned short)port);
    for (size_t i = 0; i < sizeof(addr.sin_zero); i++) addr.sin_zero[i] = 0;

    if (bind(server, (sockaddr*)&addr, sizeof(addr)) < 0) {
        cout << "Could not use port " << port << ". Is the server already running?\n";
        return 1;
    }
    listen(server, 16);

    cout << "===== DICTIONARY WITH AUTO-SUGGESTIONS (C++ backend) =====\n";
    cout << "Server running at http://localhost:" << port << "\n";
    cout << "Open that link in your browser. Press Ctrl+C to stop.\n";

    while (true) {
        socket_t client = accept(server, NULL, NULL);
        if (client == INVALID_SOCKET) continue;

        // Don't wait forever for browsers that open a connection and send nothing
#ifdef _WIN32
        DWORD timeout = 2000;
        setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
#else
        struct timeval tv;
        tv.tv_sec = 2;
        tv.tv_usec = 0;
        setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
#endif

        handleClient(client, dict);
        CLOSE_SOCKET(client);
    }

    return 0;
}
