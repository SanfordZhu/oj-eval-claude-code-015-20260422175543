#include <iostream>
#include <fstream>
#include <string>
#include <unordered_map>
#include <vector>
#include <cstdint>
#include <algorithm>

using namespace std;

const string DATA_FILE = "data.bin";
const string INDEX_FILE = "index.bin";
const uint64_t NULL_OFFSET = 0;

struct Node {
    int value;
    uint64_t next;
};

bool file_exists(const string& filename) {
    ifstream f(filename);
    return f.good();
}

void clear_files() {
    remove(DATA_FILE.c_str());
    remove(INDEX_FILE.c_str());
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    // Check file consistency - both files must exist together
    bool data_exists = file_exists(DATA_FILE);
    bool index_exists = file_exists(INDEX_FILE);

    if (data_exists != index_exists) {
        // Files are inconsistent, clear both
        clear_files();
        data_exists = false;
        index_exists = false;
    }

    unordered_map<string, uint64_t> index_map;

    // Load index file if exists
    if (index_exists) {
        ifstream index_in(INDEX_FILE, ios::binary);
        if (index_in.is_open()) {
            string key;
            key.resize(64);
            uint64_t offset;
            while (index_in.read(&key[0], 64)) {
                if (index_in.read(reinterpret_cast<char*>(&offset), sizeof(offset))) {
                    // Trim null characters from key
                    size_t len = key.find('\0');
                    if (len != string::npos) {
                        key = key.substr(0, len);
                    }
                    index_map[key] = offset;
                }
            }
            index_in.close();
        }
    }

    int n;
    cin >> n;

    for (int i = 0; i < n; i++) {
        string cmd;
        cin >> cmd;

        if (cmd == "insert") {
            string index;
            int value;
            cin >> index >> value;

            if (index_map.find(index) == index_map.end()) {
                // Create new list - append to data file
                ofstream data_out(DATA_FILE, ios::binary | ios::app);
                if (data_out.is_open()) {
                    Node new_node = {value, NULL_OFFSET};
                    uint64_t new_offset = data_out.tellp();
                    data_out.write(reinterpret_cast<char*>(&new_node), sizeof(Node));
                    data_out.close();
                    index_map[index] = new_offset;
                }
            } else {
                // Insert into existing list, maintaining sorted order
                // First, read all nodes for this index
                vector<pair<uint64_t, int>> nodes; // (offset, value)
                uint64_t current = index_map[index];

                ifstream data_in(DATA_FILE, ios::binary);
                if (data_in.is_open()) {
                    while (current != NULL_OFFSET) {
                        data_in.seekg(current);
                        Node node;
                        if (data_in.read(reinterpret_cast<char*>(&node), sizeof(Node))) {
                            nodes.push_back({current, node.value});
                            current = node.next;
                        } else {
                            break;
                        }
                    }
                    data_in.close();
                }

                // Check if value already exists
                bool found = false;
                for (const auto& p : nodes) {
                    if (p.second == value) {
                        found = true;
                        break;
                    }
                }

                if (!found) {
                    // Find insertion position
                    size_t insert_pos = 0;
                    for (size_t j = 0; j < nodes.size(); j++) {
                        if (nodes[j].second > value) {
                            insert_pos = j;
                            break;
                        }
                        insert_pos = j + 1;
                    }

                    // Append new node
                    ofstream data_out(DATA_FILE, ios::binary | ios::app);
                    if (data_out.is_open()) {
                        Node new_node = {value, NULL_OFFSET};
                        uint64_t new_offset = data_out.tellp();
                        data_out.write(reinterpret_cast<char*>(&new_node), sizeof(Node));
                        data_out.close();

                        // Update linked list
                        if (insert_pos == 0) {
                            // Insert at head
                            new_node.next = index_map[index];
                            fstream data_rw(DATA_FILE, ios::binary | ios::in | ios::out);
                            if (data_rw.is_open()) {
                                data_rw.seekp(new_offset);
                                data_rw.write(reinterpret_cast<char*>(&new_node), sizeof(Node));
                                data_rw.close();
                            }
                            index_map[index] = new_offset;
                        } else if (insert_pos == nodes.size()) {
                            // Insert at end
                            uint64_t last_offset = nodes.back().first;
                            fstream data_rw(DATA_FILE, ios::binary | ios::in | ios::out);
                            if (data_rw.is_open()) {
                                data_rw.seekg(last_offset);
                                Node last_node;
                                if (data_rw.read(reinterpret_cast<char*>(&last_node), sizeof(Node))) {
                                    last_node.next = new_offset;
                                    data_rw.seekp(last_offset);
                                    data_rw.write(reinterpret_cast<char*>(&last_node), sizeof(Node));
                                }
                                data_rw.close();
                            }
                        } else {
                            // Insert in middle
                            uint64_t prev_offset = nodes[insert_pos - 1].first;
                            new_node.next = nodes[insert_pos].first;
                            fstream data_rw(DATA_FILE, ios::binary | ios::in | ios::out);
                            if (data_rw.is_open()) {
                                data_rw.seekp(new_offset);
                                data_rw.write(reinterpret_cast<char*>(&new_node), sizeof(Node));

                                data_rw.seekg(prev_offset);
                                Node prev_node;
                                if (data_rw.read(reinterpret_cast<char*>(&prev_node), sizeof(Node))) {
                                    prev_node.next = new_offset;
                                    data_rw.seekp(prev_offset);
                                    data_rw.write(reinterpret_cast<char*>(&prev_node), sizeof(Node));
                                }
                                data_rw.close();
                            }
                        }
                    }
                }
            }

        } else if (cmd == "delete") {
            string index;
            int value;
            cin >> index >> value;

            if (index_map.find(index) == index_map.end()) {
                continue;
            }

            // Read all nodes for this index
            vector<pair<uint64_t, int>> nodes; // (offset, value)
            uint64_t current = index_map[index];

            ifstream data_in(DATA_FILE, ios::binary);
            if (data_in.is_open()) {
                while (current != NULL_OFFSET) {
                    data_in.seekg(current);
                    Node node;
                    if (data_in.read(reinterpret_cast<char*>(&node), sizeof(Node))) {
                        nodes.push_back({current, node.value});
                        current = node.next;
                    } else {
                        break;
                    }
                }
                data_in.close();
            }

            // Find and delete the node
            for (size_t j = 0; j < nodes.size(); j++) {
                if (nodes[j].second == value) {
                    if (j == 0) {
                        // Delete head
                        if (nodes.size() == 1) {
                            index_map.erase(index);
                        } else {
                            index_map[index] = nodes[j + 1].first;
                        }
                    } else {
                        // Delete middle/end
                        uint64_t prev_offset = nodes[j - 1].first;
                        uint64_t next_offset = (j + 1 < nodes.size()) ? nodes[j + 1].first : NULL_OFFSET;

                        fstream data_rw(DATA_FILE, ios::binary | ios::in | ios::out);
                        if (data_rw.is_open()) {
                            data_rw.seekg(prev_offset);
                            Node prev_node;
                            if (data_rw.read(reinterpret_cast<char*>(&prev_node), sizeof(Node))) {
                                prev_node.next = next_offset;
                                data_rw.seekp(prev_offset);
                                data_rw.write(reinterpret_cast<char*>(&prev_node), sizeof(Node));
                            }
                            data_rw.close();
                        }
                    }
                    break;
                }
            }

        } else if (cmd == "find") {
            string index;
            cin >> index;

            if (index_map.find(index) == index_map.end()) {
                cout << "null\n";
                continue;
            }

            ifstream data_file(DATA_FILE, ios::binary);
            if (!data_file.is_open()) {
                cout << "null\n";
                continue;
            }

            uint64_t current = index_map[index];
            vector<int> values;

            while (current != NULL_OFFSET) {
                data_file.seekg(current);
                Node node;
                if (data_file.read(reinterpret_cast<char*>(&node), sizeof(Node))) {
                    values.push_back(node.value);
                    current = node.next;
                } else {
                    break;
                }
            }

            data_file.close();

            for (size_t j = 0; j < values.size(); j++) {
                if (j > 0) cout << " ";
                cout << values[j];
            }
            cout << "\n";
        }
    }

    // Save index file
    ofstream index_out(INDEX_FILE, ios::binary);
    for (const auto& p : index_map) {
        string key = p.first;
        key.resize(64, '\0');
        index_out.write(key.c_str(), 64);
        index_out.write(reinterpret_cast<const char*>(&p.second), sizeof(p.second));
    }
    index_out.close();

    return 0;
}
