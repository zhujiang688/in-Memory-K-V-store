#include "client.h"

void cmd_msg_cb(int fd, short events, void *arg)
{
    client *ptr = (client *)arg;
    char msg[1024];
    int ret = read(fd, msg, sizeof(msg));
    if (ret < 0)
    {
        perror("read fail ");
        exit(1);
    }
    if (msg[0] == 'S')
    { //设置参数
        std::cout << "Please enter operator name: ";
        bzero(msg, 1024);
        fgets(msg, 1024, stdin);
        msg[strcspn(msg, "\n")] = 0;
        if(strcmp(msg, "set_args") == 0)
        {
            std::cout << "Please enter thread num: ";
            bzero(msg, 1024);
            fgets(msg, 1024, stdin);
            msg[strcspn(msg, "\n")] = 0;
            int thread_num = stoi(msg);
            std::cout << "Please enter insert ratio: ";
            bzero(msg, 1024);
            fgets(msg, 1024, stdin);
            msg[strcspn(msg, "\n")] = 0;
            int ins_rio = stoi(msg);
            std::cout << "Please enter update ratio: ";
            bzero(msg, 1024);
            fgets(msg, 1024, stdin);
            msg[strcspn(msg, "\n")] = 0;
            int up_rio = stoi(msg);
            std::cout << "Please enter delete ratio: ";
            bzero(msg, 1024);
            fgets(msg, 1024, stdin);
            msg[strcspn(msg, "\n")] = 0;
            int del_rio = stoi(msg);
            std::cout << "Please enter get ratio: ";
            bzero(msg, 1024);
            fgets(msg, 1024, stdin);
            msg[strcspn(msg, "\n")] = 0;
            int get_rio = stoi(msg);
            std::cout << "Please enter range ratio: ";
            bzero(msg, 1024);
            fgets(msg, 1024, stdin);
            msg[strcspn(msg, "\n")] = 0;
            int ran_rio = stoi(msg);
            ptr->set_args(thread_num, ins_rio, up_rio, del_rio, get_rio, ran_rio);
        }
        else if(strcmp(msg, "show_args") == 0)
        {
            ptr->disp_args();
        }
        else if(strcmp(msg, "range_len") == 0)
        {
            std::cout << "Please enter range len: ";
            bzero(msg, 1024);
            fgets(msg, 1024, stdin);
            msg[strcspn(msg, "\n")] = 0;
            ptr->set_range_len(stoi(msg));
        }
        else if(strcmp(msg, "set_value_range") == 0)
        {
            std::cout << "Please enter min value: ";
            bzero(msg, 1024);
            fgets(msg, 1024, stdin);
            msg[strcspn(msg, "\n")] = 0;
            int min_v = stoi(msg);
            std::cout << "Please enter max value: ";
            bzero(msg, 1024);
            fgets(msg, 1024, stdin);
            msg[strcspn(msg, "\n")] = 0;
            ptr->set_value_range(min_v, stoi(msg));
        }
        else if(strcmp(msg, "set_key_range") == 0)
        {
            std::cout << "Please enter min key: ";
            bzero(msg, 1024);
            fgets(msg, 1024, stdin);
            msg[strcspn(msg, "\n")] = 0;
            int min_k = stoi(msg);
            std::cout << "Please enter max key: ";
            bzero(msg, 1024);
            fgets(msg, 1024, stdin);
            msg[strcspn(msg, "\n")] = 0;
            ptr->set_key_range(min_k, stoi(msg));
        }
        else if(strcmp(msg, "use log") == 0)
        {
            ptr->use_log();
        }
        else if(strcmp(msg, "forbiden log") == 0)
        {
            ptr->forbidden_log();
        }
        else if(strcmp(msg, "run") == 0)
        {
            ptr->start_run();
        }
        else if(strcmp(msg, "test") == 0)
        {
            std::cout << "Please enter count number: ";
            bzero(msg, 1024);
            fgets(msg, 1024, stdin);
            msg[strcspn(msg, "\n")] = 0;
            ptr->test(stoi(msg));
        }
        else if(strcmp(msg, "verify") == 0)
        {
            std::cout << "Please enter file path: ";
            bzero(msg, 1024);
            fgets(msg, 1024, stdin);
            msg[strcspn(msg, "\n")] = 0;
            ptr->verify(msg);
        }
        else if(strcmp(msg, "set operator num") == 0)
        {
            std::cout << "Please enter operator num per thread: ";
            bzero(msg, 1024);
            fgets(msg, 1024, stdin);
            msg[strcspn(msg, "\n")] = 0;
            ptr->set_operator_per_thread(stoi(msg));
        }
        else if(strcmp(msg, "pre split") == 0)
        {
            std::cout << "Please enter split num per: ";
            bzero(msg, 1024);
            fgets(msg, 1024, stdin);
            msg[strcspn(msg, "\n")] = 0;
            ptr->pre_split(stoi(msg));
        }
        else if(strcmp(msg, "insert inorder") == 0)
        {
            ptr->insert_in_order();
        }
        else if(strcmp(msg, "quit") == 0)
        {
            event_base_loopbreak(ptr->m_base);
        }
    }
    else
    {
        setbuf(stdin, NULL); //清空输入
        return;
    }
    setbuf(stdin, NULL);
}

