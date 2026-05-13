#ifndef PLAYERS_H
#define PLAYERS_H

#include "MiniLinux.h"
#include "Network.h"
#include <bits/stdc++.h>
#include <fstream>

class Player {
private:
    int id;
    vector<string> cards;
    vector<int> path;
    map<string, int> card_lim;

    static vector<string> load_card_pool() {
        vector<string> pool;
        ifstream fin("data/card.txt");
        if(!fin) {
            vector<pair<string, int>> def = {
                {"cd", 25}, {"mkdir", 18}, {"mv", 16}, {"rm", 10},
                {"echo", 12}, {"sudo", 10}, {"ls", 12}, {"pwd", 6},
                {"chmod", 6}, {"event", 15}
            };
            for(const auto& i : def) {
                for(int j = 0; j < i.second; j++) pool.push_back(i.first);
            }
            return pool;
        }
        string name;
        int cnt;
        while(fin >> name >> cnt) {
            for(int i = 0; i < cnt; i++) pool.push_back(name);
        }
        return pool;
    }

    static bool parse_int(const string& text, int& value) {
        istringstream in(text);
        in >> value;
        return !in.fail();
    }

    static string prompt(Endpoint& io, const string& text) {
        io.write(text);
        string line;
        if(!io.read_line(line)) return "";
        return line;
    }

    bool remove_card_once(const string& card) {
        auto it = find(cards.begin(), cards.end(), card);
        if(it == cards.end()) return false;
        cards.erase(it);
        return true;
    }

    void print_dir(Directory& dir, Endpoint& io) const {
        Directory::Node* now = dir.get_node(path.back());
        if(!now) return;
        ostringstream out;
        out << "\n当前目录 (" << now->name << ") 内容:\n";
        for(size_t i = 0; i < now->children.size(); i++) {
            auto chd = now->children[i];
            out << " [" << (i + 1) << "] " << chd->name
                << (chd->is_dir ? "/" : "")
                << (!chd->is_dir ? " (玩家" + to_string(chd->owner) + ")" : "")
                << " [id=" << chd->id << "]\n";
        }
        io.write(out.str());
    }

public:
    Player(int i, Directory& dir) : id(i) {
        card_lim["cd"] = 3;
        card_lim["sudo"] = -1;
        card_lim["mkdir"] = 1;
        card_lim["mv"] = 1;
        card_lim["rm"] = 1;
        card_lim["echo"] = 1;
        card_lim["ls"] = 1;
        card_lim["pwd"] = 1;
        card_lim["chmod"] = 1;
        card_lim["event"] = 0;

        int tmp = dir.mkdir(1, "player" + to_string(id));
        path.push_back(1);
        path.push_back(tmp);
        for(int j = 1; j <= 5; j++) {
            dir.echo(tmp, id, "file" + to_string(j));
        }
    }

    Player(Player&&) = default;
    Player& operator=(Player&&) = default;
    Player(const Player&) = delete;
    Player& operator=(const Player&) = delete;

    int get_id() const { return id; }
    int get_dir() const { return path.back(); }

    string path_string(Directory& dir) const {
        ostringstream out;
        for(int d : path) {
            Directory::Node* node = dir.get_node(d);
            if(node) out << "/" << node->name;
        }
        return out.str();
    }

    void draw_cards(int n) {
        static vector<string> pool = [](){
            auto p = load_card_pool();
            static mt19937 rng(static_cast<unsigned>(time(nullptr)));
            shuffle(p.begin(), p.end(), rng);
            return p;
        }();
        static size_t idx = 0;

        for(int i = 0; i < n && idx < pool.size(); i++) {
            cards.push_back(pool[idx++]);
        }
    }

    void show_cards(Endpoint& io) const {
        ostringstream out;
        out << "玩家 " << id << " 的手牌:";
        for(const auto& c : cards) out << " " << c;
        out << "\n";
        io.write(out.str());
    }

