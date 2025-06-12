#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pcap.h>
#include "custom_tcpdump.h"

#define BUFFER_SIZE 1024 * 1024  // 1MB 缓冲区

// 打印捕获到的数据包
void print_captured_packets(void* buffer, int packet_count) {
    char* ptr = (char*)buffer;
    size_t offset = 0;
    
    printf("捕获到 %d 个数据包:\n", packet_count);
    
    for (int i = 0; i < packet_count; i++) {
        struct pcap_pkthdr* header = (struct pcap_pkthdr*)ptr;
        ptr += sizeof(struct pcap_pkthdr);
        
        printf("\n数据包 #%d:\n", i + 1);
        printf("  时间戳: %ld.%06ld\n", header->ts.tv_sec, header->ts.tv_usec);
        printf("  捕获长度: %d\n", header->caplen);
        printf("  实际长度: %d\n", header->len);
        
        printf("  数据 (前16字节):");
        for (int j = 0; j < 16 && j < header->caplen; j++) {
            if (j % 8 == 0) printf("\n    ");
            printf("%02x ", (unsigned char)ptr[j]);
        }
        printf("\n");
        
        ptr += header->caplen;
    }
}

int main(int argc, char *argv[]) {
    void* buffer = malloc(BUFFER_SIZE);
    if (!buffer) {
        fprintf(stderr, "内存分配失败\n");
        return 1;
    }
    
    const char* interface = argc > 1 ? argv[1] : "eth0";  // 默认使用eth0接口
    const char* filter = argc > 2 ? argv[2] : "tcp";      // 默认过滤TCP包
    
    printf("开始在接口 '%s' 上抓取数据包，过滤条件: '%s'\n", interface, filter);
    printf("按Ctrl+C停止捕获...\n");
    
    int result = custom_tcpdump_capture(
        interface,            // 网络接口
        filter,              // 过滤表达式
        buffer,              // 缓冲区
        BUFFER_SIZE,         // 缓冲区大小
        1000,                // 超时时间：1秒
        10                   // 最多捕获10个包
    );
    
    if (result < 0) {
        fprintf(stderr, "抓包失败: %s\n", custom_tcpdump_get_error());
        free(buffer);
        return 1;
    }
    
    // 显示捕获到的数据包
    print_captured_packets(buffer, result);
    
    free(buffer);
    return 0;
}