client::client()
{
    m_base = event_base_new();
    //监听终端输入事件
    struct event *ev_cmd = event_new(m_base, STDIN_FILENO, EV_READ | EV_PERSIST, cmd_msg_cb, this);
    event_add(ev_cmd, NULL);

    event_base_dispatch(m_base);
    //释放资源
    event_base_free(m_base);
}

void *thread_task_function(void *arg)
{
    //参数准备
    inf *ptr = (inf *)arg;
    vector<int>arr;
    for(int i = 0;i<ptr->address->insert_ratio;i++) arr.push_back(0);
    for(int i = 0;i<ptr->address->update_ratio;i++) arr.push_back(1);
    for(int i = 0;i<ptr->address->delete_ratio;i++) arr.push_back(2);
    for(int i = 0;i<ptr->address->get_ratio;i++) arr.push_back(3);
    for(int i = 0;i<ptr->address->range_ratio;i++) arr.push_back(4);

    vector<int> op_array(ptr->address->operator_num_pre_thread);
    vector<uint64_t> key1_array(ptr->address->operator_num_pre_thread);
    vector<uint64_t> key2_array(ptr->address->operator_num_pre_thread);
    vector<uint32_t> value_array(ptr->address->operator_num_pre_thread);
    vector<vector<int>> result_array(ptr->address->operator_num_pre_thread);

    //初始化随机数据
    uniform_int_distribution<uint64_t> key_dis(ptr->address->min_key, ptr->address->max_key);
    uniform_int_distribution<uint32_t> value_dis(ptr->address->min_value, ptr->address->max_value);
    uniform_int_distribution<uint32_t> operator_dis(0, arr.size() - 1);
    uniform_int_distribution<unsigned int> range_leng_dis(1, ptr->address->range_len);

    default_random_engine value_seed(ptr->address->thread_id[ptr->id] + 2021);
    default_random_engine operator_seed(ptr->address->thread_id[ptr->id] + 1202);
    default_random_engine range_seed(ptr->address->thread_id[ptr->id] + 100);
    default_random_engine key_seed(ptr->address->thread_id[ptr->id] + 1000);

    for (int i = 0; i < ptr->address->operator_num_pre_thread; i++)
    {
        op_array[i] = arr[operator_dis(operator_seed)];
        if (op_array[i] == 0) //insert
        {
            op_array[i] = 0;
            key1_array[i] = key_dis(key_seed);
            value_array[i] = value_dis(value_seed);
            continue;
        }
        else if (op_array[i] == 1)
        {
            op_array[i] = 1;
            key1_array[i] = key_dis(key_seed);
            value_array[i] = value_dis(value_seed);
            continue;
        }
        else if (op_array[i] == 2)
        {
            op_array[i] = 2;
            key1_array[i] = key_dis(key_seed);
            continue;
        }
        else if (op_array[i] == 3)
        {
            op_array[i] = 3;
            key1_array[i] = key_dis(key_seed);
            continue;
        }
        else if (op_array[i] == 4)
        {
            op_array[i] = 4;
            key1_array[i] = key_dis(range_seed);
            key2_array[i] = range_leng_dis(range_seed) + key1_array[i];
        }
    }

    //开始执行
    timeval start, end;
    gettimeofday(&start, NULL);
    Concurr_LHash *k = Concurr_LHash::get_Lhash();
    //LHash* k = LHash::get_Lhash();

    for (int i = 0; i < ptr->address->operator_num_pre_thread; i++)
    {
        //cout<<"thread_id"<<ptr->id<<endl;
        switch (op_array[i])
        {
        case 0: //insert
            result_array[i].push_back(k->insert(key1_array[i], value_array[i]));
            ptr->address->insert_num[ptr->id]++;
            break;
        case 1: //update
            result_array[i].push_back(k->update(key1_array[i], value_array[i]));
            ptr->address->update_num[ptr->id]++;
            break;
        case 2: //delete
            result_array[i].push_back(k->remove(key1_array[i]));
            ptr->address->delete_num[ptr->id]++;
            break;
        case 3: //get
        {
            int32_t res = k->get(key1_array[i]);
            result_array[i].push_back(res);
            ptr->address->get_num[ptr->id]++;
            break;
        }
        case 4: //range
        {
            vector<uint32_t> res;
            k->range(key1_array[i], key2_array[i], res);
            ptr->address->range_num[ptr->id]++;
            for (auto x : res)
                result_array[i].push_back(x);
            break;
        }
        default:
            break;
        }
    }

    gettimeofday(&end, NULL);
    double cost = end.tv_sec - start.tv_sec + (end.tv_usec - start.tv_usec) / 1000000.0;
    //输出锁
    ptr->address->cout_loc.lock();
    cout << "线程：" << ptr->id << "：\n"
         << "运行时间： " << cost << " s\n";
    cout << "操作数量： " << ptr->address->operator_num_pre_thread << endl;
    cout << "QPS: " << ptr->address->operator_num_pre_thread / cost << endl;
    ptr->address->cout_loc.unlock();
    ptr->address->QPS[ptr->id] = ptr->address->operator_num_pre_thread / cost;

    if (ptr->address->log)
    {
        //导出数据
        ofstream out("result.txt");
        for (int i = 0; i < ptr->address->operator_num_pre_thread; i++)
        {
            if (op_array[i] == 4) //range
            {
                out << "<range> " << key1_array[i] << " [ " << key2_array[i] << " ]< ";
                for (auto x : result_array[i])
                    out << x << " ";
                out << "-1 >";
            }
            else if (op_array[i] == 0) //insert
            {
                out << "<insert> " << key1_array[i] << " [ " << value_array[i] << " ]< " << result_array[i][0] << " >";
            }
            else if (op_array[i] == 1) //update
            {
                out << "<update> " << key1_array[i] << " [ " << value_array[i] << " ]< " << result_array[i][0] << " >";
            }
            else if (op_array[i] == 2) //delete
            {
                out << "<delete> " << key1_array[i] << " < " << result_array[i][0] << " >";
            }
            else
            {
                out << "<get> " << key1_array[i] << " < " << result_array[i][0] << " >";
            }
            out << endl;
        }
    }

    ptr->address->finsh_thread++;
}

