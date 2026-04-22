#include <iostream>
#include <fstream>
#include <string>
#include <unordered_map>
#include <vector>
#include <cstdint>
#include <algorithm>
#include <cstdio>
#include <cstring>

using namespace std;

const char* DATA_FILE = "data.bin";
const char* INDEX_FILE = "index.bin";
const uint64_t NULL_OFFSET = 0;

struct Node {
    int value;
    uint64_t next;
};

bool file_exists(const char* filename) {
    FILE* f = fopen(filename, "rb");
    if (f) {
        fclose(f);
        return true;
    }
    return false;
}

void clear_files() {
    remove(DATA_FILE);
    remove(INDEX_FILE);
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
        FILE* index_in = fopen(INDEX_FILE, "rb");
        if (index_in) {
            char key[65];
            uint64_t offset;
            while (fread(key, 64, 1, index_in) == 1) {
                if (fread(&offset, sizeof(offset), 1, index_in) == 1) {
                    // Trim null characters from key
                    size_t len = strnlen(key, 64);
                    string key_str(key, len);
                    index_map[key_str] = offset;
                }
            }
            fclose(index_in);
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
                FILE* data_out = fopen(DATA_FILE, "ab");
                if (data_out) {
                    Node new_node = {value, NULL_OFFSET};
                    fseek(data_out, 0, SEEK_END);
                    uint64_t new_offset = ftell(data_out);
                    fwrite(&new_node, sizeof(Node), 1, data_out);
                    fclose(data_out);
                    index_map[index] = new_offset;
                }
            } else {
                // Insert into existing list, maintaining sorted order
                // Traverse the list to find insertion point
                uint64_t current = index_map[index];
                uint64_t prev_offset = NULL_OFFSET;
                bool found = false;

                FILE* data_in = fopen(DATA_FILE, "rb");
                if (data_in) {
                    while (current != NULL_OFFSET) {
                        fseek(data_in, current, SEEK_SET);
                        Node node;
                        if (fread(&node, sizeof(Node), 1, data_in) != 1) {
                            break;
                        }

                        if (node.value == value) {
                            found = true;
                            break;
                        }

                        if (node.value > value) {
                            break;
                        }

                        prev_offset = current;
                        current = node.next;
                    }
                    fclose(data_in);
                }

                if (!found) {
                    // Append new node
                    FILE* data_out = fopen(DATA_FILE, "ab");
                    if (data_out) {
                        Node new_node = {value, current};
                        fseek(data_out, 0, SEEK_END);
                        uint64_t new_offset = ftell(data_out);
                        fwrite(&new_node, sizeof(Node), 1, data_out);
                        fclose(data_out);

                        // Update previous node's next pointer
                        if (prev_offset == NULL_OFFSET) {
                            // Insert at head
                            index_map[index] = new_offset;
                        } else {
                            // Update previous node
                            FILE* data_rw = fopen(DATA_FILE, "r+b");
                            if (data_rw) {
                                fseek(data_rw, prev_offset, SEEK_SET);
                                Node prev_node;
                                if (fread(&prev_node, sizeof(Node), 1, data_rw) == 1) {
                                    prev_node.next = new_offset;
                                    fseek(data_rw, prev_offset, SEEK_SET);
                                    fwrite(&prev_node, sizeof(Node), 1, data_rw);
                                }
                                fclose(data_rw);
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

            // Traverse the list to find the node to delete
            uint64_t current = index_map[index];
            uint64_t prev_offset = NULL_OFFSET;
            bool found = false;

            FILE* data_in = fopen(DATA_FILE, "rb");
            if (data_in) {
                while (current != NULL_OFFSET) {
                    fseek(data_in, current, SEEK_SET);
                    Node node;
                    if (fread(&node, sizeof(Node), 1, data_in) != 1) {
                        break;
                    }

                    if (node.value == value) {
                        found = true;
                        if (prev_offset == NULL_OFFSET) {
                            // Delete head
                            index_map[index] = node.next;
                            if (node.next == NULL_OFFSET) {
                                index_map.erase(index);
                            }
                        } else {
                            // Update previous node's next pointer
                            FILE* data_rw = fopen(DATA_FILE, "r+b");
                            if (data_rw) {
                                fseek(data_rw, prev_offset, SEEK_SET);
                                Node prev_node;
                                if (fread(&prev_node, sizeof(Node), 1, data_rw) == 1) {
                                    prev_node.next = node.next;
                                    fseek(data_rw, prev_offset, SEEK_SET);
                                    fwrite(&prev_node, sizeof(Node), 1, data_rw);
                                }
                                fclose(data_rw);
                            }
                        }
                        break;
                    }

                    prev_offset = current;
                    current = node.next;
                }
                fclose(data_in);
            }

        } else if (cmd == "find") {
            string index;
            cin >> index;

            if (index_map.find(index) == index_map.end()) {
                cout << "null\n";
                continue;
            }

            FILE* data_file = fopen(DATA_FILE, "rb");
            if (!data_file) {
                cout << "null\n";
                continue;
            }

            uint64_t current = index_map[index];
            vector<int> values;

            while (current != NULL_OFFSET) {
                fseek(data_file, current, SEEK_SET);
                Node node;
                if (fread(&node, sizeof(Node), 1, data_file) != 1) {
                    break;
                }
                values.push_back(node.value);
                current = node.next;
            }

            fclose(data_file);

            for (size_t j = 0; j < values.size(); j++) {
                if (j > 0) cout << " ";
                cout << values[j];
            }
            cout << "\n";
        }
    }

    // Save index file
    FILE* index_out = fopen(INDEX_FILE, "wb");
    if (index_out) {
        for (const auto& p : index_map) {
            char key[65];
            memset(key, 0, 65);
            strncpy(key, p.first.c_str(), 64);
            fwrite(key, 64, 1, index_out);
            fwrite(&p.second, sizeof(p.second), 1, index_out);
        }
        fclose(index_out);
    }

    return 0;
}
