#include "custom_tcpdump.h"
#include <pcap.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define ERROR_BUFFER_SIZE 256

static char error_buffer[ERROR_BUFFER_SIZE];

typedef struct {
    void* buffer;
    size_t buffer_size;
    size_t offset;
    int max_packets;
    int packet_count;
} capture_ctx_t;

/**
 * 回调函数：处理每个捕获的数据包
 */
static void packet_handler(u_char *user_data, 
                          const struct pcap_pkthdr *pkthdr, 
                          const u_char *packet) {
    capture_ctx_t *ctx = (capture_ctx_t *)user_data;
    
    // 如果已达到最大包数限制，停止捕获
    if (ctx->max_packets > 0 && ctx->packet_count >= ctx->max_packets) {
        return;
    }
    
    // 检查缓冲区大小是否足够
    if (ctx->offset + sizeof(struct pcap_pkthdr) + pkthdr->caplen > ctx->buffer_size) {
        return; // 缓冲区已满
    }
    
    // 将包头复制到缓冲区
    memcpy((char *)ctx->buffer + ctx->offset, pkthdr, sizeof(struct pcap_pkthdr));
    ctx->offset += sizeof(struct pcap_pkthdr);
    
    // 将数据包复制到缓冲区
    memcpy((char *)ctx->buffer + ctx->offset, packet, pkthdr->caplen);
    ctx->offset += pkthdr->caplen;
    
    ctx->packet_count++;
}

int custom_tcpdump_capture(const char* iface,
                           const char* custom_filter,
                           void* buffer,
                           size_t buffer_size,
                           int timeout_ms,
                           int max_packets) {
    pcap_t *handle;
    struct bpf_program filter_program;
    bpf_u_int32 net, mask;
    int ret;
    capture_ctx_t ctx;
    
    error_buffer[0] = '\0';
    
    // 参数检查
    if (!iface || !buffer || buffer_size == 0) {
        snprintf(error_buffer, ERROR_BUFFER_SIZE, "Invalid parameters");
        return -1;
    }

    // 初始化上下文
    ctx.buffer = buffer;
    ctx.buffer_size = buffer_size;
    ctx.offset = 0;
    ctx.max_packets = max_packets;
    ctx.packet_count = 0;
    
    // 获取网络接口地址信息
    char pcap_errbuf[PCAP_ERRBUF_SIZE];
    if (pcap_lookupnet(iface, &net, &mask, pcap_errbuf) == -1) {
        net = 0;
        mask = 0;
        snprintf(error_buffer, ERROR_BUFFER_SIZE, "Warning: %s", pcap_errbuf);
        // 继续执行，因为这不是致命错误
    }
    
    // 打开网络接口进行抓包
    handle = pcap_open_live(iface, BUFSIZ, 1, timeout_ms, pcap_errbuf);
    if (handle == NULL) {
        snprintf(error_buffer, ERROR_BUFFER_SIZE, "Couldn't open device %s: %s", iface, pcap_errbuf);
        return -2;
    }
    
    // 编译过滤表达式
    if (custom_filter && strlen(custom_filter) > 0) {
        if (pcap_compile(handle, &filter_program, custom_filter, 0, net) == -1) {
            snprintf(error_buffer, ERROR_BUFFER_SIZE, "Couldn't parse filter '%s': %s", 
                    custom_filter, pcap_geterr(handle));
            pcap_close(handle);
            return -3;
        }
        
        // 应用过滤规则
        if (pcap_setfilter(handle, &filter_program) == -1) {
            snprintf(error_buffer, ERROR_BUFFER_SIZE, "Couldn't install filter '%s': %s", 
                    custom_filter, pcap_geterr(handle));
            pcap_freecode(&filter_program);
            pcap_close(handle);
            return -4;
        }
        
        pcap_freecode(&filter_program);
    }
    
    // 开始抓包
    ret = pcap_loop(handle, max_packets > 0 ? max_packets : -1, packet_handler, (u_char *)&ctx);
    
    // 检查抓包结果
    if (ret == -1) {
        snprintf(error_buffer, ERROR_BUFFER_SIZE, "Error in pcap_loop: %s", pcap_geterr(handle));
        pcap_close(handle);
        return -5;
    } else if (ret == -2) {
        // pcap_breakloop() was called
        snprintf(error_buffer, ERROR_BUFFER_SIZE, "Capture interrupted");
    }
    
    // 关闭抓包句柄
    pcap_close(handle);
    
    return ctx.packet_count;
}

const char* custom_tcpdump_get_error(void) {
    return error_buffer;
}
