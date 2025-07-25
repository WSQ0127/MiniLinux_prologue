#include <bits/stdc++.h>
#include "src/MiniLinux.h"
#include "src/Players.h"
using namespace std;

void game_loop() {
    Directory dire;
    vector<Player> players;
    int player_count;

    cout << "请输入玩家数量(2-4): ";
    cin >> player_count;
    player_count = max(2, min(4, player_count));

    // 初始化游戏
    for (int i = 1; i <= player_count; ++i) {
        players.emplace_back(i, dire);
        players.back().draw_cards(3); // 初始3张牌
    }

    // 游戏主循环
    int current_player = 0;
    while (true) {
        Player& p = players[current_player];
        cout << "\n===== 玩家 " << p.get_id() << " 的回合 =====\n";
        
        // 每回合开始时显示完整目录
        
        // 摸牌阶段
        cout << "摸牌阶段 - 获得2张牌\n";
        p.draw_cards(2);
        
        // 出牌阶段
        cout << "\n出牌阶段 (输入指令或skip跳过)\n";
        map<string, int> used_counts = {
            {"cd", 0}, {"mkdir", 0}, {"mv", 0}, 
            {"rm", 0}, {"echo", 0}, {"sudo", 0}
        };
        
        while (true) {
            dire.print();
            p.show_status(dire);
            p.show_hand();
            string cmd;
            cout << "选择要出的牌: ";
            cin >> cmd;
            
            if (cmd == "skip") break;
            if (!p.play_card(cmd, dire, used_counts)) {
                cout << "无效操作! 请重试\n";
                continue;
            }
        }
        
        // 弃牌阶段
        p.discard_excess_cards();
        
        // 检查淘汰
        if (p.is_eliminated()) {
            cout << "\n玩家 " << p.get_id() << " 已被淘汰!\n";
            players.erase(players.begin() + current_player);
            if (players.size() == 1) {
                cout << "游戏结束! 玩家 " << players[0].get_id() << " 获胜!\n";
                break;
            }
            current_player %= players.size();
            continue;
        }
        
        // 切换玩家
        current_player = (current_player + 1) % players.size();
    }
}

int main() {
    system("chcp 65001");
    srand(time(0));
    game_loop();
    return 0;
}