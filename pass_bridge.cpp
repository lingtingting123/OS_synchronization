#include <stdio.h>
#include <stdlib.h>
#include <semaphore.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>

sem_t way_on_bridge;
sem_t south_gate, north_gate;

const int TIME_PASS_GATE = 2;

typedef struct{
    int id;
    char type;
    float arrive_time;
    float pass_time;
}ThreadInfo;

// 获取信号量的当前值（跨平台安全实现）
int get_sem_value(sem_t* sem) {
    int val;
    if (sem_getvalue(sem, &val) != 0) {
        perror("sem_getvalue failed");
        return -1; // 错误时返回-1
    }
    return val;
}

// 获取当前时间的字符串表示
char* get_time_str() {
    static char buf[50];
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    strftime(buf, sizeof(buf), "%H:%M:%S", tm);
    return buf;
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
            &(*threads)[i].arrive_time,
            &(*threads)[i].pass_time
        );
        i++;
    }

    fclose(file);
    return count;
}

void* SouthPerson(void* arg){
    ThreadInfo* info = (ThreadInfo*)arg;
    //延迟到达
    usleep(info->arrive_time * 1000000);
    printf("[%s] 南行人%d 到达南端（等待进入）\n", get_time_str(), info->id);
    sem_wait(&way_on_bridge);
    printf("[%s] 南行人%d 获得桥访问权（剩余容量：%d）\n", 
           get_time_str(), info->id, get_sem_value(&way_on_bridge));
    //如果此时桥上有空位
    sem_wait(&south_gate);
    printf("[%s] 南行人%d 开始进入南门\n", get_time_str(), info->id);
    sleep(TIME_PASS_GATE);
    sem_post(&south_gate);
    printf("[%s] 南行人%d 通过南门，开始过桥\n", get_time_str(), info->id);
    sleep(info->pass_time);
    printf("[%s] 南行人%d 结束过桥，等待北门开放\n", get_time_str(), info->id);
    sem_wait(&north_gate);
    printf("[%s] 南行人%d 开始进入北门\n", get_time_str(), info->id);
    sleep(TIME_PASS_GATE);
    sem_post(&north_gate);
    sem_post(&way_on_bridge);
    printf("[%s] 南行人%d 离开北门，离开大桥\n", get_time_str(), info->id);

    return NULL;
}

void* NorthPerson(void* arg){
    ThreadInfo* info = (ThreadInfo*)arg;
    //延迟到达
    usleep(info->arrive_time * 1000000);

    printf("[%s] 北行人%d 到达北端（等待进入）\n", get_time_str(), info->id);
    sem_wait(&way_on_bridge);
    printf("[%s] 北行人%d 获得桥访问权（剩余容量：%d）\n", 
           get_time_str(), info->id, get_sem_value(&way_on_bridge));
    //如果此时桥上有空位
    sem_wait(&north_gate);
    printf("[%s] 北行人%d 开始进入北门\n", get_time_str(), info->id);
    sleep(TIME_PASS_GATE);
    sem_post(&north_gate);
    printf("[%s] 北行人%d 通过北门，开始过桥\n", get_time_str(), info->id);
    sleep(info->pass_time);
    printf("[%s] 北行人%d 结束过桥，等待南门开放\n", get_time_str(), info->id);
    sem_wait(&south_gate);
    printf("[%s] 北行人%d 开始进入南门\n", get_time_str(), info->id);
    sleep(TIME_PASS_GATE);
    sem_post(&south_gate);
    sem_post(&way_on_bridge);
    printf("[%s] 北行人%d 离开南门，离开大桥\n", get_time_str(), info->id);

    return NULL;
}

int main(int argc, char* argv[]){
    if(argc != 2){
        printf("Usage %s <input file>\n", argv[0]);
        return 1;
    }
    ThreadInfo* passerby;
    int num_of_passer = read_threads_from_file(argv[1], &passerby);

    for(int i = 0; i < num_of_passer; i++)
        printf("id:%d type:%c arrive:%f pass:%f\n",passerby[i].id,passerby[i].type,passerby[i].arrive_time,passerby[i].pass_time);

    printf("\n===== Starting %d threads =====\n", num_of_passer);

    //初始化变量
    sem_init(&way_on_bridge, 0, 2);
    sem_init(&south_gate, 0, 1);
    sem_init(&north_gate, 0, 1);

    //创建线程id
    pthread_t tid[num_of_passer]; 

    //根据类型创建线程（函数）
    for(int i = 0; i < num_of_passer; i++){
        if(passerby[i].type == 'S')
            pthread_create(&tid[i], NULL, SouthPerson, &passerby[i]);
        else
            pthread_create(&tid[i], NULL, NorthPerson, &passerby[i]);
    }

    //跑线程
    for(int i = 0; i < num_of_passer; i++){
        pthread_join(tid[i], NULL);
    }

    printf("\n===== All threads completed =====\n");

    //清理资源
    free(passerby);
    sem_destroy(&way_on_bridge);
    sem_destroy(&north_gate);
    sem_destroy(&south_gate);

    return 0;
}