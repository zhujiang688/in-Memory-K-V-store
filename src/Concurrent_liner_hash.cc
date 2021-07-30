#include "Concurrent_liner_hash.h"
/**
 * PMLHash::PMLHash 
 * 
 * @param  {char*} file_path : the file path of data file
 * if the data file exist, open it and recover the hash
 * if the data file does not exist, create it and initial the hash
 */


Concurr_LHash::Concurr_LHash()
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
        table_arr = (pm_table_with_loc *)((uint64_t)start_addr + sizeof(metadata));
        overflow_arr = (pm_table_without_loc *)overflow_addr;
        meta->size = HASH_SIZE;
        meta->max_size = ((uint64_t)overflow_addr - (uint64_t)table_arr) / sizeof(pm_table_with_loc);
        meta->max_overflow_size = (FILE_SIZE / 2) / sizeof(pm_table_without_loc);
        for (int i = 0; i < meta->max_size; i++) //初始化锁
        {
            pthread_rwlock_init(&table_arr[i].m_lock, NULL);
        }
        pthread_rwlock_init(&find_free, NULL);

        meta->level = 0;
        meta->overflow_num = 0;
        meta->next = 0;
        meta->total = 0;
        free_overflow_head = nullptr;

        //清空持久化文件夹
        DeleteFile("./file_con");

    }
}

Concurr_LHash *Concurr_LHash::get_Lhash()
{
    static Concurr_LHash *my_lhash = new Concurr_LHash;
    return my_lhash;
}

/**
 * PMLHash::~PMLHash 
 * 
 * unmap and close the data file
 */
Concurr_LHash::~Concurr_LHash()
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
int Concurr_LHash::insert_bucket(pm_table_with_loc *addr, entry en) //向哈希桶中插入一个值
{
    pm_table_with_loc *table = addr;
    pthread_rwlock_wrlock(&table->m_lock);

    //该哈希槽是否固化过数据
    if(table->dump_flag)
    {
        //首先查找固化的数据中是否存在该键
        if(get_dump_data((addr - table_arr),en.key) != -1)//存在
        {
            pthread_rwlock_unlock(&addr->m_lock);
            return 0;
        }
    }

    //遍历检查是否已经存在该值
    while (table->next != NULL)
    {
        for (int i = 0; i < TABLE_SIZE; i++)
        {
            if (table->kv_arr[i].key == en.key)
            {
                pthread_rwlock_unlock(&addr->m_lock);
                return 0;
            }
        }
        table = (pm_table_with_loc *)(table->next);
    }
    //检查最后的溢出表里面有没有重复的
    for (int i = 0; i < table->fill_num; i++)
    {
        if (table->kv_arr[i].key == en.key)
        {
            pthread_rwlock_unlock(&addr->m_lock);
            return 0;
        }
    }
    //cout<<2<<endl;
    //插入操作
    if (table->fill_num == TABLE_SIZE) //如果该桶满了
    {
        //cout<<3<<endl;
        pm_table_without_loc *new_table = find_first_free_table(); //获取一个空闲的桶
        if (new_table == nullptr) //空间已满，没有多余的溢出桶
        {
            //将该哈希桶固化
            dump_bucket(addr);
            addr->kv_arr[addr->fill_num++] = en;//经过固化，该哈希桶时空的
            pthread_rwlock_unlock(&addr->m_lock);
            return 1;
        }
        table->next = new_table;
        table = (pm_table_with_loc *)(table->next);
        table->fill_num = 0;
    }
    table->kv_arr[table->fill_num] = en;
    table->fill_num++;
    pthread_rwlock_unlock(&addr->m_lock);
    return 1;
}

