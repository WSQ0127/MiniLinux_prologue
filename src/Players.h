//本文件由 DS 辅助调试（（（ 
#ifndef PLAYERS_H
#define PLAYERS_H

#include "MiniLinux.h"
#include <windows.h>
#include <fstream>
#include <memory>

class Player {
private:
    int id;
    vector<string> hand;
    vector<int> path;
    int file_count = 3;
    shared_ptr<map<string, int>> card_limits;
    static vector<string> load_card_pool() {
        vector<string> pool;
        ifstream infile("data/cards.txt");
        if(!infile) {
            vector<pair<string, int>> defaults = {
                make_pair("cd", 40), 
                make_pair("mkdir", 20),
                make_pair("mv", 20),
                make_pair("rm", 16),
                make_pair("echo", 8),
                make_pair("sudo", 23)
            };
            for(const auto& entry : defaults) {
                for(int i = 0; i < entry.second; ++i) {
                    pool.push_back(entry.first);
                }
            }
            return pool;
        }
        string name;
        int count;
        while(infile >> name >> count) {
            for(int i = 0; i < count; ++i) {
                pool.push_back(name);
            }
        }
        return pool;
    }
    void print_current_dir(Directory& dir) {
        Directory::Node* current = dir.get_node(path.back());
        if(!current) return;
        cout << "\n当前目录 (" << current->name << ") 内容:\n";
        for(size_t i = 0; i < current->children.size(); ++i) {
            auto child = current->children[i];
            cout << " [" << (i+1) << "] " << child->name 
                 << (child->is_dir ? "/" : "")
                 << (!child->is_dir ? " (玩家" + to_string(child->owner) + ")" : "")
                 << endl;
        }
    }
public:
    Player(int i, Directory& dir) : id(i), 
        card_limits(make_shared<map<string, int>>()) {
        (*card_limits)["cd"] = 3;
        (*card_limits)["sudo"] = -1;
        (*card_limits)["mkdir"] = 1;
        (*card_limits)["mv"] = 1;
        (*card_limits)["rm"] = 1;
        (*card_limits)["echo"] = 1;
        
        int player_dir = dir.mkdir(1, "player" + to_string(id));
        path.push_back(1);
        path.push_back(player_dir);
        for(int j = 1; j <= 3; ++j) {
            dir.echo(player_dir, id, "file" + to_string(j));
        }
    }
    Player(Player&&) = default;
    Player& operator=(Player&&) = default;
    Player(const Player&) = delete;
    Player& operator=(const Player&) = delete;
    int get_id() const { return id; }
    int get_current_dir() const { return path.back(); }
    void draw_cards(int n) {
        static vector<string> card_pool = [](){
            auto pool = load_card_pool();
            random_shuffle(pool.begin(), pool.end());
            return pool;
        }();
        static size_t index = 0;
        
        for(int i = 0; i < n && index < card_pool.size(); ++i) {
            hand.push_back(card_pool[index++]);
        }
    }
    void show_hand() {
        wstring_convert<codecvt_utf8_utf16<wchar_t>> converter;
        wstring msg = L"玩家 " + to_wstring(id) + L" 的手牌:\n";
        for(const auto& card : hand) {
            msg += L" " + converter.from_bytes(card);
        }
        MessageBoxW(NULL, msg.c_str(), L"手牌", MB_OK);
    }
    bool play_card(const string& card, Directory& dir, map<string, int>& used_counts) {
        auto it = find(hand.begin(), hand.end(), card);
        if(it == hand.end()) {
            cout << "没有这张牌！\n";
            return false;
        }
        if(card_limits->at(card) != -1 && used_counts[card] >= card_limits->at(card)) {
            cout << card << " 本回合使用已达上限(" << card_limits->at(card) << "次)\n";
            return false;
        }
        bool sudo_used = (card == "sudo");
        string actual_card = card;
        if(sudo_used) {
            cout << "选择要强化的指令: ";
            string target;
            cin >> target;
            it = find(hand.begin(), hand.end(), target);
            if(it == hand.end()) {
                cout << "没有找到目标卡牌！\n";
                return false;
            }
            actual_card = *it;
        }
        bool success = false;
        int current_dir = path.back();
        print_current_dir(dir);
        if(actual_card == "cd") {
            cout << "输入要进入的子目录编号(0返回上级): ";
            int choice;
            cin >> choice;
            if(choice == 0 && path.size() > 1) {
                path.pop_back();
                success = true;
            }
            else if(choice > 0) {
                Directory::Node* current = dir.get_node(current_dir);
                if(current && choice <= current->children.size()) {
                    auto child = current->children[choice-1];
                    if(child->is_dir) {
                        path.push_back(child->id);
                        success = true;
                    }
                }
            }
        } 
        else if(actual_card == "mkdir") {
            cout << "输入新目录名: ";
            string name;
            cin >> name;
            success = dir.mkdir(current_dir, name);
        }
        else if(actual_card == "echo") {
            cout << "输入新文件名: ";
            string name;
            cin >> name;
            success = dir.echo(current_dir, id, name);
            if(success) file_count++;
        }
        else if(actual_card == "rm") {
            cout << "输入要删除的文件编号: ";
            int idx;
            cin >> idx;
            success = dir.rm(current_dir, idx-1, sudo_used ? -1 : id);
            if(success) file_count--;
        }
        else if(actual_card == "mv") {
            print_current_dir(dir);
            Directory::Node* current = dir.get_node(current_dir);
            cout << "输入要移动的文件编号(多个编号用空格分隔，0返回上级): ";
            string input;
            cin.ignore();
            getline(cin, input);
            
            istringstream iss(input);
            vector<int> file_indices;
            int index;
            while (iss >> index) {
                if(index > 0 && index <= current->children.size()) {
                    file_indices.push_back(index - 1);
                }
            }
            cout << "输入目标目录ID(0表示上级目录): ";
            int target_id;
            cin >> target_id;
            bool all_success = !file_indices.empty();
            sort(file_indices.rbegin(), file_indices.rend());
            for(int idx : file_indices) {
                if(!dir.mv(current_dir, idx, 
                         (target_id == 0) ? path[path.size()-2] : target_id, 
                         sudo_used ? -1 : id)) {
                    all_success = false;
                }
            }
            if(all_success) {
                cout << "\n操作成功！更新后的目录状态:\n";
                print_current_dir(dir);
                
                Directory::Node* target = dir.get_node(
                    (target_id == 0) ? path[path.size()-2] : target_id
                );
                if(target) {
                    cout << "\n目标目录 (" << target->name << ") 现在包含:\n";
                    for(size_t i = 0; i < target->children.size(); ++i) {
                        auto child = target->children[i];
                        cout << " [" << (i+1) << "] " << child->name 
                             << (child->is_dir ? "/" : "") << endl;
                    }
                }
            }
            else {
                cout << "文件移动失败！\n当前目录状态:\n";
                print_current_dir(dir);
            }
            
            success = all_success;
        }
        if(success) {
            hand.erase(find(hand.begin(), hand.end(), card));
            if(sudo_used) {
                hand.erase(it);
            }
            used_counts[card]++;
            if(sudo_used) used_counts[actual_card]++;
        }
        return success;
    }
    void discard() {
        const int max_keep = max(5, file_count * 3);
        int to_discard = hand.size() - max_keep;
        if(to_discard <= 0) return;
        cout << "\n=== 强制弃牌阶段 ===\n";
        cout << "手牌数: " << hand.size() << " (需保留≤" << max_keep << ")\n";
        cout << "必须弃掉 " << to_discard << " 张牌\n";
        while(to_discard > 0 && !hand.empty()) {
            show_hand();
            cout << "选择要弃掉的牌: ";
            string choice;
            cin >> choice;
            auto it = find(hand.begin(), hand.end(), choice);
            if(it != hand.end()) {
                hand.erase(it);
                to_discard--;
                cout << "已弃掉: " << choice << " (剩余需弃: " << to_discard << ")\n";
            }
            else {
                cout << "无效选择！\n";
            }
        }
    }
    bool is_eliminated() const { return file_count <= 0; }
    void show_status(Directory& dir) {
        cout << "玩家" << id << " 路径: ";
        for(int d : path) {
            Directory::Node* node = dir.get_node(d);
            if(node) cout << "/" << node->name;
        }
        cout << "\n文件数: " << file_count << endl;
    }
};

#endif