#include <stdio.h>
#include<stdlib.h>
#include <string.h>
#include "mpi.h"
#include<omp.h>


void merge(int l1, int r1, int r2, int* data, int* temp) {
	int top = l1, p = l1, q = r1;
	while (p < r1 || q < r2) {
		if (q >= r2 || (p < r1 && data[p] <= data[q])) {
			temp[top++] = data[p++];
		}
		else {
			temp[top++] = data[q++];
		}
	}
	for (top = l1; top < r2; top++) {
		data[top] = temp[top];
	}
}

void merge_sort(int l, int r, int* data, int N) {
	int i, j, t, * temp;
	temp = (int*)malloc(N * sizeof(int));
#pragma omp parallel for private(i, t) shared(N, data) 
	for (i = 0; i < N / 2; i++) {
		if (data[i * 2] > data[i * 2 + 1]) {
			t = data[i * 2];
			data[i * 2] = data[i * 2 + 1];
			data[i * 2 + 1] = t;
		}
		//printf("Hello world from #%d!\n", omp_get_thread_num());
	}

	for (i = 2; i < r; i *= 2) {
#pragma omp parallel for private(j) shared(r, i) 
		for (j = 0; j < r - i; j += i * 2) {
			merge(j, j + i, (j + i * 2 < r ? j + i * 2 : r), data, temp);
		//printf("Hello world from #%d!\n", omp_get_thread_num());
		}
	}
    free(temp);
}

void main(int argc, char* argv[])
{	
    double time_start, time_end;
    int ProNum, MyRank;
    int num_threads=2;//每个进程开的线程数
    int len = 512*1024*1024;//数据长度
    int* data = (int*)malloc(sizeof(int) * len);//为数组分配内存
    for (int j = 0; j < len; j++) {//为数组赋值
      data[j] = len-j;
    }
    omp_set_num_threads(num_threads);
    MPI_Init(&argc, &argv);//初始化MPI环境
    MPI_Comm_rank(MPI_COMM_WORLD, &MyRank);
    MPI_Comm_size(MPI_COMM_WORLD, &ProNum);
  	
 	MPI_Barrier(MPI_COMM_WORLD); //进程对齐
	time_start = MPI_Wtime();//开始记录时间
  
  	int size=len/ProNum;
  	int *recSubData=(int*)malloc(sizeof(int)*size);//为每个进程处理的数据分配接受缓存区
  	MPI_Scatter(data, size, MPI_INT, recSubData, size, MPI_INT, 0, MPI_COMM_WORLD);  // 将0号进程的数组元素分发给每个进程（包括它本身）0->[0,1,2,3]
    merge_sort(0, size, recSubData, size);
    // for(int i=0;i<len;i++){
    //     printf("%d:%d ",MyRank,data[i]);
    // }
  	MPI_Gather(recSubData, size, MPI_INT, data, size, MPI_INT, 0, MPI_COMM_WORLD);//将各个进程的数据归并到根进程[0,1,2,3]->0
  	if(MyRank==0){
        merge_sort(0, len, data, len);
      	for(int i=0;i<len;i++){
          	printf("%d ",data[i]);
        }
    }
  	MPI_Barrier(MPI_COMM_WORLD); //进程对齐
	time_end = MPI_Wtime();//结束时间记录

    MPI_Finalize();
  	free(data);
  	if(MyRank==0){
      	printf("Using 8 Process conduct 512M data MergeSort cost time: %f ms \n",(time_end-time_start)*100);
    }
} 