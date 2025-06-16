# nccl

## task1

```bash
cd /home/featurize/work/nccl && ./script/compile.sh 
./nccl
```

## task2

```bash
mpirun -np 2 -H localhost,localhost ./nccl_multi_node # nccl
./script/run_tcp_test.sh # tcp，需要稍微等几秒
```

<!-- 在一个终端上启动 Rank1 进程

```bash
./tcp_ip_test 1 2 127.0.0.1 127.0.0.1
```

在另一个终端上启动 Rank2 进程

```bash
./tcp_ip_test 0 2 127.0.0.1 127.0.0.1
``` -->