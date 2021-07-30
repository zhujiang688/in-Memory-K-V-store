#include <stdint.h>
#include <iostream>
#include <unistd.h>
#include <memory.h>
#include <stdlib.h>
#include <vector>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <fstream>
#include <assert.h>
#include <sys/stat.h>
#include <dirent.h>


#define TABLE_SIZE 16            // adjustable 一个桶的容量
#define HASH_SIZE 16             // adjustable 哈希函数取余的初始值
#define FILE_SIZE 1024UL * 1024  *  1024 * 4 // store的内存大小为4GB

using namespace std;

typedef struct metadata
{
    atomic<size_t> size;           // the size of whole hash table array 哈希槽的数量
    atomic<size_t> level;          // level of hash 当前使用的哈希函数等级
    atomic<size_t> next;         // the index of the next split hash table 分裂顶点的位置
    atomic<size_t> overflow_num; // amount of overflow hash tables 溢出表的数目
    atomic<size_t> total;        // the number of total elements 哈希表中存储的元素数量，包括溢出表
    uint64_t max_size;
    uint64_t max_overflow_size;
} metadata;

// data entry of hash table
typedef struct entry
{
    uint64_t key;
    uint32_t value;
} entry;

struct pm_table_without_loc
{
    entry kv_arr[TABLE_SIZE]; // data entry array of hash table 每一个桶中可以存放的元素数量
    uint64_t fill_num;        // amount of occupied slots in kv_arr 桶中被占用的数量
    pm_table_without_loc* next;     // the file address of overflow hash table 哈希溢出表的地址
};

// hash table
struct pm_table_with_loc
{
    entry kv_arr[TABLE_SIZE]; // data entry array of hash table 每一个桶中可以存放的元素数量
    uint64_t fill_num;        // amount of occupied slots in kv_arr 桶中被占用的数量
    pm_table_without_loc* next;     // the file address of overflow hash table 哈希溢出表的地址
    pthread_rwlock_t m_lock;
    bool dump_flag = false;//该哈希槽是否被固化过
};



// persistent memory linear hash
class Concurr_LHash
{
private:
    private:
    void *start_addr;    // the start address of mapped file
    void *overflow_addr; // the start address of overflow table array 溢出表内存的起始地址
    metadata *meta;      // virtual address of metadata 哈希表的信息描述结构
    pm_table_without_loc *free_overflow_head;//指向第一个之前被释放的溢出表
    pm_table_with_loc *table_arr; // virtual address of hash table array 哈希槽的启示地址
    pm_table_without_loc *overflow_arr; // virtual address of overflow table array 指向第一个未分配的溢出表位置
    pthread_rwlock_t find_free;


    int insert_bucket(pm_table_with_loc *addr, entry en);
    void split();
    uint64_t hashFunc(const uint64_t &key, const size_t &hash_size);
    //pm_table *newOverflowTable(uint64_t &offset);
    pm_table_without_loc *find_first_free_table();
    void huishou_free_overflow_table(pm_table_without_loc *);
    Concurr_LHash();
    //数据固化相关函数
    void dump_bucket(pm_table_with_loc *addr);
    int32_t get_dump_data(uint64_t hash_value, uint64_t target);
    bool remove_dump_data(uint64_t hash_value, uint64_t target);

    bool DeleteFile(const char* path);
    void Getfilepath(const char *path, const char *filename,  char *filepath);

public:
    static Concurr_LHash* get_Lhash();
    ~Concurr_LHash();
    bool insert(const uint64_t &key, const uint32_t &value);
    int32_t get(const uint64_t &key);
    bool remove(const uint64_t &key);
    bool update(const uint64_t &key, const uint32_t &value);
    void range(const uint64_t &start, const uint64_t &end, vector<unsigned int> &res);
    uint64_t res_element_num();
    void pre_split(int count);
    void disp();
};