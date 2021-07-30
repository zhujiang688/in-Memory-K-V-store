#include "liner_hash.h"
/**
 * PMLHash::PMLHash 
 * 
 * @param  {char*} file_path : the file path of data file
 * if the data file exist, open it and recover the hash
 * if the data file does not exist, create it and initial the hash
 */
LHash::LHash()
{
    //开辟哈希表的储存地址
    if ((start_addr = malloc(FILE_SIZE)) == NULL)
    {
        cout << ("memory mallloc failed");
        exit(1);
    }
    else
    {
        //内存初始化
        memset(start_addr, 0, FILE_SIZE);
        //分配各个结构的初始地址
        overflow_addr = (void *)((uint64_t)start_addr + FILE_SIZE / 2); //溢出key的分配地址
        meta = (metadata *)start_addr;
        free_overflow_head = (pm_table *)((uint64_t)start_addr + sizeof(metadata));
        table_arr = (pm_table *)((uint64_t)start_addr + sizeof(metadata) + sizeof(pm_table));
        overflow_arr = (pm_table *)overflow_addr;
        if (meta->size == 0)
            meta->size = 16;
        meta->max_size = ((uint64_t)overflow_addr - (uint64_t)table_arr)/sizeof(pm_table);
        meta->max_overflow_size = (FILE_SIZE / 2) / sizeof(pm_table);
    }
}

LHash *LHash::get_Lhash()
{
    static LHash *my_lhash = new LHash;
    return my_lhash;
}

/**
 * PMLHash::~PMLHash 
 * 
 * unmap and close the data file
 */
LHash::~LHash()
{
    //释放哈希表的地址
    free(start_addr);
}

/**
 * PMLHash 
 * 
 * @param  {pm_table *} addr : address of hash table to be inserted
 * @param  {entry} en        : the entry to insert
 * @return {int}             : success: 0. fail: -1
 * 
 * insert a key-value pair into a specified address
 */
void LHash::insert_bucket(pm_table *addr, entry en) //向哈希桶中插入一个值
{
    pm_table *table = addr;
    //遍历检查是否已经存在该值
    while (table->next_offset != 0)
    {
        for (int i = 0; i < TABLE_SIZE; i++)
        {
            if (table->kv_arr[i].key == en.key)
            {
                return;
            }
        }
        table = (pm_table *)table->next_offset;
    }
    //检查最后的溢出表里面有没有重复的
    for (int i = 0; i < table->fill_num; i++)
    {
        if (table->kv_arr[i].key == en.key)
        {
            return;
        }
    }
    //插入操作
    if (table->fill_num >= 16) //如果该桶满了
    {
        pm_table *new_table = find_first_free_table(); //获取一个空闲的桶
        table->next_offset = (uint64_t)(new_table);
        table = (pm_table *)table->next_offset;
    }
    table->kv_arr[table->fill_num] = en;
    table->fill_num++;
}

/**
 * PMLHash 
 * 
 * split the hash table indexed by the meta->next
 * update the metadata
 */
