#include <stdio.h>
#include <stdlib.h>
#include <semaphore.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>

//全局变量
int rcount = 0;
int wcount = 0;

//同步量
pthread_mutex_t fmutex, rmutex, wmutex;
sem_t queue;

//临界区共享资源
int shared_data = 0;

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

void* ReaderThread(void* arg){
    ThreadInfo* info = (ThreadInfo*)arg;
    //延迟等待
    printf("[%s] Reader %d: Waiting for %.1f seconds\n", 
           get_time_str(), info->id, info->delay_time);
    usleep(info->delay_time * 1000000);

    //看大门有没有锁
    printf("[%s] Reader %d: Trying to enter queue\n", get_time_str(), info->id);
    sem_wait(&queue);
    printf("[%s] Reader %d: Entered queue\n", get_time_str(), info->id);

    //如果进入大门，证明此时里面没有写者，开始读操作
    //保护rcount临界资源
    pthread_mutex_lock(&rmutex);
    if(rcount == 0){
        //如果是第一个读者，则拿起资源，防止后进入的写者进行写操作
        printf("[%s] Reader %d: First reader, acquiring fmutex\n", get_time_str(), info->id);
        pthread_mutex_lock(&fmutex);
    }
    //进入后读者增1
    rcount++;
    printf("[%s] Reader %d: Total readers now: %d\n", get_time_str(), info->id, rcount);
    pthread_mutex_unlock(&rmutex);

    //开大门
    sem_post(&queue);
    printf("[%s] Reader %d: Released queue\n", get_time_str(), info->id);

    //读操作
    printf("[%s] Reader %d: STARTED reading (will take %.1f seconds)\n", 
           get_time_str(), info->id, info->duration_time);
    usleep(info->duration_time * 1000000);
    printf("[%s] Reader %d: FINISHED reading\n", get_time_str(), info->id);

    //离开，rcount减1
    pthread_mutex_lock(&rmutex);
    rcount--;
    printf("[%s] Reader %d: Left reading, readers now: %d\n", get_time_str(), info->id, rcount);
    if(rcount == 0){
        //如果所有的读者都离开，则允许写者获取资源
        printf("[%s] Reader %d: Last reader, releasing fmutex\n", get_time_str(), info->id);
        pthread_mutex_unlock(&fmutex);
    }
    pthread_mutex_unlock(&rmutex);
    return NULL;
}

void* WriterThread(void* arg){
    ThreadInfo* info = (ThreadInfo*)arg;
    //延迟等待
    printf("[%s] Writer %d: Waiting for %.1f seconds\n",  
           get_time_str(), info->id, info->delay_time);
    usleep(info->delay_time * 1000000);

    //看看大门能不能进去, 如果里面有写者，则也可以进去
    pthread_mutex_lock(&wmutex);
    if(wcount == 0){
        //第一个写者进入，锁（读者的）大门
        printf("[%s] Writer %d: First writer, acquiring queue\n", get_time_str(), info->id);
        sem_wait(&queue);
    }
        //对于写者来说，到来即只需要等资源的锁
    wcount++;
    printf("[%s] Writer %d: Total writers now: %d\n", get_time_str(), info->id, wcount);
    pthread_mutex_unlock(&wmutex);

    printf("[%s] Writer %d: Trying to acquire fmutex\n", get_time_str(), info->id);
    pthread_mutex_lock(&fmutex);
    printf("[%s] Writer %d: Acquired fmutex\n", get_time_str(), info->id);

    //开始写操作
    shared_data++;

    //写延迟
    printf("[%s] Writer %d: STARTED writing (will take %.1f seconds)\n", 
           get_time_str(), info->id, info->duration_time);
    usleep(info->duration_time * 1000000);
    printf("[%s] Writer %d: FINISHED writing\n", get_time_str(), info->id);

    //写结束，释放资源
    pthread_mutex_unlock(&fmutex);
    printf("[%s] Writer %d: Released fmutex\n", get_time_str(), info->id);

    //如果当前所有的写者都写完了（也就是该线程是最后一个写者要离开），开读者的大门
    pthread_mutex_lock(&wmutex);
    wcount--;
    printf("[%s] Writer %d: Left writing, writers now: %d\n", get_time_str(), info->id, wcount);
    if(wcount == 0){
        printf("[%s] Writer %d: Last writer, releasing queue\n", get_time_str(), info->id);
        sem_post(&queue);
    }
    pthread_mutex_unlock(&wmutex);
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
    pthread_mutex_init(&fmutex, NULL);
    pthread_mutex_init(&rmutex, NULL);
    pthread_mutex_init(&wmutex, NULL);
    sem_init(&queue, 0, 1);     //arg2：选择线程（还是进程）共享；arg3：初始值

    printf("\n===== Starting %d threads =====\n", num_of_threads);
    //创建线程id
    pthread_t tid[num_of_threads]; 

    //根据类型创建线程（函数）
    for(int i = 0; i < num_of_threads; i++){
        if(threads[i].type == 'R')
            pthread_create(&tid[i], NULL, ReaderThread, &threads[i]);
        else
            pthread_create(&tid[i], NULL, WriterThread, &threads[i]);
    }

    //跑线程
    for(int i = 0; i < num_of_threads; i++){
        pthread_join(tid[i], NULL);
    }

    printf("\n===== All threads completed =====\n");

    // for(int i = 0; i < num_of_threads; i++){
    //     printf("thread[%d] %c %f %f\n", threads[i].id, threads[i].type, threads[i].delay_time, threads[i].duration_time);
    // }

    //清理资源
    free(threads);
    pthread_mutex_destroy(&fmutex);
    pthread_mutex_destroy(&rmutex);
    pthread_mutex_destroy(&wmutex);
    sem_destroy(&queue);
    return 0;
}