void* insert_order_thread(void*arg)
{
    //参数准备
    inf *ptr = (inf *)arg;
    vector<uint64_t> key1_array(ptr->address->operator_num_pre_thread);
    vector<uint32_t> value_array(ptr->address->operator_num_pre_thread);
    uniform_int_distribution<uint32_t> value_dis(ptr->address->min_value, ptr->address->max_value);
    default_random_engine value_seed(ptr->address->thread_id[ptr->id] + 2021);
    for (int i = 0; i < ptr->address->operator_num_pre_thread; i++)
    {
        key1_array[i] = ptr->id * ptr->address->operator_num_pre_thread + i;
        value_array[i] = value_dis(value_seed);
    }
    Concurr_LHash* k = Concurr_LHash::get_Lhash();
    //开始执行
    timeval start, end;
    gettimeofday(&start, NULL);
    for (int i = 0; i < ptr->address->operator_num_pre_thread; i++)
    {
        k->insert(key1_array[i], value_array[i]);
    }

    gettimeofday(&end, NULL);
    double cost = end.tv_sec - start.tv_sec + (end.tv_usec - start.tv_usec) / 1000000.0;
    //输出锁
    ptr->address->cout_loc.lock();
    cout << "线程：" << ptr->id << "：\n"
         << "运行时间： " << cost << " s\n";
    cout << "操作数量： " << ptr->address->operator_num_pre_thread << endl;
    cout << "QPS: " << ptr->address->operator_num_pre_thread / cost << endl;
    ptr->address->cout_loc.unlock();
    ptr->address->QPS[ptr->id] = ptr->address->operator_num_pre_thread / cost;

    ptr->address->finsh_thread++;
}

