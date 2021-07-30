#ifndef __STRUCT
#define __STRUCt
#include <stdint.h>
#define TABLE_SIZE 20             // 一个桶的容量
#define FILE_SIZE 1024UL * 1024 * 1024 * 4 // 内存大小为4GB
#define TABLE_NUM 100000   //哈希槽的数量
#define HASH_VALUE 200 
// data entry of hash table
typedef struct entry
{
    uint64_t key;
    uint32_t value;
} entry;

//桶的结构
struct pm_table
{
    entry kv_arr[TABLE_SIZE]; // data entry array of hash table 每一个桶中可以存放的元素数量
    uint64_t fill_num;        // amount of occupied slots in kv_arr 桶中被占用的数量
    pm_table* next;     // the file address of overflow hash table 哈希溢出表的地址
};

#endif