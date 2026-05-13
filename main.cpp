//本文件无 GPT&DS 参与（（（
#include <bits/stdc++.h>
#include "src/MiniLinux.h"
#include "src/Network.h"
#include "src/Players.h"
using namespace std;

static int read_int(Endpoint& io, const string& prompt_text, int fallback) {
    io.write(prompt_text);
    string line;
    if(!io.read_line(line)) return fallback;
    istringstream in(line);
    int value = fallback;
    in >> value;
    return value;
}

static void broadcast(const vector<shared_ptr<Endpoint>>& endpoints, const string& text) {
    set<Endpoint*> sent;
    for(const auto& ep : endpoints) {
        if(ep && sent.insert(ep.get()).second) ep->write(text);
    }
}

void game_loop(vector<shared_ptr<Endpoint>> endpoints) {
    Directory dire;
    vector<unique_ptr<Player>> players;

    int player_count = static_cast<int>(endpoints.size());
    player_count = max(2, min(4, player_count));
    endpoints.resize(player_count);

    for(int i = 1; i <= player_count; ++i) {
        players.emplace_back(make_unique<Player>(i, dire));
        players.back()->draw_cards(5);
        endpoints[i - 1]->write("你是玩家 " + to_string(i) + "。游戏开始时位于 /Home/player" + to_string(i) + "\n");
    }

    int current_player = 0;
    while(true) {
        Player& p = *players[current_player];
        Endpoint& io = *endpoints[current_player];

        broadcast(endpoints, "\n===== 玩家 " + to_string(p.get_id()) + " 的回合 =====\n");
        io.write("摸牌阶段 - 获得2张牌\n");
        p.draw_cards(2);

        map<string, int> used_counts = {
            {"cd", 0}, {"mkdir", 0}, {"mv", 0}, {"rm", 0}, {"echo", 0},
            {"sudo", 0}, {"ls", 0}, {"pwd", 0}, {"chmod", 0}
        };

        while(true) {
            io.write(dire.to_string_tree());
            p.show_status(dire, io);
            p.show_cards(io);

            ostringstream used_text;
            used_text << "已使用: ";
            bool any_used = false;
            for(const auto& entry : used_counts) {
                if(entry.second > 0) {
                    used_text << entry.first << "×" << entry.second << " ";
                    any_used = true;
                }
            }
            if(!any_used) used_text << "无";
            used_text << "\n";
            io.write(used_text.str());

            io.write("选择要出的牌(或输入skip结束): ");
            string cmd;
            if(!io.read_line(cmd) || cmd == "skip") break;
            if(!p.play_cards(cmd, dire, used_counts, io, players)) {
                io.write("出牌失败！请重试\n");
            }
        }

        p.discard(dire, io);
        vector<int> eliminated_positions;
        for(size_t i = 0; i < players.size(); ++i) {
            if(players[i]->is_eliminated(dire)) eliminated_positions.push_back(static_cast<int>(i));
        }

        bool current_erased = false;
        for(auto it = eliminated_positions.rbegin(); it != eliminated_positions.rend(); ++it) {
            int pos = *it;
            int eliminated_id = players[pos]->get_id();
            broadcast(endpoints, "\n玩家 " + to_string(eliminated_id) + " 已被淘汰!\n");
            players.erase(players.begin() + pos);
            endpoints.erase(endpoints.begin() + pos);
            if(pos == current_player) current_erased = true;
            else if(pos < current_player) current_player--;
        }

        if(players.size() == 1) {
            broadcast(endpoints, "游戏结束! 玩家 " + to_string(players[0]->get_id()) + " 获胜!\n");
            break;
        }

        if(current_erased) {
            if(current_player >= static_cast<int>(players.size())) current_player = 0;
        } else {
            current_player = (current_player + 1) % static_cast<int>(players.size());
        }
    }
}

void local_game() {
    auto console = make_shared<ConsoleEndpoint>();
    int player_count = read_int(*console, "请输入玩家数量(2-4): ", 2);
    player_count = max(2, min(4, player_count));
    vector<shared_ptr<Endpoint>> endpoints;
    for(int i = 0; i < player_count; ++i) endpoints.push_back(console);
    game_loop(endpoints);
}

void host_game(int port) {
    NetworkRuntime runtime;
    auto console = make_shared<ConsoleEndpoint>();
    int player_count = read_int(*console, "请输入玩家数量(2-4): ", 2);
    player_count = max(2, min(4, player_count));

    SocketHandle server_fd = create_server_socket(port);
    cout << "局域网房间已开启，端口 " << port << "。\n";
    cout << "其他玩家运行: ./minilinux --connect <你的局域网IP> " << port << "\n";

    vector<shared_ptr<Endpoint>> endpoints;
    endpoints.push_back(console);
    for(int i = 2; i <= player_count; ++i) {
        cout << "等待玩家 " << i << " 连接..." << endl;
        SocketHandle client = accept_client(server_fd);
        if(client == INVALID_SOCKET_HANDLE) {
            close_socket(server_fd);
            throw runtime_error("接受客户端连接失败");
        }
        auto remote = make_shared<SocketEndpoint>(client);
        remote->write("连接成功，你将作为玩家 " + to_string(i) + " 加入。\n");
        endpoints.push_back(remote);
        cout << "玩家 " << i << " 已连接。" << endl;
    }
    close_socket(server_fd);
    game_loop(endpoints);
}

int main(int argc, char* argv[]) {
#ifdef _WIN32
    system("chcp 65001");
#endif
    srand(static_cast<unsigned>(time(0)));

    try {
        if(argc >= 2 && string(argv[1]) == "--host") {
            int port = argc >= 3 ? stoi(argv[2]) : 5555;
            host_game(port);
        }
        else if(argc >= 4 && string(argv[1]) == "--connect") {
            run_client(argv[2], stoi(argv[3]));
        }
        else {
            cout << "MiniLinux: Prologue\n";
            cout << "本机模式: 直接运行程序\n";
            cout << "联机开房: " << argv[0] << " --host [端口]\n";
            cout << "加入房间: " << argv[0] << " --connect <房主IP> <端口>\n\n";
            local_game();
        }
    }
    catch(const exception& e) {
        cerr << "错误: " << e.what() << endl;
        return 1;
    }
    return 0;
}
