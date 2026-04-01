#ifndef BPT_MEMORYRIVER_HPP
#define BPT_MEMORYRIVER_HPP

#include <fstream>
#include <string>

using std::string;
using std::fstream;
using std::ifstream;
using std::ofstream;

// A simple file-backed storage with optional space reuse.
// Layout: first info_len ints are reserved header.
// We additionally use header slot info_len (1-based) to store the head of a free list
// measured in bytes offset from file start. 0 means empty free list.
// Each freed block is stored as: [int next_free][raw bytes of T]
// When writing, we try to reuse the head free block; otherwise append to end.
// The returned index is the byte offset where the object bytes begin.

template<class T, int info_len = 2>
class MemoryRiver {
private:
    fstream file;
    string file_name;
    int sizeofT = sizeof(T);

    // We reserve info_len ints for user (1..info_len) and add 1 internal int
    // at offset info_len (0-based) to store the free list head.
    static constexpr int USER_HEADER_BYTES = info_len * static_cast<int>(sizeof(int));
    static constexpr int INTERNAL_HEADER_BYTES = (info_len + 1) * static_cast<int>(sizeof(int));
    static constexpr long long FREE_HEAD_OFFSET = USER_HEADER_BYTES; // byte offset for internal free head

    long long file_size()
    {
        file.seekg(0, std::ios::end);
        return static_cast<long long>(file.tellg());
    }

    // Helper: read an int at absolute byte offset
    void read_int_at(int &val, long long offset)
    {
        file.seekg(offset, std::ios::beg);
        file.read(reinterpret_cast<char *>(&val), sizeof(int));
    }

    // Helper: write an int at absolute byte offset
    void write_int_at(int val, long long offset)
    {
        file.seekp(offset, std::ios::beg);
        file.write(reinterpret_cast<char *>(&val), sizeof(int));
    }

public:
    MemoryRiver() = default;

    explicit MemoryRiver(const string &file_name) : file_name(file_name) {}

    void initialise(string FN = "") {
        if (FN != "") file_name = FN;
        file.open(file_name, std::ios::out | std::ios::binary | std::ios::trunc);
        int tmp = 0;
        // Initialize user header (info_len) + 1 internal int (free list head)
        for (int i = 0; i < info_len + 1; ++i)
            file.write(reinterpret_cast<char *>(&tmp), sizeof(int));
        file.close();
    }

    // Read the n-th int (1-based) from header into tmp
    void get_info(int &tmp, int n) {
        if (n > info_len || n <= 0) return;
        file.open(file_name, std::ios::in | std::ios::binary);
        if (!file) return;
        long long offset = static_cast<long long>((n - 1)) * sizeof(int);
        read_int_at(tmp, offset);
        file.close();
    }

    // Write tmp into the n-th int (1-based) in header
    void write_info(int tmp, int n) {
        if (n > info_len || n <= 0) return;
        file.open(file_name, std::ios::in | std::ios::out | std::ios::binary);
        if (!file) return;
        long long offset = static_cast<long long>((n - 1)) * sizeof(int);
        write_int_at(tmp, offset);
        file.close();
    }

    // Write object t and return its index (byte offset to the object bytes)
    int write(T &t) {
        // Try reuse from free list
        file.open(file_name, std::ios::in | std::ios::out | std::ios::binary);
        if (!file) {
            // if file missing, initialize header
            file.clear();
            initialise(file_name);
            file.open(file_name, std::ios::in | std::ios::out | std::ios::binary);
        }

        int free_head = 0;
        read_int_at(free_head, FREE_HEAD_OFFSET);

        int index = 0;
        if (free_head != 0) {
            // Reuse block at free_head (points to block start). Layout: [int next][T bytes]
            int next_free = 0;
            read_int_at(next_free, free_head);
            // Update free head
            write_int_at(next_free, FREE_HEAD_OFFSET);
            // Write object at block_start + sizeof(int)
            index = free_head + static_cast<int>(sizeof(int));
            file.seekp(index, std::ios::beg);
            file.write(reinterpret_cast<char *>(&t), sizeof(T));
        } else {
            // Append new block: [int reserved=0][T]
            long long sz = file_size();
            if (sz < INTERNAL_HEADER_BYTES) sz = INTERNAL_HEADER_BYTES; // ensure header exists
            // block start is sz
            int reserved = 0;
            file.seekp(sz, std::ios::beg);
            file.write(reinterpret_cast<char *>(&reserved), sizeof(int));
            index = static_cast<int>(sz + sizeof(int));
            file.write(reinterpret_cast<char *>(&t), sizeof(T));
        }

        file.close();
        return index;
    }

    // Update object at index
    void update(T &t, const int index) {
        file.open(file_name, std::ios::in | std::ios::out | std::ios::binary);
        if (!file) return;
        file.seekp(static_cast<long long>(index), std::ios::beg);
        file.write(reinterpret_cast<char *>(&t), sizeof(T));
        file.close();
    }

    // Read object at index
    void read(T &t, const int index) {
        file.open(file_name, std::ios::in | std::ios::binary);
        if (!file) return;
        file.seekg(static_cast<long long>(index), std::ios::beg);
        file.read(reinterpret_cast<char *>(&t), sizeof(T));
        file.close();
    }

    // Delete object at index: push a free block with next pointer = previous free head
    void Delete(int index) {
        file.open(file_name, std::ios::in | std::ios::out | std::ios::binary);
        if (!file) return;
        // Load current free head
        int free_head = 0;
        read_int_at(free_head, FREE_HEAD_OFFSET);

        // Compute block start and link into free list
        int block_start = index - static_cast<int>(sizeof(int));
        write_int_at(free_head, static_cast<long long>(block_start));
        write_int_at(block_start, FREE_HEAD_OFFSET);

        file.close();
    }
};


#endif //BPT_MEMORYRIVER_HPP
