// tcp_ip_test.cpp
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <vector>
#include <string>
#include <cmath>

// 定义操作类型
#define OP_BROADCAST 0
#define OP_ALLREDUCE 1

// 定义端口基址
#define BASE_PORT 5000

// 错误处理宏
#define TCP_CHECK(x) do { \
    if (!(x)) { \
        fprintf(stderr, "Error at %s:%d: %s\n", __FILE__, __LINE__, strerror(errno)); \
        exit(EXIT_FAILURE); \
    } \
} while (0)

// 节点信息结构
typedef struct {
    std::string ip;
    int rank;
} node_info;

// 获取当前时间（微秒）
double get_time_us() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000000 + tv.tv_usec;
}

// TCP发送函数
void tcp_send(int sock, void* data, size_t size) {
    size_t total_sent = 0;
    while (total_sent < size) {
        int sent = send(sock, (char*)data + total_sent, size - total_sent, 0);
        TCP_CHECK(sent > 0);
        total_sent += sent;
    }
}

// TCP接收函数
void tcp_recv(int sock, void* data, size_t size) {
    size_t total_received = 0;
    while (total_received < size) {
        int received = recv(sock, (char*)data + total_received, size - total_received, 0);
        TCP_CHECK(received > 0);
        total_received += received;
    }
}

