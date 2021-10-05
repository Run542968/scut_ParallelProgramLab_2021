#include<stdio.h>
#include<stdlib.h>
#include <sys/time.h>
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
}


int main()
{
	float time_use=0;//记录时间的参数
    struct timeval start;
    struct timeval end;

	int len = 1000000;//数据长度
	int num_threads = 64;//线程数量

	int* data = (int*)malloc(sizeof(int) * len);//为数组分配内存
	for (int j = 0; j < len; j++) {//为数组赋值
		data[j] = len - j;
	}

	omp_set_num_threads(num_threads);
    gettimeofday(&start,NULL);//开始记录时间
	merge_sort(0, len, data, len);
	gettimeofday(&end,NULL);//结束时间记录

    time_use=(end.tv_sec-start.tv_sec)*1000+(end.tv_usec-start.tv_usec)/1000;//毫秒

	/*for (int i = 0; i < 100;i++) {
		printf("%d \n",data[i]);
	}*/
	//printf("Serial program sort 512M Data, time usage is %.4f ms .\n",time_use);
    printf("64 threads parallel program sort 1M Data, time usage is %.4f ms .\n",time_use);
}


