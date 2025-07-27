#ifndef PLAYERS_H
#define PLAYERS_H

#include "MiniLinux.h"
#include <windows.h>
#include <bits/stdc++.h>
#include <fstream>

class Player {
private:
    int id;
    vector<string> cards;
    vector<int> path;
    int file_cnt = 3;
    map<string, int>* card_lim;
    
    static vector<string> load_card_pool() {
        vector<string> pool;
        ifstream fin("data/cards.txt");
        if(!fin) {
            vector<pair<string, int>> def = {
                {"cd", 40}, 
                {"mkdir", 20},
                {"mv", 20},
                {"rm", 16},
                {"echo", 8},
                {"sudo", 23}
            };
            for(const auto& i : def) {
                for(int j = 0; j < i.second; j++) {
                    pool.push_back(i.first);
                }
            }
            return pool;
        }
        string name;
        int cnt;
        while(fin >> name >> cnt) {
            for(int i = 0; i < cnt; i++) {
                pool.push_back(name);
            }
        }
        return pool;
    }
    
    void print_dir(Directory& dir) {
        Directory::Node* now = dir.get_node(path.back());
        if(!now) return;
        cout << "\n当前目录 (" << now->name << ") 内容:\n";
        for(size_t i = 0; i < now->children.size(); i++) {
            auto chd = now->children[i];
            cout << " [" << (i+1) << "] " << chd->name 
                 << (chd->is_dir ? "/" : "")
                 << (!chd->is_dir ? " (玩家" + to_string(chd->owner) + ")" : "")
                 << endl;
        }
    }
    
public:
    Player(int i, Directory& dir) : id(i) {
        card_lim = new map<string, int>();
        (*card_lim)["cd"] = 3;
        (*card_lim)["sudo"] = -1;
        (*card_lim)["mkdir"] = 1;
        (*card_lim)["mv"] = 1;
        (*card_lim)["rm"] = 1;
        (*card_lim)["echo"] = 1;
        
        int tmp = dir.mkdir(1, "player" + to_string(id));
        path.push_back(1);
        path.push_back(tmp);
        for(int j = 1; j <= 3; j++) {
            dir.echo(tmp, id, "file" + to_string(j));
        }
    }
    
    ~Player() {
        delete card_lim;
    }
    
    Player(Player&&) = default;
    Player& operator=(Player&&) = default;
    Player(const Player&) = delete;
    Player& operator=(const Player&) = delete;
    
    int get_id() const { return id; }
    int get_dir() const { return path.back(); }
    
    void draw_cards(int n) {
        static vector<string> pool = [](){
            auto p = load_card_pool();
            random_shuffle(p.begin(), p.end());
            return p;
        }();
        static size_t idx = 0;
        
        for(int i = 0; i < n && idx < pool.size(); i++) {
            cards.push_back(pool[idx++]);
        }
    }
    
    void show_cards() {
        wstring_convert<codecvt_utf8_utf16<wchar_t>> cv;
        wstring msg = L"玩家 " + to_wstring(id) + L" 的手牌:\n";
        for(const auto& c : cards) {
            msg += L" " + cv.from_bytes(c);
        }
        MessageBoxW(NULL, msg.c_str(), L"手牌", MB_OK);
    }
    
