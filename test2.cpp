#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>

#define BUFFER_SIZE 5  // 缓冲区大小

// 全局变量
int buffer[BUFFER_SIZE];  // 缓冲区
int in = 0;  // 生产者放入产品的位置
int out = 0;  // 消费者取出产品的位置

// 同步对象
pthread_mutex_t mutex;  // 互斥锁，保护对缓冲区的访问
sem_t empty;   // 空缓冲区信号量
sem_t full;    // 满缓冲区信号量

// 线程信息结构体
typedef struct {
    int id;
    char type;   // 'P' for producer, 'C' for consumer
    float delay_time;   // 延迟时间(秒)
    float duration_time; // 操作持续时间(秒)
} ThreadInfo;

// 获取当前时间的字符串表示
char* get_time_str() {
    static char buf[50];
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    strftime(buf, sizeof(buf), "%H:%M:%S", tm);
    return buf;
}

// 生产者线程函数
void* ProducerThread(void* arg) {
    ThreadInfo* info = (ThreadInfo*)arg;
    
    // 初始延迟
    printf("[%s] Producer %d: Waiting for %.1f seconds before starting\n", 
           get_time_str(), info->id, info->delay_time);
    usleep(info->delay_time * 1000000);
    
    printf("[%s] Producer %d: Started\n", get_time_str(), info->id);
    
    // 生产一个产品
    int item = rand() % 100;  // 随机生成一个产品
    
    // 生产者算法
    printf("[%s] Producer %d: Waiting for empty slot\n", get_time_str(), info->id);
    sem_wait(&empty);  // P(empty)
    printf("[%s] Producer %d: Trying to acquire buffer lock\n", get_time_str(), info->id);
    pthread_mutex_lock(&mutex);  // P(mutex)
    printf("[%s] Producer %d: Acquired buffer lock\n", get_time_str(), info->id);
    
    // 将产品放入缓冲区
    usleep(info->duration_time * 1000000);
    buffer[in] = item;
    in = (in + 1) % BUFFER_SIZE;
    printf("[%s] Producer %d: Produced item %d, buffer count: %d\n", 
           get_time_str(),info->id, item, (in - out + BUFFER_SIZE) % BUFFER_SIZE);
    
    pthread_mutex_unlock(&mutex);      // V(mutex)
    printf("[%s] Producer %d: Released buffer lock\n", get_time_str(), info->id);
    sem_post(&full);  // V(full)
    printf("[%s] Producer %d: Signaled full semaphore\n", get_time_str(), info->id);
    
    printf("Producer %d: Finished\n", info->id);
    return NULL;
}

// 消费者线程函数
void* ConsumerThread(void* arg) {
    ThreadInfo* info = (ThreadInfo*)arg;
    
   //延迟等待
    printf("[%s] Consumer %d: Waiting for %.1f seconds before starting\n", 
           get_time_str(), info->id, info->delay_time);
    usleep(info->delay_time * 1000000);
    
    printf("[%s] Consumer %d: Started\n", get_time_str(), info->id);
    
    // 消费者算法
    printf("[%s] Consumer %d: Waiting for full slot\n", get_time_str(), info->id);
    sem_wait(&full);   // P(full)
    printf("[%s] Consumer %d: Trying to acquire buffer lock\n", get_time_str(), info->id);
    pthread_mutex_lock(&mutex);  // P(mutex)
    printf("[%s] Consumer %d: Acquired buffer lock\n", get_time_str(), info->id);
    
    // 从缓冲区取出产品
    usleep(info->duration_time * 1000000);
    int item = buffer[out];
    out = (out + 1) % BUFFER_SIZE;
    printf("[%s] Consumer %d: Consumed item %d, buffer count: %d\n", 
           get_time_str(),info->id, item, (in - out + BUFFER_SIZE) % BUFFER_SIZE);
    
    pthread_mutex_unlock(&mutex);      // V(mutex)
    printf("[%s] Consumer %d: Released buffer lock\n", get_time_str(), info->id);
    sem_post(&empty);  // V(empty)
    printf("[%s] Consumer %d: Signaled empty semaphore\n", get_time_str(), info->id);
    
    printf("Consumer %d: Finished\n", info->id);
    return NULL;
}

int read_threads_from_file(const char* file_name, ThreadInfo** threads){
    FILE* file = fopen(file_name, "r");
    if(!file){
        printf("file %s cannot open\n", file_name);
        return 1;
    }
    int count = 0;
    char line[100];

    while(fgets(line, sizeof(line), file)){
        count++;
    }
    rewind(file);
    *threads = (ThreadInfo*)malloc(count * sizeof(ThreadInfo));
    int i = 0;
    while(fgets(line, sizeof(line), file)){
        sscanf(line, "%d %c %f %f",
            &(*threads)[i].id,
            &(*threads)[i].type,
            &(*threads)[i].delay_time,
            &(*threads)[i].duration_time
        );
        i++;
    }

    fclose(file);
    return count;
}

int main(int argc, char* argv[]) {
    if(argc != 2){
        printf("Usage %s <input file>\n", argv[0]);
        return 1;
    }

    // 初始化同步对象
    pthread_mutex_init(&mutex, NULL);
    sem_init(&empty, 0, BUFFER_SIZE);  // 初始值为BUFFER_SIZE
    sem_init(&full, 0, 0);             // 初始值为0
    
    ThreadInfo* threads;
    int num_of_threads = read_threads_from_file(argv[1], &threads);

    if(num_of_threads <= 0){ return 1;}

    //创建线程id
    pthread_t tid[num_of_threads]; 
    
    // 创建线程
    for (int i = 0; i < num_of_threads; i++) {
        if (threads[i].type == 'P') {
            pthread_create(&tid[i], NULL, ProducerThread, &threads[i]);
        } else {
            pthread_create(&tid[i], NULL, ConsumerThread, &threads[i]);
        }
    }
    
    // 等待所有线程完成
    for (int i = 0; i < num_of_threads; i++) {
        pthread_join(tid[i], NULL);
    }
    
    // 清理资源
    free(threads);
    
    pthread_mutex_destroy(&mutex);
    sem_destroy(&empty);
    sem_destroy(&full);
    
    printf("All threads completed. Program finished.\n");
    return 0;
}
