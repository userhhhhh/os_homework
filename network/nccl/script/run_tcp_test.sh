#!/bin/bash
# 在一个进程运行rank 1（后台）
./tcp_ip_test 1 2 127.0.0.1 127.0.0.1 > ./log/tcp_rank1.log 2>&1 &  
# 等待2秒
sleep 2
# 在另一个进程运行rank 0
./tcp_ip_test 0 2 127.0.0.1 127.0.0.1 > ./log/tcp_rank0.log 2>&1
# 等待rank 1完成
wait
# 合并输出
cat ./log/tcp_rank1.log ./log/tcp_rank0.log