void LHash::split()
{
    // fill the split table
    vector<entry> temp_arr;
    int hash_num = (1 << meta->level) * HASH_SIZE * 2;
    pm_table *split_table = &table_arr[meta->next];
    while (true)
    {
        for (uint64_t i = 0; i < split_table->fill_num; i++)
        {
            uint64_t hash_value = hashFunc(split_table->kv_arr[i].key, hash_num);
            // move to the new table
            if (hash_value != meta->next)
            {
                pm_table *new_table = &table_arr[hash_value];
                entry en = {
                    key : split_table->kv_arr[i].key,
                    value : split_table->kv_arr[i].value
                };
                insert_bucket(new_table, en);
            }
            // stay in the old table, move to temp_arr first
            else
            {
                entry en = {
                    key : split_table->kv_arr[i].key,
                    value : split_table->kv_arr[i].value
                };
                temp_arr.push_back(en);
            }
        }
        if (split_table->next_offset == 0) //没有溢出表
            break;
        split_table = (pm_table *)(split_table->next_offset);
    }
    //fill old table
    split_table = &table_arr[meta->next];
    split_table->fill_num = 0;
    for (size_t i = 0; i < temp_arr.size(); i++)
    {
        if (split_table->fill_num >= 16)
        {
            split_table = (pm_table *)split_table->next_offset;
            split_table->fill_num = 0;
        }
        split_table->kv_arr[split_table->fill_num++] = temp_arr[i];
    }
    vector<uint64_t> stk;
    while (split_table->next_offset != 0) //重置多余的条目
    {
        stk.push_back(split_table->next_offset);
        uint64_t offset = split_table->next_offset;
        split_table->next_offset = 0;
        //添加到头部

        split_table = (pm_table *)offset;
    }
    for (size_t i = 0; i < stk.size(); i++) //重置多余的溢出表
    {
        huishou_free_overflow_table(stk[i]);
    }
    meta->next++; //分裂顶点移动
    meta->size++;//代表用了多少哈希槽
    if (meta->next == (uint64_t)((1 << meta->level) * HASH_SIZE)) //分裂顶点达到最大
    {
        meta->next = 0;
        meta->level++;
    }
}

/**
 * PMLHash 
 * 
 * @param  {uint64_t} key     : key
 * @param  {size_t} hash_size : the N in hash func: idx = hash % N
 * @return {uint64_t}         : index of hash table array
 * 
 * need to hash the key with proper hash function first
 * then calculate the index by N module
 */
uint64_t LHash::hashFunc(const uint64_t &key, const size_t &hash_size)
{
    return (key * 3) % hash_size;
}

/**
 * PMLHash 
 * 
 * @return {pm_table*} : the virtual address of the first
 *                       overflow hash table
 */
pm_table *LHash::find_first_free_table()
{
    if (free_overflow_head->next_offset == 0) //没有回收的空闲溢出表
    {
        return &overflow_arr[meta->overflow_num++];
    }
    pm_table * res = (pm_table*)free_overflow_head->next_offset;
    free_overflow_head->next_offset = ((pm_table *)free_overflow_head->next_offset)->next_offset;
    res->next_offset = 0;
    return res;
}

void LHash::huishou_free_overflow_table(uint64_t loc)
{
    //添加到队列头部
    pm_table* free_table = (pm_table*)loc;
    free_table->fill_num = 0;
    free_table->next_offset = free_overflow_head->next_offset;
    free_overflow_head->next_offset = loc;
}

/**
 * PMLHash 
 * 
 * @param  {uint64_t} key   : inserted key
 * @param  {uint64_t} value : inserted value
 * @return {int}            : success: 0. fail: -1
 * 
 * insert the new kv pair in the hash
 * 
 * always insert the entry in the first empty slot
 * 
 * if the hash table is full then split is triggered
 */
bool LHash::insert(const uint64_t &key, const uint32_t &value)
{
    //内存安全检查
    if(free_overflow_head->next_offset == 0 && meta->overflow_num == meta->max_overflow_size)//没有剩余的溢出表
    {
        return false;
    }

    uint64_t hash_value = hashFunc(key, (1 << (meta->level)) * HASH_SIZE);
    if (hash_value < meta->next)
        hash_value = hashFunc(key, (1 << meta->level) * HASH_SIZE * 2);
    pm_table *table = table_arr + hash_value;
    entry en{
        key : key,
        value : value
    };
    meta->total++;
    insert_bucket(table, en);

    if (meta->size < meta->max_size && (double)(meta->total) / (double)(TABLE_SIZE * meta->size) > 0.9)
        split();//负载达到0.9时做一次分裂
    return true;
}

/**
 * PMLHash 
 * 
 * @param  {uint64_t} key   : the searched key
 * @param  {uint64_t} value : return value if found
 * @return {int}            : 0 found, -1 not found
 * 
 * search the target entry and return the value
 */