    bool play_cards(const string& card, Directory& dir, map<string, int>& used,
                    Endpoint& io, const vector<unique_ptr<Player>>& players) {
        auto it = find(cards.begin(), cards.end(), card);
        if(it == cards.end()) {
            io.write("没有这张牌！\n");
            return false;
        }
        if(!card_lim.count(card)) {
            io.write("未知卡牌，暂不能使用。\n");
            return false;
        }
        if(card_lim[card] == 0) {
            io.write("这张牌不能在出牌阶段直接使用。\n");
            return false;
        }
        if(card_lim[card] != -1 && used[card] >= card_lim[card]) {
            io.write(card + " 本回合使用已达上限(" + to_string(card_lim[card]) + "次)\n");
            return false;
        }

        bool sudo = (card == "sudo");
        string now_card = card;
        if(sudo) {
            string tmp = prompt(io, "选择要强化的指令: ");
            it = find(cards.begin(), cards.end(), tmp);
            if(it == cards.end()) {
                io.write("没有找到目标卡牌！\n");
                return false;
            }
            if(tmp == "sudo" || !card_lim.count(tmp) || card_lim[tmp] == 0) {
                io.write("该卡牌不能被 sudo 强化。\n");
                return false;
            }
            now_card = tmp;
        }

        bool ok = false;
        int now_dir = path.back();
        print_dir(dir, io);
        if(now_card == "cd") {
            int idx;
            if(parse_int(prompt(io, "输入要进入的子目录编号(0返回上级): "), idx)) {
                if(idx == 0 && path.size() > 1) {
                    path.pop_back();
                    ok = true;
                }
                else if(idx > 0) {
                    Directory::Node* now = dir.get_node(now_dir);
                    if(now && idx <= static_cast<int>(now->children.size())) {
                        auto chd = now->children[idx - 1];
                        if(chd->is_dir) {
                            path.push_back(chd->id);
                            ok = true;
                        }
                    }
                }
            }
        }
        else if(now_card == "ls") {
            print_dir(dir, io);
            ok = true;
        }
        else if(now_card == "pwd") {
            int target;
            if(parse_int(prompt(io, "输入要查看路径的玩家编号: "), target)) {
                for(const auto& p : players) {
                    if(p->get_id() == target) {
                        io.write("玩家" + to_string(target) + " 路径: " + p->path_string(dir) + "\n");
                        ok = true;
                        break;
                    }
                }
            }
        }
        else if(now_card == "chmod") {
            io.write("chmod 规则已记录：当前程序版本暂不保存文件权限，视为展示/宣告后消耗卡牌。\n");
            ok = true;
        }
        else if(now_card == "mkdir") {
            string name = prompt(io, "输入新目录名: ");
            ok = !name.empty() && dir.mkdir(now_dir, name) != -1;
        }
        else if(now_card == "echo") {
            string name = prompt(io, "输入新文件名: ");
            ok = !name.empty() && dir.echo(now_dir, id, name);
        }
        else if(now_card == "rm") {
            int idx;
            if(parse_int(prompt(io, "输入要删除的文件编号: "), idx)) {
                Directory::Node* now = dir.get_node(now_dir);
                if(now && idx > 0 && idx <= static_cast<int>(now->children.size())) {
                    auto chd = now->children[idx - 1];
                    if(!chd->is_dir && (sudo || chd->owner == id)) {
                        ok = dir.rm(now_dir, idx - 1, sudo ? -1 : id);
                    } else {
                        io.write("无法删除该文件！\n");
                    }
                }
            }
        }
        else if(now_card == "mv") {
            Directory::Node* now = dir.get_node(now_dir);
            if(!now) return false;

            string ids = prompt(io, "输入要移动的文件编号(多个编号用空格分隔): ");
            istringstream iss(ids);
            vector<int> idxs;
            int idx;
            while(iss >> idx) {
                if(idx > 0 && idx <= static_cast<int>(now->children.size())) {
                    auto chd = now->children[idx - 1];
                    if(!chd->is_dir && (sudo || chd->owner == id)) idxs.push_back(idx - 1);
                }
            }

            if(idxs.empty()) {
                io.write("没有有效的文件可移动！\n");
                return false;
            }

            int tar_id;
            if(!parse_int(prompt(io, "输入目标目录ID: "), tar_id)) return false;

            Directory::Node* tar = dir.get_node(tar_id);
            if(!tar || !tar->is_dir) {
                io.write("无效的目标目录！\n");
                return false;
            }

            sort(idxs.rbegin(), idxs.rend());
            ok = true;
            for(int i : idxs) {
                if(!dir.mv(now_dir, i, tar_id, sudo ? -1 : id)) {
                    ok = false;
                    break;
                }
            }

            if(ok) {
                io.write("\n操作成功！更新后的目录状态:\n");
                print_dir(dir, io);
            } else {
                io.write("文件移动失败！\n");
            }
        }

        if(ok) {
            remove_card_once(card);
            if(sudo) remove_card_once(now_card);
            used[card]++;
            if(sudo) used[now_card]++;
        }
        return ok;
    }

    void discard(Directory& dir, Endpoint& io) {
        const int max_keep = max(5, dir.count_owner_files(id) * 2);
        int to_discard = static_cast<int>(cards.size()) - max_keep;
        if(to_discard <= 0) return;
        io.write("\n=== 强制弃牌阶段 ===\n");
        io.write("手牌数: " + to_string(cards.size()) + " (需保留≤" + to_string(max_keep) + ")\n");
        io.write("必须弃掉 " + to_string(to_discard) + " 张牌\n");
        while(to_discard > 0 && !cards.empty()) {
            show_cards(io);
            string c = prompt(io, "选择要弃掉的牌: ");
            auto it = find(cards.begin(), cards.end(), c);
            if(it != cards.end()) {
                cards.erase(it);
                to_discard--;
                io.write("已弃掉: " + c + " (剩余需弃: " + to_string(to_discard) + ")\n");
            }
            else {
                io.write("无效选择！\n");
            }
        }
    }

    bool is_eliminated(Directory& dir) const { return dir.count_owner_files(id) <= 0; }

    void show_status(Directory& dir, Endpoint& io) const {
        io.write("玩家" + to_string(id) + " 路径: " + path_string(dir) + "\n文件数: " +
                 to_string(dir.count_owner_files(id)) + "\n");
    }
};

#endif
