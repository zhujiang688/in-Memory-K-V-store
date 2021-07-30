#ifndef __CLIENT
#define __CLIENT

//#include "liner_hash.h"
#include "Concurrent_liner_hash.h"

#include <pthread.h>
#include <vector>
#include <ctime>
#include <cstdlib>
#include <random>
#include <atomic>
#include <fstream>
#include <mutex>
#include <sys/time.h>
#include <unordered_map>
#include <unistd.h>
#include <iostream>
#include <stdint.h>
#include <event2/event.h>


using namespace std;

class client
{
private:
    int thread_task_num = 0; //开启的线程数量
    vector<int>insert_num;  //每个线程的插入操作的数量
    vector<int>update_num; //每个线程的更新操作的数量
    vector<int>delete_num;//每个线程的删除操作的数量
    vector<int>get_num;//每个线程的查询操作的数量
    vector<int>range_num;//每个线程的范围查询操作的数量
    //记录各个线程的QPS
    vector<int>QPS;//每个线程的QPS
    uint32_t min_value = 1;  //value的最小值
    uint32_t max_value = 100000000; //value的最大值
    uint64_t min_key = 1; //key的最小值
    uint64_t max_key = 20000000; //key的最大值
    int range_len = 100; //范围查询的最大范围长度
    bool log = false; //是否打印日志
    int operator_num_pre_thread = 20000000; //每个线程总的操作次数
    vector<pthread_t>thread_id;//每个线程的线程id
    int insert_ratio = 0; //insert操作的比例
    int update_ratio = 0;//update操作的比例
    int delete_ratio = 0;//delete操作的比例
    int get_ratio = 0;//get操作的比例
    int range_ratio = 0;//range操作的比例
    event_base *m_base = nullptr; //event_base，用于监控键盘输入
    atomic<int> finsh_thread;//运行结束线程的数量
    mutex cout_loc;//线程之间的输出锁，避免输出乱码


    friend void *thread_task_function(void *);//随机key, 随机value, 任意操作比例的线程运行函数
    friend void *insert_order_thread(void*);//有序插入的线程函数
    friend void cmd_msg_cb(int fd, short events, void *arg);//键盘事件的回调函数
public:
    client();
    //参数设置接口
    void set_args(int th_num, int insert, int update, int del, int get, int range);
    void set_range_len(int len);
    void set_key_range(int min, int max);
    void set_value_range(int min, int max);
    int set_operator_per_thread(int num);
    void use_log();
    void forbidden_log();
    void disp_args();
    
    void pre_split(int count);//提前完成哈希桶的分裂
    void insert_in_order();//顺序插入
    void start_run();//根据设置的参数，完成任务
    void test(int count);//单线程正确性测试
    void verify(string file_name);//单线程读入log文件，重新重复操作并对比结果，即重做式正确性验证
};

struct inf
{
    client *address;
    int id;
};

#endif