// 创建与其他节点的连接
std::vector<int> establish_connections(const std::vector<node_info>& nodes, int my_rank) {
    std::vector<int> sockets(nodes.size(), -1);
    
    // 作为服务器接受比自己rank高的节点连接
    for (size_t i = my_rank + 1; i < nodes.size(); i++) {
        int server_fd = socket(AF_INET, SOCK_STREAM, 0);
        TCP_CHECK(server_fd >= 0);
        
        int opt = 1;
        TCP_CHECK(setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == 0);
        
        struct sockaddr_in address;
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = INADDR_ANY;
        address.sin_port = htons(BASE_PORT + my_rank);
        
        TCP_CHECK(bind(server_fd, (struct sockaddr*)&address, sizeof(address)) == 0);
        TCP_CHECK(listen(server_fd, 10) == 0);
        
        printf("Rank %d 等待 Rank %zu 连接...\n", my_rank, i);
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int new_socket = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
        TCP_CHECK(new_socket >= 0);
        
        // 接收rank信息以确认连接的是哪个节点
        int connecting_rank;
        tcp_recv(new_socket, &connecting_rank, sizeof(int));
        
        printf("Rank %d 接受了来自 Rank %d 的连接\n", my_rank, connecting_rank);
        sockets[connecting_rank] = new_socket;
        close(server_fd);
    }
    
    // 作为客户端连接到rank比自己小的节点
    for (int i = 0; i < my_rank; i++) {
        int client_fd = socket(AF_INET, SOCK_STREAM, 0);
        TCP_CHECK(client_fd >= 0);
        
        struct sockaddr_in server_addr;
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(BASE_PORT + i);
        
        // 转换IP地址
        TCP_CHECK(inet_pton(AF_INET, nodes[i].ip.c_str(), &server_addr.sin_addr) > 0);
        
        printf("Rank %d 尝试连接到 Rank %d (%s:%d)...\n", 
               my_rank, i, nodes[i].ip.c_str(), BASE_PORT + i);
               
        // 尝试连接，如果失败则等待一秒后重试
        int retry = 0;
        while (connect(client_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            if (retry++ > 10) {
                perror("连接失败");
                exit(EXIT_FAILURE);
            }
            printf("连接失败，1秒后重试...\n");
            sleep(1);
        }
        
        // 发送自己的rank以便对方确认
        tcp_send(client_fd, &my_rank, sizeof(int));
        
        printf("Rank %d 成功连接到 Rank %d\n", my_rank, i);
        sockets[i] = client_fd;
    }
    
    return sockets;
}

// 关闭所有连接
void close_connections(const std::vector<int>& sockets) {
    for (int sock : sockets) {
        if (sock >= 0) {
            close(sock);
        }
    }
}

// TCP广播实现
void tcp_broadcast(float* data, size_t count, int root, int my_rank, 
                  const std::vector<int>& sockets) {
    size_t size = count * sizeof(float);
    
    if (my_rank == root) {
        // 根节点发送给所有其他节点
        for (size_t i = 0; i < sockets.size(); i++) {
            if ((int)i != root && sockets[i] >= 0) {
                tcp_send(sockets[i], data, size);
            }
        }
    } else {
        // 非根节点从根节点接收
        tcp_recv(sockets[root], data, size);
    }
}

// TCP AllReduce实现 (使用简单的Reduce-then-Broadcast算法)
void tcp_allreduce(float* sendbuf, float* recvbuf, size_t count, int my_rank,
                  const std::vector<int>& sockets) {
    size_t size = count * sizeof(float);
    
    // 首先复制数据到接收缓冲区
    memcpy(recvbuf, sendbuf, size);

    // 第1阶段：Reduce到rank 0
    if (my_rank == 0) {
        float* temp_buf = (float*)malloc(size);
        
        // 从每个其他rank接收并累加
        for (size_t i = 1; i < sockets.size(); i++) {
            tcp_recv(sockets[i], temp_buf, size);
            
            // 执行sum操作
            for (size_t j = 0; j < count; j++) {
                recvbuf[j] += temp_buf[j];
            }
        }
        
        free(temp_buf);
    } else {
        // 其他rank发送到rank 0
        tcp_send(sockets[0], recvbuf, size);
    }
    
    // 第2阶段：将结果广播给所有节点
    tcp_broadcast(recvbuf, count, 0, my_rank, sockets);
}

// TCP性能测试程序
int tcp_test_main(int argc, char** argv) {
    // 检查参数
    if (argc < 3) {
        printf("用法: %s <我的rank> <节点总数> [IP地址1] [IP地址2] ...\n", argv[0]);
        return 1;
    }
    
    int my_rank = atoi(argv[1]);
    int world_size = atoi(argv[2]);
    
    if (my_rank >= world_size) {
        printf("错误: rank超出范围\n");
        return 1;
    }
    
    // 存储节点信息
    std::vector<node_info> nodes;
    for (int i = 0; i < world_size; i++) {
        node_info node;
        node.rank = i;
        
        if (i == my_rank) {
            node.ip = "127.0.0.1"; // 自身IP不重要
        } else if (i + 3 < argc) {
            node.ip = argv[i + 3];
        } else {
            printf("错误: 缺少rank %d的IP地址\n", i);
            return 1;
        }
        
        nodes.push_back(node);
    }
    
    printf("我是rank %d，总共有 %d 个节点\n", my_rank, world_size);
    
    // 建立与其他节点的连接
    std::vector<int> sockets = establish_connections(nodes, my_rank);
    printf("Rank %d: 所有连接已建立\n", my_rank);
    
    // 等待所有节点同步
    if (my_rank == 0) {
        // 接收其他所有节点的同步信号
        int signal;
        for (int i = 1; i < world_size; i++) {
            tcp_recv(sockets[i], &signal, sizeof(int));
        }
        
        // 发送开始信号
        signal = 1;
        for (int i = 1; i < world_size; i++) {
            tcp_send(sockets[i], &signal, sizeof(int));
        }
    } else {
        // 发送同步信号
        int signal = 1;
        tcp_send(sockets[0], &signal, sizeof(int));
        
        // 等待开始信号
        tcp_recv(sockets[0], &signal, sizeof(int));
    }
    
    // 数据大小（从1MB到256MB）
    size_t sizes[] = {
        1 << 20,      // 1 MB
        4 << 20,      // 4 MB
        16 << 20,     // 16 MB
        64 << 20,     // 64 MB
        256 << 20     // 256 MB
    };
    
    // 为每种大小运行测试
    for (int s = 0; s < sizeof(sizes)/sizeof(sizes[0]); s++) {
        size_t size = sizes[s];
        size_t count = size / sizeof(float);
        
        printf("\nRank %d: 测试数据大小: %.2f MB\n", my_rank, size / (1024.0 * 1024.0));
        
        // 分配内存
        float* sendbuf = (float*)malloc(size);
        float* recvbuf = (float*)malloc(size);
        
        // 初始化数据
        if (my_rank == 0) {
            for (size_t i = 0; i < count; i++) {
                sendbuf[i] = (float)rand() / RAND_MAX;
            }
        } else {
            memset(sendbuf, 0, size);
        }
        
        // 同步所有节点
        if (my_rank == 0) {
            int signal;
            for (int i = 1; i < world_size; i++) {
                tcp_recv(sockets[i], &signal, sizeof(int));
            }
            signal = 1;
            for (int i = 1; i < world_size; i++) {
                tcp_send(sockets[i], &signal, sizeof(int));
            }
        } else {
            int signal = 1;
            tcp_send(sockets[0], &signal, sizeof(int));
            tcp_recv(sockets[0], &signal, sizeof(int));
        }
        
        // 测试TCP广播
        printf("Rank %d: 测试TCP广播...\n", my_rank);
        
        // 执行热身以减少系统噪声
        for (int i = 0; i < 3; i++) {
            tcp_broadcast(sendbuf, count, 0, my_rank, sockets);
        }
        
        // 实际计时测试
        double start_time = get_time_us();
        
        tcp_broadcast(sendbuf, count, 0, my_rank, sockets);
        
        double end_time = get_time_us();
        double elapsed = end_time - start_time;
        
        if (my_rank == 0) {
            // 等待其他节点完成并收集他们的时间
            double max_elapsed = elapsed;
            for (int i = 1; i < world_size; i++) {
                double remote_elapsed;
                tcp_recv(sockets[i], &remote_elapsed, sizeof(double));
                if (remote_elapsed > max_elapsed) max_elapsed = remote_elapsed;
            }
            
            // 计算带宽和延迟
            double bandwidth = (size * (world_size - 1)) / (max_elapsed / 1000000.0) / (1024.0 * 1024.0 * 1024.0);
            printf("TCP广播性能：时间 = %.3f ms, 吞吐量 = %.3f GB/s\n", 
                   max_elapsed / 1000.0, bandwidth);
        } else {
            // 发送自己的时间到rank 0
            tcp_send(sockets[0], &elapsed, sizeof(double));
        }
        
        // 再次同步
        if (my_rank == 0) {
            int signal;
            for (int i = 1; i < world_size; i++) {
                tcp_recv(sockets[i], &signal, sizeof(int));
            }
            signal = 1;
            for (int i = 1; i < world_size; i++) {
                tcp_send(sockets[i], &signal, sizeof(int));
            }
        } else {
            int signal = 1;
            tcp_send(sockets[0], &signal, sizeof(int));
            tcp_recv(sockets[0], &signal, sizeof(int));
        }
        
        // 测试TCP AllReduce
        printf("Rank %d: 测试TCP AllReduce...\n", my_rank);
        
        // 初始化数据 - 每个节点不同的值
        for (size_t i = 0; i < count; i++) {
            sendbuf[i] = (float)(my_rank + 1) / world_size;
        }
        
        // 执行热身
        for (int i = 0; i < 3; i++) {
            tcp_allreduce(sendbuf, recvbuf, count, my_rank, sockets);
        }
        
        // 实际计时测试
        start_time = get_time_us();
        
        tcp_allreduce(sendbuf, recvbuf, count, my_rank, sockets);
        
        end_time = get_time_us();
        elapsed = end_time - start_time;
        
        // 验证结果
        float expected_sum = 0;
        for (int i = 0; i < world_size; i++) {
            expected_sum += (float)(i + 1) / world_size;
        }
        
        bool allreduce_correct = true;
        for (size_t i = 0; i < count; i++) {
            if (fabs(recvbuf[i] - expected_sum) > 1e-5) {
                printf("Rank %d: AllReduce验证失败：位置 %zu 值为 %f, 期望值 %f\n", 
                       my_rank, i, recvbuf[i], expected_sum);
                allreduce_correct = false;
                break;
            }
        }
        
        if (my_rank == 0 && allreduce_correct) {
            printf("Rank %d: AllReduce验证成功\n", my_rank);
        }
        
        if (my_rank == 0) {
            // 等待其他节点完成并收集他们的时间
            double max_elapsed = elapsed;
            for (int i = 1; i < world_size; i++) {
                double remote_elapsed;
                tcp_recv(sockets[i], &remote_elapsed, sizeof(double));
                if (remote_elapsed > max_elapsed) max_elapsed = remote_elapsed;
            }
            
            // 计算带宽和延迟
            double bandwidth = (2.0 * size * world_size) / (max_elapsed / 1000000.0) / (1024.0 * 1024.0 * 1024.0);
            printf("TCP AllReduce性能：时间 = %.3f ms, 吞吐量 = %.3f GB/s\n", 
                   max_elapsed / 1000.0, bandwidth);
        } else {
            // 发送自己的时间到rank 0
            tcp_send(sockets[0], &elapsed, sizeof(double));
        }
        
        // 释放内存
        free(sendbuf);
        free(recvbuf);
    }
    
    // 清理连接
    close_connections(sockets);
    
    printf("Rank %d: 测试完成\n", my_rank);
    return 0;
}

int main(int argc, char** argv) {
    return tcp_test_main(argc, argv);
}
