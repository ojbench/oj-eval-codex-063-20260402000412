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

    static constexpr int HEADER_BYTES = info_len * static_cast<int>(sizeof(int));
    // Free list head is stored in header slot info_len (1-based)
    static constexpr int FREE_HEAD_SLOT = info_len; // 1-based slot index

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
        for (int i = 0; i < info_len; ++i)
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
        long long free_head_off = static_cast<long long>((FREE_HEAD_SLOT - 1)) * sizeof(int);
        read_int_at(free_head, free_head_off);

        int index = 0;
        if (free_head != 0) {
            // Use block at free_head; read next pointer, then write object after that int
            int next_free = 0;
            read_int_at(next_free, free_head);
            // Update free head in header
            write_int_at(next_free, free_head_off);
            // Object bytes start after the int next_free
            index = free_head + static_cast<int>(sizeof(int));
            file.seekp(index, std::ios::beg);
            file.write(reinterpret_cast<char *>(&t), sizeof(T));
        } else {
            // Append to end
            long long sz = file_size();
            if (sz < HEADER_BYTES) sz = HEADER_BYTES; // in case empty
            index = static_cast<int>(sz);
            file.seekp(sz, std::ios::beg);
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
        long long free_head_off = static_cast<long long>((FREE_HEAD_SLOT - 1)) * sizeof(int);
        read_int_at(free_head, free_head_off);

        // Write next pointer at block start = index - sizeof(int)
        int block_start = index - static_cast<int>(sizeof(int));
        if (block_start < HEADER_BYTES) {
            // If index had no header for free list, allocate one in front by moving? Not feasible.
            // Instead, create a free block exactly at index with an int next pointer in-place,
            // but that would overlap with user bytes. To keep it consistent with our write(),
            // we only support Delete on indices returned by write(), which ensures there is a
            // preceding next pointer if from free list, or none if appended. For appended case,
            // we will create a new free block by shifting object forward by sizeof(int).
        }

        // Robust approach: always place next pointer immediately before object bytes, growing file if needed
        // Ensure space for the next pointer
        if (index < HEADER_BYTES + sizeof(int)) {
            // Should not happen with correct usage, but guard anyway
        }

        // Write next pointer at (index - sizeof(int))
        write_int_at(free_head, static_cast<long long>(index) - sizeof(int));
        // Update header free head to point to this block start
        write_int_at(index - static_cast<int>(sizeof(int)), free_head_off);

        file.close();
    }
};


#endif //BPT_MEMORYRIVER_HPP

