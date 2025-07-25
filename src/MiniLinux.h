//本文件几乎完全由 DS 编写（（（ 
#ifndef MINILINUX_H
#define MINILINUX_H

#include <bits/stdc++.h>
using namespace std;

class Directory {
public:
    struct Node {
        int id;
        bool is_dir;
        int owner;
        string name;
        vector<Node*> children;
        
        Node(int i, bool d, int o, const string& n) 
            : id(i), is_dir(d), owner(o), name(n) {}
    };

private:
    Node* root;
    int next_id = 1;
    
    Node* find_node(int id, Node* current) {
        if (!current) return nullptr;
        if (current->id == id) return current;
        for (Node* child : current->children) {
            Node* found = find_node(id, child);
            if (found) return found;
        }
        return nullptr;
    }

public:
    Directory() {
        root = new Node(next_id++, true, -1, "Home");
    }

    Node* get_node(int id) { return find_node(id, root); }

    int mkdir(int parent_id, const string& name) {
        Node* parent = get_node(parent_id);
        if (!parent || !parent->is_dir) return -1;
        
        Node* new_dir = new Node(next_id++, true, -1, name);
        parent->children.push_back(new_dir);
        return new_dir->id;
    }

    bool echo(int parent_id, int owner, const string& name) {
        Node* parent = get_node(parent_id);
        if (!parent || !parent->is_dir) return false;
        
        parent->children.push_back(new Node(next_id++, false, owner, name));
        return true;
    }

    bool rm(int parent_id, int file_idx, int requester) {
        Node* parent = get_node(parent_id);
        if (!parent || file_idx < 0 || file_idx >= parent->children.size()) 
            return false;
            
        Node* target = parent->children[file_idx];
        if (!target->is_dir && target->owner != requester) 
            return false;
            
        parent->children.erase(parent->children.begin() + file_idx);
        delete target;
        return true;
    }

    bool mv(int src_id, int file_idx, int dest_id, int requester) {
        Node* src = get_node(src_id);
        Node* dest = get_node(dest_id);
        if (!src || !dest || !dest->is_dir || file_idx < 0 || file_idx >= src->children.size())
            return false;
            
        Node* file = src->children[file_idx];
        if (!file->is_dir && file->owner != requester)
            return false;
            
        src->children.erase(src->children.begin() + file_idx);
        dest->children.push_back(file);
        return true;
    }

    void print() {
        cout << "\n完整目录结构:\nHome\n";
        function<void(Node*, int)> dfs = [&](Node* node, int depth) {
            for (size_t i = 0; i < node->children.size(); ++i) {
                Node* child = node->children[i];
                for (int j = 0; j < depth; ++j) cout << "|   ";
                cout << (i == node->children.size()-1 ? "└── " : "├── ")
                     << child->name << (child->is_dir ? "/" : "") << endl;
                if (child->is_dir) dfs(child, depth + 1);
            }
        };
        dfs(root, 0);
    }
};

#endif