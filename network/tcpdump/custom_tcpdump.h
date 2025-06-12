#ifndef CUSTOM_TCPDUMP_H
#define CUSTOM_TCPDUMP_H

#include <stddef.h>

/**
 * @brief 使用自定义过滤规则对网络数据进行抓包
 * @param iface 需要进行抓包的网络接口名称
 * @param custom_filter 用户自定义的过滤表达式
 * @param buffer 存储抓取到的数据缓冲区
 * @param buffer_size 存储抓包数据的缓冲区大小
 * @param timeout_ms 抓包超时时间(毫秒)，0表示一直抓取直到缓冲区满
 * @param max_packets 最多抓取的数据包数量，0表示不限制
 * @return 成功时返回抓取到的数据包数量，失败返回负值错误码
 */
int custom_tcpdump_capture(const char* iface,
                          const char* custom_filter,
                          void* buffer,
                          size_t buffer_size,
                          int timeout_ms,
                          int max_packets);

/**
 * @brief 获取上次操作的错误信息
 * @return 错误信息字符串
 */
const char* custom_tcpdump_get_error(void);

#endif /* CUSTOM_TCPDUMP_H */