void Concurr_LHash::split()
{
    pm_table_with_loc *loc_table = &table_arr[meta->next];
    pthread_rwlock_wrlock(&loc_table->m_lock); //对分割点所在哈希槽上锁
    if(&table_arr[meta->next] != loc_table)//分裂点发生了移动
    {
        pthread_rwlock_unlock(&loc_table->m_lock);
        return;
    }

    pm_table_with_loc *split_table = &table_arr[meta->next];
    // fill the split table
    vector<entry> temp_arr;
    //cout<<"分割点"<<meta->next<<endl;
    int hash_num = (1 << meta->level) * HASH_SIZE * 2;
    while (true)
    {
        for (uint64_t i = 0; i < split_table->fill_num; i++)
        {
            uint64_t hash_value = hashFunc(split_table->kv_arr[i].key, hash_num);
            // move to the new table
            if (hash_value != meta->next)
            {
                //cout<<hash_value<<endl;
                pm_table_with_loc *new_table = &table_arr[hash_value];
                entry en = {
                    key : split_table->kv_arr[i].key,
                    value : split_table->kv_arr[i].value
                };
                assert(insert_bucket(new_table, en) != -1);//分裂过程中不能出现空间不足的情况
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
        if (split_table->next == NULL) //没有溢出表
            break;
        split_table = (pm_table_with_loc *)(split_table->next);
    }
    //cout<<"fill new table down"<<endl;
    //fill old table
    split_table = &table_arr[meta->next];
    split_table->fill_num = 0;
    for (size_t i = 0; i < temp_arr.size(); i++)
    {
        if (split_table->fill_num == TABLE_SIZE)
        {
            split_table = (pm_table_with_loc *)(split_table->next);
            split_table->fill_num = 0;
        }
        split_table->kv_arr[split_table->fill_num++] = temp_arr[i];
    }
    //cout<<"fill old table down"<<endl;
    vector<pm_table_without_loc *> stk;
    while (split_table->next != 0) //重置多余的条目
    {
        stk.push_back(split_table->next);
        pm_table_without_loc *offset = split_table->next;
        split_table->next = 0;
        //添加到头部
        split_table = (pm_table_with_loc *)offset;
    }
    for (size_t i = 0; i < stk.size(); i++) //重置多余的溢出表
    {
        huishou_free_overflow_table(stk[i]);
    }
    //cout<<"clear table down"<<endl;
    meta->next++;                                                 //分裂顶点移动
    meta->size++;                                                 //代表用了多少哈希槽
    if (meta->next == (uint64_t)((1 << meta->level) * HASH_SIZE)) //分裂顶点达到最大
    {
        meta->next = 0;
        meta->level++;
    }
    pthread_rwlock_unlock(&loc_table->m_lock);
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
uint64_t Concurr_LHash::hashFunc(const uint64_t &key, const size_t &hash_size)
{
    return (key) % hash_size;
}

/**
 * PMLHash 
 * 
 * @return {pm_table*} : the virtual address of the first
 *                       overflow hash table
 */
pm_table_without_loc *Concurr_LHash::find_first_free_table()
{
    pthread_rwlock_wrlock(&find_free); //对该哈希桶上锁
    pm_table_without_loc *res = nullptr;
    if (free_overflow_head == nullptr) //没有回收的空闲溢出表
    {
        if (meta->overflow_num == meta->max_overflow_size) //空间不足
        {
            pthread_rwlock_unlock(&find_free);
            return nullptr;
        }
        res = &overflow_arr[meta->overflow_num++];
    }
    else
    {
        res = free_overflow_head;
        free_overflow_head = free_overflow_head->next;
        res->next = nullptr;
    }
    pthread_rwlock_unlock(&find_free);
    return res;
}

void Concurr_LHash::huishou_free_overflow_table(pm_table_without_loc *free_table)
{
    pthread_rwlock_wrlock(&find_free); //对该哈希桶上锁
    //添加到队列头部
    free_table->fill_num = 0;
    free_table->next = free_overflow_head;
    free_overflow_head = free_table;
    pthread_rwlock_unlock(&find_free);
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
bool Concurr_LHash::insert(const uint64_t &key, const uint32_t &value)
{
    entry en{
        key : key,
        value : value
    };
    int ind;
    uint64_t hash_value = hashFunc(key, (1 << (meta->level)) * HASH_SIZE);
    if (hash_value < meta->next)
        hash_value = hashFunc(key, (1 << meta->level) * HASH_SIZE * 2);
    ind = insert_bucket(table_arr + hash_value, en);

    if (ind == 1)
    {
        meta->total++;          //成功插入的情况才增加
    }
    else if (ind == 0) //已经有该键值存在
    {
        return true;
    }

    if (meta->size < meta->max_size && ((double)(meta->total) / (double)(TABLE_SIZE * (meta->size))) > 0.9)
    {
        split(); //负载达到0.9时做一次分裂
    }
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
int32_t Concurr_LHash::get(const uint64_t &key)
{
    uint32_t value;
    uint64_t hash_value = hashFunc(key, (1 << meta->level) * HASH_SIZE);
    if (hash_value < meta->next)
        hash_value = hashFunc(key, (1 << meta->level) * HASH_SIZE * 2);
    pm_table_with_loc *p = &table_arr[hash_value];
    pthread_rwlock_rdlock(&p->m_lock);

    //首先判断固化内存中有没有
    if(p->dump_flag)
    {
        int res = get_dump_data(hash_value, key);
        if(res != -1)//有
        {
            pthread_rwlock_unlock(&table_arr[hash_value].m_lock);
            return res;
        }
    }

    while (true)
    {
        //search the temp pm_table;
        for (int i = 0; i < p->fill_num; i++)
        {
            if (p->kv_arr[i].key == key)
            {
                value = p->kv_arr[i].value;
                pthread_rwlock_unlock(&table_arr[hash_value].m_lock);
                return value;
            }
        }
        //if this table is full-filled, switch to the next_offset
        if (p->next)
            p = (pm_table_with_loc *)p->next;
        //else break the loop
        else
            break;
    }
    pthread_rwlock_unlock(&table_arr[hash_value].m_lock);
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
bool Concurr_LHash::remove(const uint64_t &key)
{
    uint64_t hash_value = hashFunc(key, (1 << meta->level) * HASH_SIZE);
    if (hash_value < meta->next)
        hash_value = hashFunc(key, (1 << meta->level) * HASH_SIZE * 2);
    pm_table_with_loc *p = &table_arr[hash_value];
    pthread_rwlock_wrlock(&p->m_lock);

    //首先判断固化内存中有没有
    if(p->dump_flag)
    {
        if(remove_dump_data(hash_value, key))//有
        {
            meta->total--;
            pthread_rwlock_unlock(&table_arr[hash_value].m_lock);
            return true;
        }
    }

    pm_table_with_loc *temp, *previous_table;
    while (true)
    {
        for (int i = 0; i < p->fill_num; i++)
        {
            if (p->kv_arr[i].key == key)
            {
                //move the last element to the tag place, and the total of elements substract 1;
                temp = p;
                //move to the last pm_table
                while (p->next)
                {
                    previous_table = p;
                    p = (pm_table_with_loc *)p->next;
                }
                //move the last element to the tagged place, and the last pm_table delete the last one element
                temp->kv_arr[i].key = p->kv_arr[p->fill_num - 1].key;
                temp->kv_arr[i].value = p->kv_arr[p->fill_num - 1].value;
                p->fill_num--;
                meta->total--;
                //the last pm_table is empty and need to be removed
                if (p->fill_num == 0) //是一个空闲表
                {
                    if ((uint64_t)p >= (uint64_t)&overflow_arr[0]) //是一个溢出表
                    {
                        huishou_free_overflow_table((pm_table_without_loc *)p);
                        previous_table->next = 0;
                    }
                }
                pthread_rwlock_unlock(&table_arr[hash_value].m_lock);
                return true;
            }
        }
        if (p->next == 0)
            break;
        previous_table = p;
        p = (pm_table_with_loc *)p->next;
    }
    pthread_rwlock_unlock(&table_arr[hash_value].m_lock);
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
bool Concurr_LHash::update(const uint64_t &key, const uint32_t &value)
{
    uint64_t hash_value = hashFunc(key, (1 << meta->level) * HASH_SIZE);
    if (hash_value < meta->next)
        hash_value = hashFunc(key, (1 << meta->level) * HASH_SIZE * 2);
    pm_table_with_loc *p = &table_arr[hash_value];
    pthread_rwlock_wrlock(&p->m_lock);

    //首先判断固化内存中有没有
    if(p->dump_flag)
    {
        if(remove_dump_data(hash_value, key))//有并且成功删除
        {
            //重新插入该键值对
            meta->total--;
            pthread_rwlock_unlock(&table_arr[hash_value].m_lock);
            insert(key, value);
            return true;
        }
    }

    while (true)
    {
        //search the temp pm_table;
        for (int i = 0; i < p->fill_num; i++)
        {
            if (p->kv_arr[i].key == key)
            {
                p->kv_arr[i].value = value;
                pthread_rwlock_unlock(&table_arr[hash_value].m_lock);
                return true;
            }
        }
        //if this table is full-filled, switch to the next_offset
        if (p->next)
            p = (pm_table_with_loc *)p->next;
        //else break the loop
        else
            break;
    }
    pthread_rwlock_unlock(&table_arr[hash_value].m_lock);
    return false;
}

void Concurr_LHash::range(const uint64_t &start, const uint64_t &end, vector<unsigned int> &res)
{
    uint32_t value;
    for (uint64_t i = start; i <= end; i++)
    {
        value = get(i);
        if (value != -1)
            res.push_back(value);
    }
}

uint64_t Concurr_LHash::res_element_num()
{
    return meta->total;
}

void Concurr_LHash::disp()
{
    cout << "degree: " << meta->level << endl;
    cout << "split point: " << meta->next << endl;
    cout << "table size: " << meta->size << endl;
    for (int i = 0; i <= meta->size; i++)
    {
        pm_table_with_loc *temp = &table_arr[i];
        while (temp->next)
        {
            for (int j = 0; j < TABLE_SIZE; j++)
            {
                cout << temp->kv_arr[j].key << " ";
            }
            cout << "*";
            temp = (pm_table_with_loc *)temp->next;
        }
        for (int j = 0; j < temp->fill_num; j++)
        {
            cout << temp->kv_arr[j].key << " ";
        }
        cout << endl;
    }
}

void Concurr_LHash::dump_bucket(pm_table_with_loc *addr)
{
    pm_table_with_loc* dump_table = addr;

    //保存
    string file_name = "./file_con/" + to_string((addr - table_arr));
    ofstream file (file_name, ios::out | ios::app | ios::binary);
    while(dump_table->next)
    {
        file.write((char*)&dump_table->kv_arr, sizeof(dump_table->kv_arr));
        dump_table = (pm_table_with_loc*)dump_table->next;
    }
    file.write((char*)&dump_table->kv_arr, sizeof(entry) * dump_table->fill_num);
    file.close();

    //清理哈希槽
    dump_table = addr;
    dump_table->dump_flag = true;
    dump_table->fill_num = 0;
    pm_table_without_loc* temp = dump_table->next;
    dump_table->next = nullptr;

    while(temp)
    {
        pm_table_without_loc* huishou = temp;
        temp = temp->next;
        huishou->fill_num = 0;
        huishou->next = nullptr;
        huishou_free_overflow_table(huishou);
    }
}

int32_t Concurr_LHash::get_dump_data(uint64_t hash_value, uint64_t target)
{
    string file_name = "./file_con/" + to_string(hash_value);
    ifstream file (file_name, ios::in | ios::binary);
    file.seekg(0,std::ios::end);
    int len = file.tellg();
    int size =  len/sizeof(entry);
    file.seekg(0,std::ios::beg);
    entry *buff = new entry[size];
    file.read((char*)buff , len);
    file.close();
    int res = -1;
    for(int i = 0;i<size;i++)
    {
        if(buff[i].key == target)
        {
            res = buff[i].value;
            break;
        }
    }
    delete [] buff;
    return res;
}

bool Concurr_LHash::remove_dump_data(uint64_t hash_value, uint64_t target)
{
    string file_name = "./file_con/" + to_string(hash_value);
    ifstream in_file (file_name, ios::in | ios::binary);
    in_file.seekg(0,std::ios::end);
    int len = in_file.tellg();
    int size =  len/sizeof(entry);
    in_file.seekg(0,std::ios::beg);
    entry *buff = new entry[size];
    in_file.read((char*)buff , len);
    in_file.close();

    bool res = false;
    ofstream out_file (file_name, ios::out | std::ios::trunc | ios::binary);
    //查找并删除
    for(int i = 0;i<size;i++)
    {
        if(buff[i].key == target)
        {
            res = true;
            swap(buff[i], buff[size -1]);
            out_file.write((char*)buff, len - sizeof(entry));
            break;
        }
    }
    out_file.close();
    delete [] buff;
    return res;
}

void Concurr_LHash::pre_split(int count)
{
    while (meta->size < meta->max_size)
    {
        meta->next++;
        meta->size++;
        if (meta->next == (uint64_t)((1 << meta->level) * HASH_SIZE)) //分裂顶点达到最大
        {
            meta->next = 0;
            meta->level++;
        }
        if (meta->size == count)//达到指定值
            break;
    }
    cout<<"pre split down, hash bucket num: "<<meta->size<<endl;
}

void Concurr_LHash::Getfilepath(const char *path, const char *filename,  char *filepath)
{
    strcpy(filepath, path);
    if(filepath[strlen(path) - 1] != '/')
        strcat(filepath, "/");
    strcat(filepath, filename);
}
 
bool Concurr_LHash::DeleteFile(const char* path)
{
    DIR *dir;
    struct dirent *dirinfo;
    struct stat statbuf;
    char filepath[256] = {0};
    lstat(path, &statbuf);
    
    if (S_ISREG(statbuf.st_mode))//判断是否是常规文件
    {
        std::remove(path);
    }
    else if (S_ISDIR(statbuf.st_mode))//判断是否是目录
    {
        if ((dir = opendir(path)) == NULL)
            return 1;
        while ((dirinfo = readdir(dir)) != NULL)
        {
            Getfilepath(path, dirinfo->d_name, filepath);
            if (strcmp(dirinfo->d_name, ".") == 0 || strcmp(dirinfo->d_name, "..") == 0)//判断是否是特殊目录
            continue;
            DeleteFile(filepath);
            rmdir(filepath);
        }
        closedir(dir);
    }
    return 0;
}