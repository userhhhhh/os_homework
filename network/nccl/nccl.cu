// nccl_test.cu
#include <stdio.h>
#include <stdlib.h>
#include <cuda_runtime.h>
#include <nccl.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <pthread.h>
#include <math.h>

// 错误检查宏
#define CUDA_CHECK(cmd) do {                         \
  cudaError_t err = cmd;                             \
  if (err != cudaSuccess) {                          \
    printf("CUDA error %s:%d: '%s'\n",               \
        __FILE__, __LINE__, cudaGetErrorString(err));\
    exit(EXIT_FAILURE);                              \
  }                                                  \
} while(0)

#define NCCL_CHECK(cmd) do {                         \
  ncclResult_t res = cmd;                            \
  if (res != ncclSuccess) {                          \
    printf("NCCL error %s:%d: '%s'\n",               \
        __FILE__, __LINE__, ncclGetErrorString(res));\
    exit(EXIT_FAILURE);                              \
  }                                                  \
} while(0)

// 线程参数结构体
typedef struct {
    int dev;
    float* sendbuff;
    float* recvbuff;
    size_t count;
    ncclComm_t comm;
    int root;
    int op_type; // 0: broadcast, 1: allreduce
} thread_args_t;

// 广播函数实现
ncclResult_t nccl_broadcast_data(void* data, size_t count, int root, ncclComm_t comm) {
    cudaStream_t stream;
    CUDA_CHECK(cudaStreamCreate(&stream));
    ncclResult_t result = ncclBroadcast(data, data, count, ncclFloat, root, comm, stream);
    CUDA_CHECK(cudaStreamSynchronize(stream));
    CUDA_CHECK(cudaStreamDestroy(stream));
    return result;
}

// AllReduce函数实现
ncclResult_t nccl_allreduce_data(void* sendbuff, void* recvbuff, size_t count, ncclComm_t comm) {
    cudaStream_t stream;
    CUDA_CHECK(cudaStreamCreate(&stream));
    ncclResult_t result = ncclAllReduce(sendbuff, recvbuff, count, ncclFloat, ncclSum, comm, stream);
    CUDA_CHECK(cudaStreamSynchronize(stream));
    CUDA_CHECK(cudaStreamDestroy(stream));
    return result;
}

// 线程执行函数
void* nccl_thread_func(void* arg) {
    thread_args_t* args = (thread_args_t*)arg;
    CUDA_CHECK(cudaSetDevice(args->dev));
    if (args->op_type == 0) {
        nccl_broadcast_data(args->sendbuff, args->count, args->root, args->comm);
    } else {
        nccl_allreduce_data(args->sendbuff, args->recvbuff, args->count, args->comm);
    }
    return NULL;
}

// 获取当前时间（微秒）
double get_time_us() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000000 + tv.tv_usec;
}

