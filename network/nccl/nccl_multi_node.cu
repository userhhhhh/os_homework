// nccl_multi_node.cu
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <cuda_runtime.h>
#include <nccl.h>
#include <mpi.h>
#include <sys/time.h>

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

#define MPI_CHECK(cmd) do {                          \
  int err = cmd;                                     \
  if (err != MPI_SUCCESS) {                          \
    char errstr[MPI_MAX_ERROR_STRING];               \
    int len;                                         \
    MPI_Error_string(err, errstr, &len);             \
    printf("MPI error %s:%d: '%s'\n",                \
           __FILE__, __LINE__, errstr);              \
    MPI_Abort(MPI_COMM_WORLD, 1);                    \
    exit(EXIT_FAILURE);                              \
  }                                                  \
} while(0)

// 获取当前时间（微秒）
double get_time_us() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000000 + tv.tv_usec;
}

int main(int argc, char* argv[]) {
    // 初始化MPI
    MPI_CHECK(MPI_Init(&argc, &argv));
    
    // 获取MPI进程信息
    int rank, world_size;
    MPI_CHECK(MPI_Comm_rank(MPI_COMM_WORLD, &rank));
    MPI_CHECK(MPI_Comm_size(MPI_COMM_WORLD, &world_size));
    
    // 打印节点信息
    char hostname[1024];
    gethostname(hostname, 1024);
    printf("Rank %d 运行在 %s\n", rank, hostname);
    
    // 获取可用的GPU数量
    int deviceCount;
    CUDA_CHECK(cudaGetDeviceCount(&deviceCount));
    
    // 确保进程数不超过GPU数量
    if (world_size > deviceCount) {
        if (rank == 0) {
            printf("错误：进程数(%d)大于GPU数量(%d)\n", world_size, deviceCount);
        }
        MPI_Abort(MPI_COMM_WORLD, 1);
        exit(EXIT_FAILURE);
    }
    
    // 每个进程使用不同的GPU
    CUDA_CHECK(cudaSetDevice(rank % deviceCount));
    printf("Rank %d 使用 GPU %d\n", rank, rank % deviceCount);
    
    // 创建NCCL唯一ID并广播给所有进程
    ncclUniqueId nccl_id;
    if (rank == 0) {
        NCCL_CHECK(ncclGetUniqueId(&nccl_id));
    }
    MPI_CHECK(MPI_Bcast(&nccl_id, sizeof(nccl_id), MPI_BYTE, 0, MPI_COMM_WORLD));
    
    // 创建NCCL通信器
    ncclComm_t comm;
    NCCL_CHECK(ncclCommInitRank(&comm, world_size, nccl_id, rank));
    
    printf("Rank %d: NCCL通信器初始化成功\n", rank);
    
    // 同步所有进程
    MPI_CHECK(MPI_Barrier(MPI_COMM_WORLD));
    
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
        
        if (rank == 0) {
            printf("\n测试数据大小: %.2f MB\n", size / (1024.0 * 1024.0));
        }
        
        // 分配GPU内存
        float *sendbuf, *recvbuf;
        CUDA_CHECK(cudaMalloc(&sendbuf, size));
        CUDA_CHECK(cudaMalloc(&recvbuf, size));
        
        // 分配主机内存用于验证
        float *h_sendbuf = (float*)malloc(size);
        float *h_recvbuf = (float*)malloc(size);
        
        // 初始化数据
        if (rank == 0) {
            for (size_t i = 0; i < count; i++) {
                h_sendbuf[i] = (float)rand() / RAND_MAX;
            }
        } else {
            memset(h_sendbuf, 0, size);
        }
        
        CUDA_CHECK(cudaMemcpy(sendbuf, h_sendbuf, size, cudaMemcpyHostToDevice));
        CUDA_CHECK(cudaMemset(recvbuf, 0, size));
        
        // 创建CUDA流
        cudaStream_t stream;
        CUDA_CHECK(cudaStreamCreate(&stream));
        
        // 同步所有进程
        MPI_CHECK(MPI_Barrier(MPI_COMM_WORLD));
        
        // -------------------------------------
        // 测试广播操作 (Broadcast)
        // -------------------------------------
        if (rank == 0) {
            printf("测试广播操作 (Broadcast):\n");
        }
        
        // 热身
        for (int i = 0; i < 3; i++) {
            NCCL_CHECK(ncclBroadcast(sendbuf, sendbuf, count, ncclFloat, 0, comm, stream));
            CUDA_CHECK(cudaStreamSynchronize(stream));
        }
        
        // 同步所有进程
        MPI_CHECK(MPI_Barrier(MPI_COMM_WORLD));
        
        // 计时开始
        double start_time = get_time_us();
        
        // 执行NCCL广播操作
        NCCL_CHECK(ncclBroadcast(sendbuf, sendbuf, count, ncclFloat, 0, comm, stream));
        CUDA_CHECK(cudaStreamSynchronize(stream));
        
        // 计时结束
        double end_time = get_time_us();
        double elapsed = end_time - start_time;
        
        // 收集所有进程的时间
        double max_time;
        MPI_CHECK(MPI_Reduce(&elapsed, &max_time, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD));
        
        // 将结果拷贝回主机内存以验证
        CUDA_CHECK(cudaMemcpy(h_recvbuf, sendbuf, size, cudaMemcpyDeviceToHost));
        
        // 仅在rank 0上输出性能结果
        if (rank == 0) {
            // 计算带宽
            double bandwidth = (size * (world_size-1)) / (max_time / 1000000.0) / (1024.0 * 1024.0 * 1024.0);
            printf("NCCL广播性能：时间 = %.3f ms, 吞吐量 = %.3f GB/s\n", 
                   max_time / 1000.0, bandwidth);
        }
        
        // 同步所有进程
        MPI_CHECK(MPI_Barrier(MPI_COMM_WORLD));
        
        // -------------------------------------
        // 测试规约操作 (AllReduce)
        // -------------------------------------
        if (rank == 0) {
            printf("\n测试规约操作 (AllReduce):\n");
        }
        
        // 重新初始化数据
        for (size_t i = 0; i < count; i++) {
            h_sendbuf[i] = (float)(rank + 1) / world_size;
        }
        
        CUDA_CHECK(cudaMemcpy(sendbuf, h_sendbuf, size, cudaMemcpyHostToDevice));
        CUDA_CHECK(cudaMemset(recvbuf, 0, size));
        
        // 热身
        for (int i = 0; i < 3; i++) {
            NCCL_CHECK(ncclAllReduce(sendbuf, recvbuf, count, ncclFloat, ncclSum, comm, stream));
            CUDA_CHECK(cudaStreamSynchronize(stream));
        }
        
        // 同步所有进程
        MPI_CHECK(MPI_Barrier(MPI_COMM_WORLD));
        
        // 计时开始
        start_time = get_time_us();
        
        // 执行NCCL AllReduce操作
        NCCL_CHECK(ncclAllReduce(sendbuf, recvbuf, count, ncclFloat, ncclSum, comm, stream));
        CUDA_CHECK(cudaStreamSynchronize(stream));
        
        // 计时结束
        end_time = get_time_us();
        elapsed = end_time - start_time;
        
        // 收集所有进程的时间
        MPI_CHECK(MPI_Reduce(&elapsed, &max_time, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD));
        
        // 将结果拷贝回主机内存以验证
        CUDA_CHECK(cudaMemcpy(h_recvbuf, recvbuf, size, cudaMemcpyDeviceToHost));
        
        // 验证AllReduce结果是否正确
        float expected_sum = 0;
        for (int i = 0; i < world_size; i++) {
            expected_sum += (float)(i + 1) / world_size;
        }
        
        bool allreduce_correct = true;
        for (size_t i = 0; i < count; i++) {
            if (fabsf(h_recvbuf[i] - expected_sum) > 1e-5) {
                printf("Rank %d: AllReduce验证失败：位置 %zu 值为 %f, 期望值 %f\n", 
                       rank, i, h_recvbuf[i], expected_sum);
                allreduce_correct = false;
                break;
            }
        }
        
        if (allreduce_correct && rank == 0) {
            printf("AllReduce验证成功：所有GPU的结果等于预期的总和\n");
        }
        
        // 仅在rank 0上输出性能结果
        if (rank == 0) {
            // 计算带宽 (2x因为数据双向流动)
            double bandwidth = (2.0 * size * world_size) / (max_time / 1000000.0) / (1024.0 * 1024.0 * 1024.0);
            printf("NCCL AllReduce性能：时间 = %.3f ms, 吞吐量 = %.3f GB/s\n", 
                   max_time / 1000.0, bandwidth);
        }
        
        // 释放资源
        CUDA_CHECK(cudaFree(sendbuf));
        CUDA_CHECK(cudaFree(recvbuf));
        free(h_sendbuf);
        free(h_recvbuf);
        CUDA_CHECK(cudaStreamDestroy(stream));
    }
    
    // 销毁NCCL通信器
    ncclCommDestroy(comm);
    
    // 结束MPI
    MPI_Finalize();
    
    return 0;
}