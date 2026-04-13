#include <iostream>
#include <sstream>
#include <string>
#include <cstring>
#include <vector>
#include <thread>
#include <signal.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#define PORT 8081
#define AI_BRIDGE_PORT 5000
#define STREAM_PORT 8080

std::string call_ai_bridge(const std::string& question) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(AI_BRIDGE_PORT);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(sock);
        return "{\"error\":\"AI bridge not available\"}";
    }

    std::string escaped = question;
    size_t pos = 0;
    while ((pos = escaped.find('"', pos)) != std::string::npos) {
        escaped.replace(pos, 1, "\\\"");
        pos += 2;
    }

    std::string body = "{\"question\":\"" + escaped + "\"}";
    std::string request =
        "POST /ask HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: " + std::to_string(body.size()) + "\r\n"
        "Connection: close\r\n\r\n" + body;

    send(sock, request.c_str(), request.size(), 0);

    std::string response;
    char buf[4096];
    int n;
    while ((n = recv(sock, buf, sizeof(buf)-1, 0)) > 0) {
        buf[n] = 0;
        response += buf;
    }
    close(sock);

    size_t json_start = response.find("\r\n\r\n");
    if (json_start != std::string::npos)
        return response.substr(json_start + 4);
    return "{\"error\":\"invalid response\"}";
}