int32_t LHash::get(const uint64_t &key)
{
    uint32_t value;
    uint64_t hash_value = hashFunc(key, (1 << meta->level) * HASH_SIZE);
    if (hash_value < meta->next)
        hash_value = hashFunc(key, (1 << meta->level) * HASH_SIZE * 2);
    pm_table *p = &table_arr[hash_value];
    while (true)
    {
        //search the temp pm_table;
        for (uint64_t i = 0; i < p->fill_num; i++)
        {
            if (p->kv_arr[i].key == key)
            {
                value = p->kv_arr[i].value;
                return value;
            }
        }
        //if this table is full-filled, switch to the next_offset
        if (p->next_offset)
            p = (pm_table *)p->next_offset;
        //else break the loop
        else
            break;
    }
    return -1;
}

/**
 * PMLHash 
 * 
 * @param  {uint64_t} key : target key
 * @return {int}          : success: 0. fail: -1
 * 
 * remove the target entry, move entries after forward
 * if the overflow table is empty, remove it from hash
 */
bool LHash::remove(const uint64_t &key)
{
    uint64_t hash_value = hashFunc(key, (1 << meta->level) * HASH_SIZE);
    if (hash_value < meta->next)
        hash_value = hashFunc(key, (1 << meta->level) * HASH_SIZE * 2);
    pm_table *p = &table_arr[hash_value];
    pm_table *temp, *previous_table;
    int tag;
    while (true)
    {
        for (uint64_t i = 0; i < p->fill_num; i++)
        {
            if (p->kv_arr[i].key == key)
            {
                //move the last element to the tag place, and the total of elements substract 1;
                temp = p;
                //move to the last pm_table
                while (p->next_offset)
                {
                    previous_table = p;
                    p = (pm_table *)p->next_offset;
                }
                //move the last element to the tagged place, and the last pm_table delete the last one element
                temp->kv_arr[i].key = p->kv_arr[p->fill_num - 1].key;
                temp->kv_arr[i].value = p->kv_arr[p->fill_num - 1].value;
                p->fill_num--;
                meta->total--;
                //the last pm_table is empty and need to be removed
                if (p->fill_num == 0) //是一个空闲表
                {
                    if ((uint64_t)p >= (uint64_t)overflow_addr) //是一个溢出表
                    {
                        huishou_free_overflow_table((uint64_t)p);
                    }
                    previous_table->next_offset = 0;
                }
                return true;
            }
        }
        if (p->next_offset == 0)
            break;
        previous_table = p;
        p = (pm_table *)p->next_offset;
    }
    return false;
}

/**
 * PMLHash 
 * 
 * @param  {uint64_t} key   : target key
 * @param  {uint64_t} value : new value
 * @return {int}            : success: 0. fail: -1
 * 
 * update an existing entry
 */
bool LHash::update(const uint64_t &key, const uint32_t &value)
{
    uint64_t hash_value = hashFunc(key, (1 << meta->level) * HASH_SIZE);
    if (hash_value < meta->next)
        hash_value = hashFunc(key, (1 << meta->level) * HASH_SIZE * 2);
    pm_table *p = &table_arr[hash_value];
    while (true)
    {
        //search the temp pm_table;
        for (uint64_t i = 0; i < p->fill_num; i++)
        {
            if (p->kv_arr[i].key == key)
            {
                p->kv_arr[i].value = value;
                return true;
            }
        }
        //if this table is full-filled, switch to the next_offset
        if (p->next_offset)
            p = (pm_table *)p->next_offset;
        //else break the loop
        else
            break;
    }
    return false;
}

void LHash::range(const uint64_t &start, const uint64_t &end, vector<unsigned int> &res)
{
    for (int i = 0; i < meta->size; i++)
    {
        pm_table *temp = &table_arr[i];
        while (temp)
        {
            for (int j = 0; j < temp->fill_num; j++)
            {
                if (temp->kv_arr[j].key <= end && temp->kv_arr[j].key >= start)
                {
                    res.push_back(temp->kv_arr[j].value);
                }
            }
            if (temp->next_offset)
            {
                temp = (pm_table *)temp->next_offset;
            }
            else
                break;
        }
    }
}

uint64_t LHash::res_element_num()
{
    return meta->total;
}