void client::start_run()
{
    //参数准备
    insert_num.resize(thread_task_num);
    update_num.resize(thread_task_num);
    delete_num.resize(thread_task_num);
    get_num.resize(thread_task_num);
    range_num.resize(thread_task_num);
    thread_id.resize(thread_task_num);
    QPS.resize(thread_task_num);
    fill(insert_num.begin(), insert_num.end(), 0);
    fill(update_num.begin(), update_num.end(), 0);
    fill(delete_num.begin(), delete_num.end(), 0);
    fill(get_num.begin(), get_num.end(), 0);
    fill(range_num.begin(), range_num.end(), 0);
    fill(thread_id.begin(), thread_id.end(), 0);
    finsh_thread = 0;

    //创建线程
    for (int i = 0; i < thread_task_num; i++)
    {
        inf *temp = new inf;
        temp->address = this;
        temp->id = i;
        pthread_create(&thread_id[i], nullptr, thread_task_function, temp); //创建新线程
    }

    //
    while (finsh_thread < thread_task_num)
        sleep(1);
    //计算操作数量总和
    int total_insert = 0;
    int total_update = 0;
    int total_delete = 0;
    int total_get = 0;
    int total_range = 0;
    int average_qps = 0;
    for (int i = 0; i < thread_task_num; i++)
    {
        total_insert += insert_num[i];
        total_update += update_num[i];
        total_delete += delete_num[i];
        total_get += get_num[i];
        total_range += range_num[i];
        average_qps += QPS[i];
    }
    cout << "操作数量统计：\ninsert: " << total_insert << "\nupdate: " << total_update << "\ndelete: " << total_delete << "\nget: " << total_get << "\nrange: " << total_range << endl
         << "Total QPS: " << average_qps << endl;
}

void client::test(int count)
{
    Concurr_LHash *k = Concurr_LHash::get_Lhash();
    //LHash* k = LHash::get_Lhash();
    clock_t start_time = clock();
    int insert_failed = 0;
    int search_failed = 0;
    int update_failed = 0;
    int range_failed = 0;
    int remove_failed = 0;
    cout << "start insert" << endl;
    for (uint64_t i = 1; i <= count; i++)
    {
        if (!k->insert(i, i))
            ++insert_failed;
    }
    cout << "insert failed: " << insert_failed << endl;
    cout << "K_V store res elment num: " << k->res_element_num() << endl;

    //k->disp();

    cout << "start search" << endl;
    for (uint64_t i = 1; i <= count; i++)
    {
        int32_t val;
        val = k->get(i);
        if (val == -1)
        {
            ++search_failed;
            //cout<<i<<endl;
        }
    }
    cout << "search failed: " << search_failed << endl;
    cout << "K_V store res elment num: " << k->res_element_num() << endl;

    cout << "start update" << endl;
    for (uint64_t i = 1; i <= count; i++)
    {
        if (!k->update(i, 2))
            ++update_failed;
    }
    cout << "update failed: " << update_failed << endl;
    cout << "K_V store res elment num: " << k->res_element_num() << endl;

    cout << "start range" << endl;
    for (uint64_t i = 1; i <= count; i++)
    {
        vector<uint32_t> res;
        k->range(i, i + 100, res);
        if (res.size() != min(101, count - (int)i + 1))
        {
            ++range_failed;
            //cout<<i<<endl;
        }
    }
    cout << "range failed: " << range_failed << endl;
    cout << "K_V store res elment num: " << k->res_element_num() << endl;

    cout << "start remove" << endl;
    for (uint64_t i = 1; i <= count; i++)
    {
        if (!k->remove(i))
            ++remove_failed;
    }
    cout << "remove failed: " << remove_failed << endl;

    cout << "finished" << endl;
    clock_t end_time = clock();
    cout << "serial time: " << (double)(end_time - start_time) / CLOCKS_PER_SEC << endl;
    cout << "K_V store res elment num: " << k->res_element_num() << endl;
}

void client::set_args(int th_num, int insert, int update, int del, int get, int range)
{
    thread_task_num = th_num;
    insert_ratio = insert;
    update_ratio = update;
    delete_ratio = del;
    get_ratio = get;
    range_ratio = range;
}

void client::set_range_len(int len)
{
    range_len = len;
}

void client::set_key_range(int min, int max)
{
    min_key = min;
    max_key = max;
}

void client::set_value_range(int min, int max)
{
    min_value = min;
    max_value = max;
}

void client::use_log()
{
    log = true;
}

void client::forbidden_log()
{
    log = false;
}

