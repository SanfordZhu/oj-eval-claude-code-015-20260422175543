#include <iostream>
#include <fstream>
#include <string>
#include <unordered_map>
#include <vector>
#include <cstdint>
#include <algorithm>
#include <filesystem>

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

            fstream data_file(DATA_FILE, ios::binary | ios::in | ios::out);
            if (!data_file.is_open()) {
                data_file.open(DATA_FILE, ios::binary | ios::out);
                data_file.close();
                data_file.open(DATA_FILE, ios::binary | ios::in | ios::out);
            }

            if (index_map.find(index) == index_map.end()) {
                // Create new list
                Node new_node = {value, NULL_OFFSET};
                data_file.seekp(0, ios::end);
                uint64_t new_offset = data_file.tellp();
                data_file.write(reinterpret_cast<char*>(&new_node), sizeof(Node));
                index_map[index] = new_offset;
            } else {
                // Insert into existing list, maintaining sorted order
                uint64_t current = index_map[index];
                uint64_t prev = NULL_OFFSET;
                uint64_t insert_pos = NULL_OFFSET;
                uint64_t prev_offset = NULL_OFFSET;
                bool found = false;

                while (current != NULL_OFFSET) {
                    Node node;
                    data_file.seekg(current);
                    data_file.read(reinterpret_cast<char*>(&node), sizeof(Node));

                    if (node.value == value) {
                        found = true;
                        break;
                    }

                    if (node.value > value) {
                        insert_pos = current;
                        prev_offset = prev;
                        break;
                    }

                    prev = current;
                    current = node.next;
                }

                if (!found) {
                    Node new_node = {value, insert_pos};
                    data_file.seekp(0, ios::end);
                    uint64_t new_offset = data_file.tellp();
                    data_file.write(reinterpret_cast<char*>(&new_node), sizeof(Node));

                    if (prev_offset == NULL_OFFSET && insert_pos == NULL_OFFSET) {
                        // Insert at end
                        if (prev != NULL_OFFSET) {
                            Node last_node;
                            data_file.seekg(prev);
                            data_file.read(reinterpret_cast<char*>(&last_node), sizeof(Node));
                            last_node.next = new_offset;
                            data_file.seekp(prev);
                            data_file.write(reinterpret_cast<char*>(&last_node), sizeof(Node));
                        }
                    } else if (prev_offset == NULL_OFFSET) {
                        // Insert at head
                        index_map[index] = new_offset;
                    } else {
                        // Insert in middle
                        Node prev_node;
                        data_file.seekg(prev_offset);
                        data_file.read(reinterpret_cast<char*>(&prev_node), sizeof(Node));
                        prev_node.next = new_offset;
                        data_file.seekp(prev_offset);
                        data_file.write(reinterpret_cast<char*>(&prev_node), sizeof(Node));
                    }
                }
            }
            data_file.close();

        } else if (cmd == "delete") {
            string index;
            int value;
            cin >> index >> value;

            if (index_map.find(index) == index_map.end()) {
                continue;
            }

            fstream data_file(DATA_FILE, ios::binary | ios::in | ios::out);
            uint64_t current = index_map[index];
            uint64_t prev = NULL_OFFSET;
            bool found = false;

            while (current != NULL_OFFSET) {
                Node node;
                data_file.seekg(current);
                data_file.read(reinterpret_cast<char*>(&node), sizeof(Node));

                if (node.value == value) {
                    found = true;
                    if (prev == NULL_OFFSET) {
                        // Delete head
                        index_map[index] = node.next;
                    } else {
                        // Delete middle/end
                        Node prev_node;
                        data_file.seekg(prev);
                        data_file.read(reinterpret_cast<char*>(&prev_node), sizeof(Node));
                        prev_node.next = node.next;
                        data_file.seekp(prev);
                        data_file.write(reinterpret_cast<char*>(&prev_node), sizeof(Node));
                    }
                    break;
                }

                prev = current;
                current = node.next;
            }

            if (index_map[index] == NULL_OFFSET) {
                index_map.erase(index);
            }

            data_file.close();

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
                Node node;
                data_file.seekg(current);
                data_file.read(reinterpret_cast<char*>(&node), sizeof(Node));
                values.push_back(node.value);
                current = node.next;
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