void proxy_stream(int client_fd) {
    int cam_sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(STREAM_PORT);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (connect(cam_sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        const char* err = "HTTP/1.1 503 Service Unavailable\r\nContent-Length: 0\r\n\r\n";
        send(client_fd, err, strlen(err), 0);
        close(cam_sock);
        return;
    }

    const char* req = "GET / HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n";
    send(cam_sock, req, strlen(req), 0);

    char buf[65536];
    int n;
    while ((n = recv(cam_sock, buf, sizeof(buf), 0)) > 0) {
        if (send(client_fd, buf, n, 0) < 0) break;
    }
    close(cam_sock);
}

const std::string HTML_PAGE = R"HTML(
<!DOCTYPE html>
<html>
<head>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>AI Rover Assistant</title>
    <style>
        * { box-sizing: border-box; margin: 0; padding: 0; }
        body {
            font-family: Arial, sans-serif;
            background: #0d1117;
            color: #e6edf3;
            height: 100vh;
            display: flex;
            flex-direction: column;
        }
        header {
            background: #161b22;
            padding: 12px 20px;
            border-bottom: 1px solid #30363d;
            font-size: 18px;
            font-weight: bold;
            color: #58a6ff;
        }
        .main {
            display: flex;
            flex: 1;
            overflow: hidden;
        }
        .camera-panel {
            flex: 1;
            background: #000;
            border-right: 1px solid #30363d;
            display: flex;
            align-items: center;
            justify-content: center;
        }
        .camera-panel img {
            width: 100%;
            height: 100%;
            object-fit: contain;
        }
        .chat-panel {
            width: 380px;
            display: flex;
            flex-direction: column;
            background: #161b22;
        }
        .chat-messages {
            flex: 1;
            overflow-y: auto;
            padding: 16px;
            display: flex;
            flex-direction: column;
            gap: 12px;
        }
        .msg {
            padding: 10px 14px;
            border-radius: 8px;
            max-width: 90%;
            line-height: 1.5;
            font-size: 14px;
            white-space: pre-wrap;
        }
        .msg.user { background: #1f6feb; align-self: flex-end; }
        .msg.ai { background: #21262d; border: 1px solid #30363d; align-self: flex-start; }
        .msg.system { background: #1a2a1a; border: 1px solid #238636; align-self: center; font-size: 12px; color: #3fb950; }
        .chat-input {
            padding: 12px;
            border-top: 1px solid #30363d;
            display: flex;
            gap: 8px;
        }
        .chat-input input {
            flex: 1;
            padding: 10px 14px;
            background: #0d1117;
            border: 1px solid #30363d;
            border-radius: 6px;
            color: #e6edf3;
            font-size: 14px;
            outline: none;
        }
        .chat-input input:focus { border-color: #58a6ff; }
        .chat-input button {
            padding: 10px 16px;
            background: #1f6feb;
            border: none;
            border-radius: 6px;
            color: white;
            font-size: 14px;
            cursor: pointer;
        }
        .chat-input button:hover { background: #388bfd; }
        .chat-input button:disabled { background: #30363d; cursor: not-allowed; }
        .thinking { display: none; padding: 8px 14px; color: #8b949e; font-size: 13px; font-style: italic; }
    </style>
</head>
<body>
<header>🤖 AI Rover Assistant</header>
<div class="main">
    <div class="camera-panel">
        <img src="/stream" alt="Camera Feed" />
    </div>
    <div class="chat-panel">
        <div class="chat-messages" id="messages">
            <div class="msg system">AI Assistant ready (Gemini 2.5 Flash). Ask me anything about what the camera sees!</div>
        </div>
        <div class="thinking" id="thinking">Gemini is thinking...</div>
        <div class="chat-input">
            <input type="text" id="input" placeholder="Ask about what you see..."
                   onkeypress="if(event.key==='Enter') sendMessage()"/>
            <button id="btn" onclick="sendMessage()">Ask</button>
        </div>
    </div>
</div>
<script>
function sendMessage() {
    const input = document.getElementById('input');
    const question = input.value.trim();
    if (!question) return;
    addMessage(question, 'user');
    input.value = '';
    document.getElementById('btn').disabled = true;
    document.getElementById('thinking').style.display = 'block';
    fetch('/ask', {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify({question: question})
    })
    .then(r => r.json())
    .then(data => { addMessage(data.answer || data.error, 'ai'); })
    .catch(e => { addMessage('Error: ' + e.message, 'ai'); })
    .finally(() => {
        document.getElementById('btn').disabled = false;
        document.getElementById('thinking').style.display = 'none';
    });
}

function addMessage(text, type) {
    const msgs = document.getElementById('messages');
    const div = document.createElement('div');
    div.className = 'msg ' + type;
    div.textContent = text;
    msgs.appendChild(div);
    msgs.scrollTop = msgs.scrollHeight;
}
</script>
</body>
</html>
)HTML";

void handle_client(int client_fd) {
    char buf[4096] = {};
    recv(client_fd, buf, sizeof(buf)-1, 0);
    std::string request(buf);

    std::string method, path;
    std::istringstream ss(request);
    ss >> method >> path;

    if (method == "GET" && path == "/") {
        std::string response =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/html\r\n"
            "Content-Length: " + std::to_string(HTML_PAGE.size()) + "\r\n\r\n" + HTML_PAGE;
        send(client_fd, response.c_str(), response.size(), 0);

    } else if (method == "GET" && path == "/stream") {
        proxy_stream(client_fd);

    } else if (method == "POST" && path == "/ask") {
        size_t body_start = request.find("\r\n\r\n");
        std::string body = (body_start != std::string::npos) ? request.substr(body_start + 4) : "";

        size_t q_start = body.find("\"question\":\"");
        std::string question = "What do you see?";
        if (q_start != std::string::npos) {
            q_start += 12;
            size_t q_end = body.find("\"", q_start);
            if (q_end != std::string::npos)
                question = body.substr(q_start, q_end - q_start);
        }

        std::string ai_response = call_ai_bridge(question);
        std::string response =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: application/json\r\n"
            "Access-Control-Allow-Origin: *\r\n"
            "Content-Length: " + std::to_string(ai_response.size()) + "\r\n\r\n" + ai_response;
        send(client_fd, response.c_str(), response.size(), 0);

    } else {
        const char* not_found = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n";
        send(client_fd, not_found, strlen(not_found), 0);
    }

    close(client_fd);
}

int main() {
    signal(SIGPIPE, SIG_IGN);

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT);

    bind(server_fd, (struct sockaddr*)&addr, sizeof(addr));
    listen(server_fd, 10);
    std::cout << "UI server running on port " << PORT << "\n";

    while (true) {
        int client_fd = accept(server_fd, nullptr, nullptr);
        if (client_fd >= 0)
            std::thread(handle_client, client_fd).detach();
    }
    return 0;
}
