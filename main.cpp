//本文件无 GPT&DS 参与（（（ 
#include <bits/stdc++.h>
#include "src/MiniLinux.h"
#include "src/Players.h"
using namespace std;

void game_loop() {
    Directory dire;
    vector<unique_ptr<Player>> players;
    int player_count;
    cout << "请输入玩家数量(2-4): ";
    cin >> player_count;
    player_count = max(2, min(4, player_count));
    for(int i = 1; i <= player_count; ++i) {
        players.emplace_back(make_unique<Player>(i, dire));
        players.back()->draw_cards(3);
    }
    int current_player = 0;
    while(true) {
        Player& p = *players[current_player];
        cout << "\n===== 玩家 " << p.get_id() << " 的回合 =====\n";
        cout << "摸牌阶段 - 获得2张牌\n";
        p.draw_cards(2);
        map<string, int> used_counts = {
            {"cd",0},{"mkdir",0},{"mv",0},
            {"rm",0},{"echo",0},{"sudo",0}
        };
        while(1) {
            dire.print();
            p.show_status(dire);
            p.show_hand();
            cout << "已使用: ";
            for(const auto& entry : used_counts) {
                if(entry.second > 0) {
                    cout << entry.first << "×" << entry.second << " ";
                }
            }
            cout << "\n选择要出的牌(或输入skip结束): ";
            string cmd;
            cin >> cmd;
            if(cmd == "skip")
                break;
            if(!p.play_card(cmd, dire, used_counts)) {
                cout << "出牌失败！请重试\n";
            }
        }
        p.discard();
        if(p.is_eliminated()) {
            cout << "\n玩家 " << p.get_id() << " 已被淘汰!\n";
            players.erase(players.begin() + current_player);
            
            if(players.size() == 1) {
                cout << "游戏结束! 玩家 " << (*players[0]).get_id() << " 获胜!\n";
                break;
            }
            
            if(current_player >= players.size()) {
                current_player = 0;
            }
            continue;
        }
        
        current_player = (current_player + 1) % players.size();
    }
}

int main() {
    system("chcp 65001");
    srand(time(0));
    game_loop();
    return 0;
}