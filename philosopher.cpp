#include <stdio.h>
#include <stdlib.h>
#include <semaphore.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>

//全局变量
const int NUM_OF_PHILOSOPHERS = 5;

sem_t count;

//资源保护
pthread_mutex_t forks[NUM_OF_PHILOSOPHERS];

typedef struct{
    int id;
    int thinking_time;
    int eating_time;
}ThreadInfo;

// 获取当前时间的字符串表示
char* get_time_str() {
    static char buf[50];
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    strftime(buf, sizeof(buf), "%H:%M:%S", tm);
    return buf;
}

void read_threads_from_file(const char* file_name, ThreadInfo** philosophers){
    FILE* file = fopen(file_name, "r");
    if(!file){
        printf("file %s cannot open\n", file_name);
        return;
    }
    char line[100];

    *philosophers = (ThreadInfo*)malloc(NUM_OF_PHILOSOPHERS * sizeof(ThreadInfo));
    int i = 0;
    while(fgets(line, sizeof(line), file)){
        sscanf(line, "%d %d %d",
            &(*philosophers)[i].id,
            &(*philosophers)[i].thinking_time,
            &(*philosophers)[i].eating_time
        );
        i++;
    }

    fclose(file);
}

void* PhilosopherThread(void* arg){
    ThreadInfo* p = (ThreadInfo*)arg;
    int id = p->id;

    //思考
    printf("[%s] Philosopher %d started thinking...\n", get_time_str(), id);
    sleep(p->thinking_time);

    //尝试进入进餐区
    printf("[%s] Philosopher %d finished thinking, waiting for dining queue...\n", get_time_str(), id);
    sem_wait(&count);
    printf("[%s] Philosopher %d Acquired dining queue...\n", get_time_str(), id);

    //等待左叉子
    printf("[%s] Philosopher %d waiting for left fork...\n", get_time_str(), id);
    pthread_mutex_lock(&forks[id % NUM_OF_PHILOSOPHERS]);
    printf("[%s] Philosopher %d Acquired left fork %d\n", get_time_str(), id, id % NUM_OF_PHILOSOPHERS);

    //等待右叉子
    printf("[%s] Philosopher %d waiting for right fork...\n", get_time_str(), id);
    pthread_mutex_lock(&forks[(id + 1) % NUM_OF_PHILOSOPHERS]);
    printf("[%s] Philosopher %d Acquired right fork %d\n", get_time_str(), id, (id + 1) % NUM_OF_PHILOSOPHERS);

    printf("[%s] Philosopher %d starting eating for %d seconds...\n", get_time_str(), id, p->eating_time);
    sleep(p->eating_time);

    pthread_mutex_unlock(&forks[id % NUM_OF_PHILOSOPHERS]);
    // printf("[%s] Philosopher %d put down left fork %d\n", get_time_str(), id, id);
    pthread_mutex_unlock(&forks[(id + 1) % NUM_OF_PHILOSOPHERS]);
    // printf("[%s] Philosopher %d put down right fork %d\n", get_time_str(), id, (id + 1) % NUM_OF_PHILOSOPHERS);

    printf("[%s] Philosopher %d finished eating...\n", get_time_str(), id);
    
    sem_post(&count);

    return NULL;
}

int main(int argc, char* argv[]){
    if(argc != 2){
        printf("Usage %s <input file>\n",argv[0]);
        return 1;
    }
    ThreadInfo* philosophers;
    read_threads_from_file(argv[1], &philosophers);

    printf("\n===== Starting %d threads =====\n", NUM_OF_PHILOSOPHERS);

    //初始化信号值
    sem_init(&count, 0, 4);
    for(int i = 0; i < NUM_OF_PHILOSOPHERS; i++)
        pthread_mutex_init(&forks[i], NULL); //所有叉子可用

    //创建线程id
    pthread_t tid[NUM_OF_PHILOSOPHERS]; 

    //创建哲学家线程（函数）
    for(int i = 0; i < NUM_OF_PHILOSOPHERS; i++)
        pthread_create(&tid[i], NULL, PhilosopherThread, &philosophers[i]);

    //跑线程
    for(int i = 0; i < NUM_OF_PHILOSOPHERS; i++)
        pthread_join(tid[i], NULL);

    printf("\n===== All threads completed =====\n");

    //销毁资源
    free(philosophers);
    sem_destroy(&count);
    for(int i = 0; i < NUM_OF_PHILOSOPHERS; i++)
        pthread_mutex_destroy(&forks[i]);

    return 0;
}