//加载日志文件并验证
void client::verify(string file_name)
{
    ifstream read_file(file_name);
    string operat;
    int total_num = 0;
    int fail_num = 0;
    string temp;

    uint64_t key1;
    uint64_t key2;
    uint32_t value;

    Concurr_LHash *k = Concurr_LHash::get_Lhash();

    while (read_file >> operat)
    {
        //cout<<total_num<<endl;
        total_num++;
        if (operat == "<insert>")
        {
            int res;
            read_file >> key1 >> temp >> value >> temp >> res >> temp;
            if (res != k->insert(key1, value))
            {
                fail_num++;
                //cout<< key1 << "  " << value<< "  "<< res <<endl;
            }      
        }
        else if (operat == "<update>")
        {
            int res;
            read_file >> key1 >> temp >> value >> temp >> res >> temp;
            if (res != k->update(key1, value))
            {
                fail_num++;
                //cout<< key1 << "  " << value<< "  "<< res <<endl;
            }
        }
        else if (operat == "<delete>")
        {
            int res;
            read_file >> key1 >> temp >> res >> temp;
            if (res != k->remove(key1))
            {
                fail_num++;
                //cout<< key1 <<  "  "<< res <<endl;
            }
        }
        else if (operat == "<get>")
        {
            int res;
            read_file >> key1 >> temp >> res >> temp;
            if (res != k->get(key1))
            {
                fail_num++;
                //cout<< key1 <<  "  "<< res <<endl;
            }
        }
        else if (operat == "<range>")
        {
            vector<uint32_t> res_old;
            vector<uint32_t> res_new;
            read_file >> key1 >> temp >> key2 >> temp;
            int32_t val;
            read_file >> val;
            while (val != -1)
            {
                res_old.push_back(val);
                read_file >> val;
            }
            read_file >> temp;

            k->range(key1, key2, res_new);
            if (res_new.size() != res_old.size())
            {
                fail_num++;
                continue;
            }
            for (int i = 0; i < res_new.size(); i++)
            {
                if (res_new[i] != res_old[i])
                {
                    fail_num++;
                    //cout<<"range"<<endl;
                    break;
                }
            }
        }
        else
        {
            total_num--;
            break;
        }
    }
    cout << "验证记录条数： " << total_num << endl;
    cout << "失败条数： " << fail_num << endl;
}

void client::disp_args()
{
    cout << "线程数：" << thread_task_num << endl;
    printf("每个线程总的操作次数：%d\n", operator_num_pre_thread);
    printf("操作比例： insert %d, update %d, delete %d, get %d, range %d\n", insert_ratio, update_ratio, delete_ratio, get_ratio, range_ratio);
    printf("value范围：%d  ----  %d\n", min_value, max_value);
    printf("key范围：%d  ----  %d\n", min_key, max_key);
    printf("range长度： %d\n", range_len);
    if (log)
        printf("log: True\n");
    else
        printf("log: False\n");
}

int client::set_operator_per_thread(int num)
{
    operator_num_pre_thread = num;
}

void client::pre_split(int count)
{
    Concurr_LHash::get_Lhash()->pre_split(count);
}

void client::insert_in_order()
{
    //参数准备
    insert_num.resize(thread_task_num);
    update_num.resize(thread_task_num);
    delete_num.resize(thread_task_num);
    get_num.resize(thread_task_num);
    range_num.resize(thread_task_num);
    thread_id.resize(thread_task_num);
    QPS.resize(thread_task_num);
    fill(insert_num.begin(), insert_num.end(), 0);
    fill(update_num.begin(), update_num.end(), 0);
    fill(delete_num.begin(), delete_num.end(), 0);
    fill(get_num.begin(), get_num.end(), 0);
    fill(range_num.begin(), range_num.end(), 0);
    fill(thread_id.begin(), thread_id.end(), 0);
    finsh_thread = 0;

    //创建线程
    for (int i = 0; i < thread_task_num; i++)
    {
        inf *temp = new inf;
        temp->address = this;
        temp->id = i;
        pthread_create(&thread_id[i], nullptr, insert_order_thread, temp); //创建新线程
    }

    //
    while (finsh_thread < thread_task_num)
        sleep(1);
    //计算操作数量总和
    int total_insert = 0;
    int total_update = 0;
    int total_delete = 0;
    int total_get = 0;
    int total_range = 0;
    int average_qps = 0;
    for (int i = 0; i < thread_task_num; i++)
    {
        total_insert += insert_num[i];
        total_update += update_num[i];
        total_delete += delete_num[i];
        total_get += get_num[i];
        total_range += range_num[i];
        average_qps += QPS[i];
    }
    printf("操作数量统计：\ninsert:%d\nQPS:%d\n", thread_task_num* operator_num_pre_thread, average_qps);
}

