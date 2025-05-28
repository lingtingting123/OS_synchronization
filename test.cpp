#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <time.h>
#include <string.h>

// 全局变量
int rcount = 0;  // 当前读者数量
int wcount = 0;  // 当前写者数量

// 同步对象
pthread_mutex_t rmutex, wmutex, fmutex;
sem_t queue;

typedef struct {
    int id;
    char type;    // 'R'或'W'
    float delay;  // 秒
    float duration; // 秒
} ThreadInfo;

// 获取当前时间的字符串表示
char* get_time_str() {
    static char buf[50];
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    strftime(buf, sizeof(buf), "%H:%M:%S", tm);
    return buf;
}

void* ReaderThread(void* arg) {
    ThreadInfo* info = (ThreadInfo*)arg;
    
    // 模拟延迟
    printf("[%s] Reader %d: Waiting for %.1f seconds\n", 
           get_time_str(), info->id, info->delay);
    usleep(info->delay * 1000000);
    
    // 进入排队
    printf("[%s] Reader %d: Trying to enter queue\n", get_time_str(), info->id);
    sem_wait(&queue);
    printf("[%s] Reader %d: Entered queue\n", get_time_str(), info->id);
    
    // 读者计数互斥
    pthread_mutex_lock(&rmutex);
    if (rcount == 0) {
        printf("[%s] Reader %d: First reader, acquiring fmutex\n", get_time_str(), info->id);
        pthread_mutex_lock(&fmutex);
    }
    rcount++;
    printf("[%s] Reader %d: Total readers now: %d\n", get_time_str(), info->id, rcount);
    pthread_mutex_unlock(&rmutex);
    
    // 释放排队信号量
    sem_post(&queue);
    printf("[%s] Reader %d: Released queue\n", get_time_str(), info->id);
    
    // 读操作
    printf("[%s] Reader %d: STARTED reading (will take %.1f seconds)\n", 
           get_time_str(), info->id, info->duration);
    usleep(info->duration * 1000000);
    printf("[%s] Reader %d: FINISHED reading\n", get_time_str(), info->id);
    
    // 读者计数互斥
    pthread_mutex_lock(&rmutex);
    rcount--;
    printf("[%s] Reader %d: Left reading, readers now: %d\n", get_time_str(), info->id, rcount);
    if (rcount == 0) {
        printf("[%s] Reader %d: Last reader, releasing fmutex\n", get_time_str(), info->id);
        pthread_mutex_unlock(&fmutex);
    }
    pthread_mutex_unlock(&rmutex);
    
    return NULL;
}

void* WriterThread(void* arg) {
    ThreadInfo* info = (ThreadInfo*)arg;
    
    // 模拟延迟
    printf("[%s] Writer %d: Waiting for %.1f seconds\n", 
           get_time_str(), info->id, info->delay);
    usleep(info->delay * 1000000);
    
    // 写者计数互斥
    pthread_mutex_lock(&wmutex);
    if (wcount == 0) {
        printf("[%s] Writer %d: First writer, acquiring queue\n", get_time_str(), info->id);
        sem_wait(&queue); // 第一个写者锁队列
    }
    wcount++;
    printf("[%s] Writer %d: Total writers now: %d\n", get_time_str(), info->id, wcount);
    pthread_mutex_unlock(&wmutex);
    
    // 获取读写锁
    printf("[%s] Writer %d: Trying to acquire fmutex\n", get_time_str(), info->id);
    pthread_mutex_lock(&fmutex);
    printf("[%s] Writer %d: Acquired fmutex\n", get_time_str(), info->id);
    
    // 写操作
    printf("[%s] Writer %d: STARTED writing (will take %.1f seconds)\n", 
           get_time_str(), info->id, info->duration);
    usleep(info->duration * 1000000);
    printf("[%s] Writer %d: FINISHED writing\n", get_time_str(), info->id);
    
    // 释放读写锁
    pthread_mutex_unlock(&fmutex);
    printf("[%s] Writer %d: Released fmutex\n", get_time_str(), info->id);
    
    // 写者计数互斥
    pthread_mutex_lock(&wmutex);
    wcount--;
    printf("[%s] Writer %d: Left writing, writers now: %d\n", get_time_str(), info->id, wcount);
    if (wcount == 0) {
        printf("[%s] Writer %d: Last writer, releasing queue\n", get_time_str(), info->id);
        sem_post(&queue); // 最后一个写者释放队列
    }
    pthread_mutex_unlock(&wmutex);
    
    return NULL;
}

// 从文件读取线程信息
int read_thread_info(const char* filename, ThreadInfo** threads) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        perror("Error opening file");
        return -1;
    }
    
    int count = 0;
    char line[100];
    while (fgets(line, sizeof(line), file)) {
        count++;
    }
    
    *threads = (ThreadInfo*)malloc(count * sizeof(ThreadInfo));
    rewind(file);
    
    int i = 0;
    while (fgets(line, sizeof(line), file)) {
        sscanf(line, "%d %c %f %f", 
               &(*threads)[i].id, 
               &(*threads)[i].type, 
               &(*threads)[i].delay, 
               &(*threads)[i].duration);
        i++;
    }
    
    fclose(file);
    return count;
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        printf("Usage: %s <input_file>\n", argv[0]);
        return 1;
    }
    
    // 初始化同步对象
    pthread_mutex_init(&rmutex, NULL);
    pthread_mutex_init(&wmutex, NULL);
    pthread_mutex_init(&fmutex, NULL);
    sem_init(&queue, 0, 1);

    // 从文件读取线程信息
    ThreadInfo* threads;
    int n_threads = read_thread_info(argv[1], &threads);
    if (n_threads <= 0) {
        return 1;
    }
    
    printf("\n===== Starting %d threads =====\n", n_threads);
    pthread_t tid[n_threads];
    
    // 创建线程
    for (int i = 0; i < n_threads; i++) {
        if (threads[i].type == 'R') {
            pthread_create(&tid[i], NULL, ReaderThread, &threads[i]);
        } else {
            pthread_create(&tid[i], NULL, WriterThread, &threads[i]);
        }
    }
    
    // 等待所有线程
    for (int i = 0; i < n_threads; i++) {
        pthread_join(tid[i], NULL);
    }
    
    printf("\n===== All threads completed =====\n");
    
    // 清理资源
    free(threads);
    pthread_mutex_destroy(&rmutex);
    pthread_mutex_destroy(&wmutex);
    pthread_mutex_destroy(&fmutex);
    sem_destroy(&queue);
    
    return 0;
}
