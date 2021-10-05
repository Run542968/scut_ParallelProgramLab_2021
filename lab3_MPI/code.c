//运行平台：超算习堂
//学习资料：
//介绍MPI函数: https://www.jianshu.com/p/2fd31665e816
//MPI的Scatter和Gether: https://blog.csdn.net/sinat_22336563/article/details/70229243
//记录MPI程序时间: https://blog.csdn.net/silent56_th/article/details/80419314
#include <stdio.h>
#include<stdlib.h>
#include <string.h>
#include "mpi.h"

void MergeSort( int arr[], int originalStep, int start, int end)
{	
	int len = end - start + 1;
	int left_min, left_max, right_min, right_max;
	int* temp = (int*)malloc(len * sizeof(int));
	for (int i = originalStep; i < len; i *= 2)//步长
	{
		for (left_min = start; left_min < end + 1 - i; left_min = right_max)
		{
			right_min = left_max = left_min + i;
			right_max = left_max + i;
			if (right_max > end+1)
			{
				right_max = end+1;
			}
			int next = 0;
			while (left_min < left_max && right_min < right_max)
			{
				if (arr[left_min] < arr[right_min])
				{
					temp[next++] = arr[left_min++];
				}
				else
				{
					temp[next++] = arr[right_min++];
				}
			}
			while (left_min < left_max)
			{
				arr[--right_min] = arr[--left_max];
			}
			while (next > 0)
			{
				arr[--right_min] = temp[--next];
			}
		}
	}
  free(temp);
}
void main(int argc, char* argv[])
{	
    double time_start, time_end;
    int ProNum, MyRank;
  	int process_num=256;
  	int len_size=512;
  	int len = len_size*1024*1024;//数据长度
    int* data = (int*)malloc(sizeof(int) * len);//为数组分配内存
    for (int j = 0; j < len; j++) {//为数组赋值
      data[j] = len-j;
    }
    MPI_Init(&argc, &argv);//初始化MPI环境
    MPI_Comm_rank(MPI_COMM_WORLD, &MyRank);
    MPI_Comm_size(MPI_COMM_WORLD, &ProNum);
  	
 	MPI_Barrier(MPI_COMM_WORLD); //进程对齐
	time_start = MPI_Wtime();//开始记录时间
  
  	int size=len/ProNum;
  	int *recSubData=(int*)malloc(sizeof(int)*len);//为每个进程处理的数据分配接受缓存区
  	MPI_Scatter(data, size, MPI_INT, recSubData, size, MPI_INT, 0, MPI_COMM_WORLD);  // 将0号进程的数组元素分发给每个进程（包括它本身）0->[0,1,2,3]
  	MergeSort(recSubData,1,0,size);
  
  	MPI_Gather(recSubData, size, MPI_INT, data, size, MPI_INT, 0, MPI_COMM_WORLD);//将各个进程的数据归并到根进程[0,1,2,3]->0
  	if(MyRank==0){
      	MergeSort(data,size,0,len-1);
      	/*for(int i=0;i<len;i++){
          	printf("%d ",data[i]);
        }*/
    }
  	MPI_Barrier(MPI_COMM_WORLD); //进程对齐
	time_end = MPI_Wtime();//结束时间记录

    MPI_Finalize();
  	free(data);
  	if(MyRank==0){
      	printf("Using %d threads conduct %dM data MergeSort cost time: %.2f ms \n",process_num,len_size,(time_end-time_start)*100);
    }
} 