    bool play_cards(const string& card, Directory& dir, map<string, int>& used) {
        auto it = find(cards.begin(), cards.end(), card);
        if(it == cards.end()) {
            cout << "没有这张牌！\n";
            return false;
        }
        if(card_lim->at(card) != -1 && used[card] >= card_lim->at(card)) {
            cout << card << " 本回合使用已达上限(" << card_lim->at(card) << "次)\n";
            return false;
        }
        bool sudo = (card == "sudo");
        string now_card = card;
        if(sudo) {
            cout << "选择要强化的指令: ";
            string tmp;
            cin >> tmp;
            it = find(cards.begin(), cards.end(), tmp);
            if(it == cards.end()) {
                cout << "没有找到目标卡牌！\n";
                return false;
            }
            now_card = *it;
        }
        bool ok = false;
        int now_dir = path.back();
        print_dir(dir);
        if(now_card == "cd") {
            cout << "输入要进入的子目录编号(0返回上级): ";
            int idx;
            cin >> idx;
            if(idx == 0 && path.size() > 1) {
                path.pop_back();
                ok = true;
            }
            else if(idx > 0) {
                Directory::Node* now = dir.get_node(now_dir);
                if(now && idx <= now->children.size()) {
                    auto chd = now->children[idx-1];
                    if(chd->is_dir) {
                        path.push_back(chd->id);
                        ok = true;
                    }
                }
            }
        } 
        else if(now_card == "mkdir") {
            cout << "输入新目录名: ";
            string name;
            cin >> name;
            ok = dir.mkdir(now_dir, name);
        }
        else if(now_card == "echo") {
            cout << "输入新文件名: ";
            string name;
            cin >> name;
            ok = dir.echo(now_dir, id, name);
            if(ok) file_cnt++;
        }
        else if(now_card == "rm") {
            cout << "输入要删除的文件编号: ";
            int idx;
            cin >> idx;
            Directory::Node* now = dir.get_node(now_dir);
            if(now && idx > 0 && idx <= now->children.size()) {
                auto chd = now->children[idx-1];
                if(!chd->is_dir && (sudo || chd->owner == id)) {
                    ok = dir.rm(now_dir, idx-1, sudo ? -1 : id);
                    if(ok) file_cnt--;
                } else {
                    cout << "无法删除该文件！\n";
                }
            }
        }
        else if(now_card == "mv") {
            Directory::Node* now = dir.get_node(now_dir);
            if(!now) return false;
            
            cout << "输入要移动的文件编号(多个编号用空格分隔): ";
            string ids;
            cin.ignore();
            getline(cin, ids);
            
            istringstream iss(ids);
            vector<int> idxs;
            int idx;
            while (iss >> idx) {
                if(idx > 0 && idx <= now->children.size()) {
                    auto chd = now->children[idx-1];
                    if(!chd->is_dir && (sudo || chd->owner == id)) {
                        idxs.push_back(idx - 1);
                    }
                }
            }
            
            if(idxs.empty()) {
                cout << "没有有效的文件可移动！\n";
                return false;
            }
            
            cout << "输入目标目录ID: ";
            int tar_id;
            cin >> tar_id;
            
            Directory::Node* tar = dir.get_node(tar_id);
            if(!tar || !tar->is_dir) {
                cout << "无效的目标目录！\n";
                return false;
            }
            
            bool all_ok = true;
            for(int i : idxs) {
                if(i < 0 || i >= now->children.size()) {
                    all_ok = false;
                    break;
                }
            }
            
            if(all_ok) {
                sort(idxs.rbegin(), idxs.rend());
                for(int i : idxs) {
                    if(!dir.mv(now_dir, i, tar_id, sudo ? -1 : id)) {
                        all_ok = false;
                        break;
                    }
                }
            }
            
            if(all_ok) {
                cout << "\n操作成功！更新后的目录状态:\n";
                print_dir(dir);
                cout << "\n目标目录 (" << tar->name << ") 现在包含:\n";
                for(size_t i = 0; i < tar->children.size(); i++) {
                    auto chd = tar->children[i];
                    cout << " [" << (i+1) << "] " << chd->name 
                         << (chd->is_dir ? "/" : "") << endl;
                }
                ok = true;
            } else {
                cout << "文件移动失败！\n当前目录状态:\n";
                print_dir(dir);
            }
        }
        
        if(ok) {
            cards.erase(find(cards.begin(), cards.end(), card));
            if(sudo) {
                cards.erase(it);
            }
            used[card]++;
            if(sudo) used[now_card]++;
        }
        return ok;
    }
    
    void discard() {
        const int max_keep = max(5, file_cnt * 3);
        int to_discard = cards.size() - max_keep;
        if(to_discard <= 0) return;
        cout << "\n=== 强制弃牌阶段 ===\n";
        cout << "手牌数: " << cards.size() << " (需保留≤" << max_keep << ")\n";
        cout << "必须弃掉 " << to_discard << " 张牌\n";
        while(to_discard > 0 && !cards.empty()) {
            show_cards();
            cout << "选择要弃掉的牌: ";
            string c;
            cin >> c;
            auto it = find(cards.begin(), cards.end(), c);
            if(it != cards.end()) {
                cards.erase(it);
                to_discard--;
                cout << "已弃掉: " << c << " (剩余需弃: " << to_discard << ")\n";
            }
            else {
                cout << "无效选择！\n";
            }
        }
    }
    
    bool is_eliminated() const { return file_cnt <= 0; }
    
    void show_status(Directory& dir) {
        cout << "玩家" << id << " 路径: ";
        for(int d : path) {
            Directory::Node* node = dir.get_node(d);
            if(node) cout << "/" << node->name;
        }
        cout << "\n文件数: " << file_cnt << endl;
    }
};

#endif