#include <stdio.h>
#include <stdlib.h>
#include <semaphore.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>

//全局变量 指针
int in = 0;
int out = 0;

//同步量
pthread_mutex_t mutex;
sem_t full, empty;

//临界区共享资源
const int BUFFER_SIZE = 5;
int buffer[BUFFER_SIZE];

const int TIME_UNIT = 1000000;

typedef struct{
    int id;                 //线程id
    char type;              //读/写
    float delay_time;       //进入时间
    float duration_time;    //操作时间
}ThreadInfo;

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

// 获取当前时间的字符串表示
char* get_time_str() {
    static char buf[50];
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    strftime(buf, sizeof(buf), "%H:%M:%S", tm);
    return buf;
}

void* ProducerThread(void* arg){
    ThreadInfo* info = (ThreadInfo*)arg;
    
    //延时等待
    printf("[%s] Producer %d: Waiting for %.1f seconds before starting\n", 
           get_time_str(), info->id, info->delay_time);
    usleep(info->delay_time * TIME_UNIT);
    printf("[%s] Producer %d: Started\n", get_time_str(), info->id);

    //生产出产品
    int item = rand() % 100;

    printf("[%s] Producer %d: Waiting for empty slot\n", get_time_str(), info->id);
    sem_wait(&empty);
    printf("[%s] Producer %d: Trying to acquire buffer lock\n", get_time_str(), info->id);
    pthread_mutex_lock(&mutex);
    printf("[%s] Producer %d: Acquired buffer lock\n", get_time_str(), info->id);

    //延时放置
    usleep(info->duration_time * TIME_UNIT);
    buffer[in] = item;
    in = (in + 1) % BUFFER_SIZE;
    printf("[%s] Producer %d: Produced item %d, buffer count: %d\n", 
           get_time_str(),info->id, item, (in - out + BUFFER_SIZE) % BUFFER_SIZE);

    pthread_mutex_unlock(&mutex);
    printf("[%s] Producer %d: Released buffer lock\n", get_time_str(), info->id);
    sem_post(&full);    //给消费者发一个信号
    printf("[%s] Producer %d: Signaled full semaphore\n", get_time_str(), info->id);

    printf("Producer %d: Finished\n", info->id);
    return NULL;
}

void* ConsumerThread(void* arg){
    ThreadInfo* info = (ThreadInfo*)arg;

    //延时等待
    printf("[%s] Consumer %d: Waiting for %.1f seconds before starting\n", 
           get_time_str(), info->id, info->delay_time);
    usleep(info->delay_time * TIME_UNIT);
    printf("[%s] Consumer %d: Started\n", get_time_str(), info->id);

    printf("[%s] Consumer %d: Waiting for full slot\n", get_time_str(), info->id);
    sem_wait(&full);
    printf("[%s] Consumer %d: Trying to acquire buffer lock\n", get_time_str(), info->id);
    pthread_mutex_lock(&mutex);
    printf("[%s] Consumer %d: Acquired buffer lock\n", get_time_str(), info->id);

    //延时取出
    usleep(info->duration_time * TIME_UNIT);
    int item = buffer[out];
    out = (out + 1) % BUFFER_SIZE;
    printf("[%s] Consumer %d: Consumed item %d, buffer count: %d\n", 
           get_time_str(),info->id, item, (in - out + BUFFER_SIZE) % BUFFER_SIZE);

    pthread_mutex_unlock(&mutex);
    printf("[%s] Consumer %d: Released buffer lock\n", get_time_str(), info->id);
    sem_post(&empty);
    printf("[%s] Consumer %d: Signaled empty semaphore\n", get_time_str(), info->id);
    
    printf("Consumer %d: Finished\n", info->id);
    return NULL;
}

int main(int argc, char* argv[]){
    if(argc != 2){
        printf("Usage %s <input file>\n", argv[0]);
        return 1;
    }
    ThreadInfo* threads;
    int num_of_threads = read_threads_from_file(argv[1], &threads);
    if(num_of_threads <= 0){ return 1;}

    //初始化互斥锁与信号量
    pthread_mutex_init(&mutex, NULL);
    sem_init(&empty, 0, BUFFER_SIZE);
    sem_init(&full, 0, 0);

    printf("\n===== Starting %d threads =====\n", num_of_threads);
    //创建线程id
    pthread_t tid[num_of_threads]; 

    //根据类型创建线程（函数）
    for(int i = 0; i < num_of_threads; i++){
        if(threads[i].type == 'P')
            pthread_create(&tid[i], NULL, ProducerThread, &threads[i]);
        else
            pthread_create(&tid[i], NULL, ConsumerThread, &threads[i]);
    }

    //跑线程
    for(int i = 0; i < num_of_threads; i++){
        pthread_join(tid[i], NULL);
    }

    printf("\n===== All threads completed =====\n");

    //清理资源
    free(threads);
    pthread_mutex_destroy(&mutex);
    sem_destroy(&full);
    sem_destroy(&empty);
    return 0;
}

