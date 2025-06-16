nvcc -o nccl nccl.cu -lnccl -lcudart
nvcc -o nccl_multi_node nccl_multi_node.cu \
    -lnccl -lcudart \
    -I/usr/include -L/usr/lib/x86_64-linux-gnu \
    -I/usr/lib/x86_64-linux-gnu/openmpi/include -L/usr/lib/x86_64-linux-gnu/openmpi/lib \
    -lmpi
g++ -o tcp_ip_test tcp_ip_test.cpp -pthread

# 找 path：mpicc --showme:compile
