#ifndef PLAYERS_H
#define PLAYERS_H

#include "MiniLinux.h"
#include <windows.h>
#include <fstream>

class Player {
private:
    int id;
    vector<string> hand;
    vector<int> path;
    int file_count = 3;
    
    static vector<string> load_card_pool() {
        vector<string> pool;
        ifstream infile("data/cards.txt");
        if (!infile) {
            vector<pair<string, int>> defaults = {
                {"cd", 40}, {"mkdir", 20}, {"mv", 20},
                {"rm", 16}, {"echo", 8}, {"sudo", 23}
            };
            for (auto& [name, count] : defaults) {
                for (int i = 0; i < count; ++i) pool.push_back(name);
            }
            return pool;
        }
        string name;
        int count;
        while (infile >> name >> count) {
            for (int i = 0; i < count; ++i) pool.push_back(name);
        }
        return pool;
    }

    void print_current_dir(Directory& dir) {
        Directory::Node* current = dir.get_node(path.back());
        if (!current) return;
        
        cout << "\n当前目录 (" << current->name << ") 内容:\n";
        for (size_t i = 0; i < current->children.size(); ++i) {
            auto child = current->children[i];
            cout << " [" << (i+1) << "] " << child->name 
                 << (child->is_dir ? "/" : "")
                 << (!child->is_dir ? " (玩家" + to_string(child->owner) + ")" : "")
                 << endl;
        }
    }

public:
    Player(int i, Directory& dir) : id(i) {
        int player_dir = dir.mkdir(1, "player" + to_string(id));
        path.push_back(1);
        path.push_back(player_dir);
        for (int j = 1; j <= 3; ++j) {
            dir.echo(player_dir, id, "file" + to_string(j));
        }
    }

    int get_id() const { return id; }
    int get_current_dir() const { return path.back(); }

    void draw_cards(int n) {
        static vector<string> card_pool = [](){
            auto pool = load_card_pool();
            random_shuffle(pool.begin(), pool.end());
            return pool;
        }();
        static size_t index = 0;
        
        for (int i = 0; i < n && index < card_pool.size(); ++i) {
            hand.push_back(card_pool[index++]);
        }
    }

    void show_hand() {
        wstring_convert<codecvt_utf8_utf16<wchar_t>> converter;
        wstring msg = L"玩家 " + to_wstring(id) + L" 的手牌:\n";
        for (const auto& card : hand) {
            msg += L" " + converter.from_bytes(card);
        }
        MessageBoxW(NULL, msg.c_str(), L"手牌", MB_OK);
    }

    bool play_card(const string& card, Directory& dir, map<string, int>& used_counts) {
        auto it = find(hand.begin(), hand.end(), card);
        if (it == hand.end()) return false;

        // 检查使用限制
        if (card != "sudo") {
            int max_use = (card == "cd") ? 2 : 1;
            if (used_counts[card] >= max_use) {
                cout << card << " 本回合使用已达上限(" << max_use << "次)\n";
                return false;
            }
        }

        bool sudo_used = (card == "sudo");
        if (sudo_used) {
            cout << "选择要强化的指令: ";
            string target;
            cin >> target;
            it = find(hand.begin(), hand.end(), target);
            if (it == hand.end()) return false;
            used_counts["sudo"]++;
        }

        bool success = false;
        int current_dir = path.back();
        print_current_dir(dir);

        if (*it == "cd") {
            cout << "输入要进入的子目录编号(0返回上级): ";
            int choice;
            cin >> choice;
            if (choice == 0 && path.size() > 1) {
                path.pop_back();
                success = true;
            } else if (choice > 0) {
                Directory::Node* current = dir.get_node(current_dir);
                if (current && choice <= current->children.size()) {
                    auto child = current->children[choice-1];
                    if (child->is_dir) {
                        path.push_back(child->id);
                        success = true;
                    }
                }
            }
        } 
        else if (*it == "mkdir") {
            cout << "输入新目录名: ";
            string name;
            cin >> name;
            success = dir.mkdir(current_dir, name);
        }
        else if (*it == "echo") {
            cout << "输入新文件名: ";
            string name;
            cin >> name;
            success = dir.echo(current_dir, id, name);
            if (success) file_count++;
        }
        else if (*it == "rm") {
            cout << "输入要删除的文件编号: ";
            int idx;
            cin >> idx;
            success = dir.rm(current_dir, idx-1, sudo_used ? -1 : id);
            if (success) file_count--;
        }
        else if (*it == "mv") {
            print_current_dir(dir);
            Directory::Node* current = dir.get_node(current_dir);
            
            // 选择要移动的文件
            cout << "输入要移动的文件编号(多个编号用空格分隔，0返回上级): ";
            string input;
            cin.ignore();
            getline(cin, input);
            
            // 解析文件编号
            istringstream iss(input);
            vector<int> file_indices;
            int index;
            while (iss >> index) {
                if (index > 0 && index <= current->children.size()) {
                    file_indices.push_back(index - 1);
                }
            }
        
            // 选择目标目录
            cout << "输入目标目录ID(0表示上级目录): ";
            int target_id;
            cin >> target_id;
            
            // 执行移动
            bool all_success = !file_indices.empty();
            sort(file_indices.rbegin(), file_indices.rend());
            for (int idx : file_indices) {
                if (!dir.mv(current_dir, idx, 
                           (target_id == 0) ? path[path.size()-2] : target_id, 
                           sudo_used ? -1 : id)) {
                    all_success = false;
                }
            }
        
            // 显示操作结果
            if (all_success) {
                cout << "\n操作成功！更新后的目录状态:\n";
                print_current_dir(dir);  // 显示移动后的当前目录
                
                // 显示目标目录状态
                Directory::Node* target = dir.get_node(
                    (target_id == 0) ? path[path.size()-2] : target_id
                );
                if (target) {
                    cout << "\n目标目录 (" << target->name << ") 现在包含:\n";
                    for (size_t i = 0; i < target->children.size(); ++i) {
                        auto child = target->children[i];
                        cout << " [" << (i+1) << "] " << child->name 
                             << (child->is_dir ? "/" : "") << endl;
                    }
                }
            } else {
                cout << "部分文件移动失败！\n当前目录状态:\n";
                print_current_dir(dir);
            }
            
            success = all_success;
        }

        if (success) {
            hand.erase(it);
            used_counts[card]++;
            if (sudo_used) used_counts[*it]++;
        }
        return success;
    }

    void discard_excess_cards() {
        int max_keep = file_count * 2;
        while (hand.size() > max(5, max_keep)) {
            cout << "弃牌: " << hand.back() << endl;
            hand.pop_back();
        }
    }

    bool is_eliminated() const { return file_count <= 0; }

    void show_status(Directory& dir) {
        cout << "玩家" << id << " 路径: ";
        for (int d : path) {
            Directory::Node* node = dir.get_node(d);
            if (node) cout << "/" << node->name;
        }
        cout << "\n文件数: " << file_count << endl;
    }
};

#endif