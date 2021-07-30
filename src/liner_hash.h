#ifndef __LINER_HASH
#define __LINER_HASH

#include <stdint.h>
#include <iostream>
#include <unistd.h>
#include <memory.h>
#include <stdlib.h>
#include <vector>
#include <assert.h>



#define TABLE_SIZE 16              // adjustable 一个桶的容量
#define HASH_SIZE 16             // adjustable 哈希函数取余的值
#define FILE_SIZE 1024UL * 1024 * 16 // 16 MB K_V store的内存大小为4GB

using namespace std;



//内存信息描述单元
typedef struct metadata
{
    size_t size;           // the size of whole hash table array 哈希槽的数量
    size_t level;          // level of hash 当前使用的哈希函数等级
    uint64_t next;         // the index of the next split hash table 分裂顶点的位置
    uint64_t overflow_num; // amount of overflow hash tables 溢出表的数目
    uint64_t total;        // the number of total elements 哈希表中存储的元素数量，包括溢出表
    uint64_t max_size;
    uint64_t max_overflow_size;
} metadata;

// data entry of hash table
//哈希槽中存储的元素
typedef struct entry
{
    uint64_t key;
    uint32_t value;
} entry;

// hash table
//一个哈希槽或则溢出表
typedef struct pm_table
{
    entry kv_arr[TABLE_SIZE]; // data entry array of hash table 每一个桶中可以存放的元素数量
    uint64_t fill_num;        // amount of occupied slots in kv_arr 桶中被占用的数量
    uint64_t next_offset;     // the file address of overflow hash table 哈希溢出表的地址
    //uint64_t pm_flag;         // 1 -- occupied, 0 -- available
} pm_table;



// persistent memory linear hash
class LHash
{
private:
    void *start_addr;    // the start address of mapped file
    void *overflow_addr; // the start address of overflow table array 溢出表内存的起始地址
    metadata *meta;      // virtual address of metadata 哈希表的信息描述结构
    pm_table *free_overflow_head;//指向第一个之前被释放的溢出表
    pm_table *table_arr; // virtual address of hash table array 哈希槽的启示地址
    pm_table *overflow_arr; // virtual address of overflow table array 指向第一个未分配的溢出表位置
    

    void insert_bucket(pm_table *addr, entry en);
    void split();
    uint64_t hashFunc(const uint64_t &key, const size_t &hash_size);
    //pm_table *newOverflowTable(uint64_t &offset);
    pm_table *find_first_free_table();
    void huishou_free_overflow_table(uint64_t loc);
    LHash();

    //消息储存器
    

public:
    static LHash* get_Lhash();
    ~LHash();
    bool insert(const uint64_t &key, const uint32_t &value);
    int32_t get(const uint64_t &key);
    bool remove(const uint64_t &key);
    bool update(const uint64_t &key, const uint32_t &value);
    void range(const uint64_t &start, const uint64_t &end, vector<unsigned int> &res);
    uint64_t res_element_num();
};

#endif