int main(int argc, char* argv[]) {
    // 设置随机种子
    srand(time(NULL));
    
    // 获取GPU数量
    int nDev = 0;
    CUDA_CHECK(cudaGetDeviceCount(&nDev));
    printf("Found %d GPUs\n", nDev);
    
    // 如果不足两个GPU，则退出
    if (nDev < 2) {
        printf("需要至少2个GPU才能进行测试\n");
        return 0;
    }
    
    // 设置使用两个GPU
    nDev = 2;  // 只使用2个GPU进行测试
    
    // 为每个GPU分配ID
    int devs[2] = {0, 1};
    
    // 创建NCCL通信器
    ncclComm_t comms[2];
    NCCL_CHECK(ncclCommInitAll(comms, nDev, devs));
    printf("NCCL通信器初始化成功\n");
    
    // 数据大小（从1MB到256MB）
    size_t sizes[] = {
        1 << 20,      // 1 MB
        4 << 20,      // 4 MB
        16 << 20,     // 16 MB
        64 << 20,     // 64 MB
        256 << 20     // 256 MB
    };
    
    for (int s = 0; s < sizeof(sizes)/sizeof(sizes[0]); s++) {
        size_t size = sizes[s];
        size_t count = size / sizeof(float);
        
        printf("\n测试数据大小: %.2f MB\n", size / (1024.0 * 1024.0));
        
        // 为每个GPU分配内存和准备数据
        float** sendbuff = (float**)malloc(nDev * sizeof(float*));
        float** recvbuff = (float**)malloc(nDev * sizeof(float*));
        float** hostbuff = (float**)malloc(nDev * sizeof(float*));
        
        for (int i = 0; i < nDev; i++) {
            CUDA_CHECK(cudaSetDevice(i));
            CUDA_CHECK(cudaMalloc(&sendbuff[i], size));
            CUDA_CHECK(cudaMalloc(&recvbuff[i], size));
            hostbuff[i] = (float*)malloc(size);
            
            // 初始化数据：对于设备0填充随机数，其他设备填充0
            if (i == 0) {
                for (size_t j = 0; j < count; j++) {
                    hostbuff[i][j] = (float)rand() / RAND_MAX;
                }
            } else {
                memset(hostbuff[i], 0, size);
            }
            
            // 将主机数据拷贝到GPU
            CUDA_CHECK(cudaMemcpy(sendbuff[i], hostbuff[i], size, cudaMemcpyHostToDevice));
            CUDA_CHECK(cudaMemset(recvbuff[i], 0, size));
        }
        
        // -------------------------------------
        // 测试广播操作 (Broadcast)
        // -------------------------------------
        printf("测试广播操作 (Broadcast):\n");
        
        // 执行广播操作前同步所有设备
        for (int i = 0; i < nDev; i++) {
            CUDA_CHECK(cudaSetDevice(i));
            CUDA_CHECK(cudaDeviceSynchronize());
        }
        
        // 计时开始
        double start_time = get_time_us();
        
        // 执行NCCL广播操作 (根节点为0) - 使用多线程
        pthread_t threads[nDev];
        thread_args_t targs[nDev];
        
        for (int i = 0; i < nDev; i++) {
            targs[i].dev = i;
            targs[i].sendbuff = sendbuff[i];
            targs[i].recvbuff = NULL; // 广播不需要recvbuff
            targs[i].count = count;
            targs[i].comm = comms[i];
            targs[i].root = 0;
            targs[i].op_type = 0; // 0: broadcast
            pthread_create(&threads[i], NULL, nccl_thread_func, &targs[i]);
        }
        
        // 等待所有线程完成
        for (int i = 0; i < nDev; i++) {
            pthread_join(threads[i], NULL);
        }
        
        // 确保所有操作完成
        for (int i = 0; i < nDev; i++) {
            CUDA_CHECK(cudaSetDevice(i));
            CUDA_CHECK(cudaDeviceSynchronize());
        }
        
        // 计时结束
        double end_time = get_time_us();
        double elapsed = end_time - start_time;
        
        // 将结果拷贝回主机内存以验证
        for (int i = 0; i < nDev; i++) {
            CUDA_CHECK(cudaSetDevice(i));
            CUDA_CHECK(cudaMemcpy(hostbuff[i], sendbuff[i], size, cudaMemcpyDeviceToHost));
        }
        
        // 验证广播结果是否一致
        bool broadcast_correct = true;
        for (int i = 1; i < nDev; i++) {
            for (size_t j = 0; j < count; j++) {
                if (hostbuff[0][j] != hostbuff[i][j]) {
                    printf("广播验证失败：GPU %d 与 GPU 0 的数据不一致\n", i);
                    broadcast_correct = false;
                    break;
                }
            }
            if (!broadcast_correct) break;
        }
        
        if (broadcast_correct) {
            printf("广播验证成功：所有GPU的数据一致\n");
        }
        
        // 计算带宽和延迟
        double bandwidth = (size * (nDev-1)) / (elapsed / 1000000.0) / (1024.0 * 1024.0 * 1024.0);
        printf("广播性能：时间 = %.3f ms, 吞吐量 = %.3f GB/s\n", 
               elapsed / 1000.0, bandwidth);
        
        // -------------------------------------
        // 测试规约操作 (AllReduce)
        // -------------------------------------
        printf("\n测试规约操作 (AllReduce):\n");
        
        // 重新准备数据
        for (int i = 0; i < nDev; i++) {
            CUDA_CHECK(cudaSetDevice(i));
            
            // 为每个GPU初始化不同的数据
            for (size_t j = 0; j < count; j++) {
                hostbuff[i][j] = (float)(i + 1) / nDev; // 简单的不同值
            }
            
            CUDA_CHECK(cudaMemcpy(sendbuff[i], hostbuff[i], size, cudaMemcpyHostToDevice));
        }
        
        // 执行AllReduce操作前同步所有设备
        for (int i = 0; i < nDev; i++) {
            CUDA_CHECK(cudaSetDevice(i));
            CUDA_CHECK(cudaDeviceSynchronize());
        }
        
        // 计时开始
        start_time = get_time_us();
        
        // 执行NCCL AllReduce操作（使用多线程）
        for (int i = 0; i < nDev; i++) {
            targs[i].dev = i;
            targs[i].sendbuff = sendbuff[i];
            targs[i].recvbuff = recvbuff[i];
            targs[i].count = count;
            targs[i].comm = comms[i];
            targs[i].op_type = 1; // 1: allreduce
            pthread_create(&threads[i], NULL, nccl_thread_func, &targs[i]);
        }
        
        // 等待所有线程完成
        for (int i = 0; i < nDev; i++) {
            pthread_join(threads[i], NULL);
        }
        
        // 确保所有操作完成
        for (int i = 0; i < nDev; i++) {
            CUDA_CHECK(cudaSetDevice(i));
            CUDA_CHECK(cudaDeviceSynchronize());
        }
        
        // 计时结束
        end_time = get_time_us();
        elapsed = end_time - start_time;
        
        // 将结果拷贝回主机内存以验证
        for (int i = 0; i < nDev; i++) {
            CUDA_CHECK(cudaSetDevice(i));
            CUDA_CHECK(cudaMemcpy(hostbuff[i], recvbuff[i], size, cudaMemcpyDeviceToHost));
        }
        
        // 验证AllReduce结果是否正确 - 所有值应该是 (1+2)/2 = 1.5
        bool allreduce_correct = true;
        float expected_sum = 0;
        for (int i = 0; i < nDev; i++) {
            expected_sum += (float)(i + 1) / nDev;
        }
        
        for (int i = 0; i < nDev; i++) {
            for (size_t j = 0; j < count; j++) {
                if (fabs(hostbuff[i][j] - expected_sum) > 1e-5) {
                    printf("AllReduce验证失败：GPU %d 的结果与预期不符\n", i);
                    allreduce_correct = false;
                    break;
                }
            }
            if (!allreduce_correct) break;
        }
        
        if (allreduce_correct) {
            printf("AllReduce验证成功：所有GPU的结果等于预期的总和\n");
        }
        
        // 计算带宽和延迟
        bandwidth = (2.0 * size * nDev) / (elapsed / 1000000.0) / (1024.0 * 1024.0 * 1024.0);
        printf("AllReduce性能：时间 = %.3f ms, 吞吐量 = %.3f GB/s\n", 
               elapsed / 1000.0, bandwidth);
               
        // 释放资源
        for (int i = 0; i < nDev; i++) {
            CUDA_CHECK(cudaSetDevice(i));
            CUDA_CHECK(cudaFree(sendbuff[i]));
            CUDA_CHECK(cudaFree(recvbuff[i]));
            free(hostbuff[i]);
        }
        free(sendbuff);
        free(recvbuff);
        free(hostbuff);
    }
    
    // 销毁NCCL通信器
    for (int i = 0; i < nDev; i++) {
        ncclCommDestroy(comms[i]);
    }
    
    return 0;
}