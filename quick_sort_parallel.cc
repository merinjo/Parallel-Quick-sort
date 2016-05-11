#include <iostream>
#include <stdlib.h>
#include <stdio.h>
#include "carbon_user.h"
#include <pthread.h>
using namespace std;

#define LEFT   0;
#define RIGHT  1; 
#define BOTH   2; 

int partition( int* A, int, int);
void* quickSortMain1( void* data);
void* quickSortWorker1( void* d);
int n = 128;
const int MAX_THREADS = 64;
int thread_count = 0;
pthread_t thread_handles[MAX_THREADS];
pthread_barrier_t barrier; 

static pthread_mutex_t lock1[MAX_THREADS];
static volatile int terminate_condition = 0;

struct core_data
{
    int pid;
	int flag;
	int L1;
	int R1;
	int L2;
	int R2;
    char dummy[40];
};
struct core_data *worker;

struct thread_input_data
{
   int *array;
   int left;
   int right;
   int ID;
   int ready_flag;
};


struct thread_input_data *info = (struct thread_input_data *)malloc(MAX_THREADS*sizeof(struct thread_input_data));

int partition(int* A, int left , int right)
{
   int x= A[left];
   int i = left;
   int j;
   int temp;
   
   for(j=left+1; j<right; j++)
   {
      if(A[j]<=x)
      {
         i=i+1;
         swap(A[i], A[j]);
      }
   }
   swap(A[i], A[left]);
   return i;
}

void quickSort(int *A, int p,int q)
{
   int r;
   if(p < q)
   {
      r = partition(A, p,q);
      quickSort(A,p,r);  
      quickSort(A,r+1,q);
   }
}

int main(int argc, char* argv[])
{
	
	info[0].array = (int *)malloc(sizeof(int)*n);
	posix_memalign((void**) &worker, 64, MAX_THREADS * 64);
	
	for (int i = 0; i < MAX_THREADS; i++)
    {
      worker[i].pid = i;
      worker[i].flag = 0;
      worker[i].L1 = -2;
      worker[i].R1 = -2;
      worker[i].L2 = -2;
		worker[i].R2 = -2;
      
      info[i].array = info[0].array;
      info[i].ID = i;
      info[i].left = -2;
      info[i].right = -2;
      info[i].ready_flag = 0;
    }
	
    srand(69);
    for (int i = 0; i < n; i++)
    {
        info[0].array[i] = rand() % 1000;
    }
	

	pthread_barrier_init(&barrier, NULL, MAX_THREADS);
	
	CarbonEnableModels();
	
   for (int i = 1; i < MAX_THREADS; i++)
   {
      int ret = pthread_create(&thread_handles[i], NULL, quickSortWorker1, &info[i]);
      if (ret != 0)
      {
         fprintf(stderr, "ERROR spawning thread %i\n", i);
         exit(EXIT_FAILURE);
      }
   }

	quickSortMain1(&info[0]);

   cout << "Starting thread join" << endl;
   for(int i = 1; i < MAX_THREADS; i++)
   {
      pthread_join(thread_handles[i], NULL);
   }
   cout << "End thread join" << endl;	

	CarbonDisableModels();
	
    cout << "Sorted array\n";
    for (int i = 1; i < n; i++)
    {
       if(info->array[i-1] > info->array[i])
       {
          cout << "Verification unsuccessful for " << n << " elements at index " << i << endl; 
          exit(1);
       }
    }
    cout << "Verification Successful\n";
	return NULL;
}

void* quickSortMain1(void *data)
{
	struct thread_input_data *input_data = (thread_input_data *)data;
   int id,pivot;
   int *array;
   int left = 0;
   int right = n;
   int ID;
   int ready_flag;
   int last_input_id = -1;

   array = input_data->array;
   ID = input_data->ID;

   // Assign first input structure
   info[0].left = left;
   info[0].right = right;
   info[0].ready_flag = 1;
	
	while(1)
	{
      
      // Assign tasks to workers 
      int worker_id = 0;
      int count,index;
      for(int i = 0; i<MAX_THREADS; i++)
      {
         if(worker[i].flag == 1) // Something is there 
         {
            // Job Assigned
            worker[i].flag = 0;

            count = 0;
            for(int j = 0; j < MAX_THREADS; j++)
            {
               if(worker[j].flag == 0 && info[j].ready_flag == 0 && count == 0)
               {
                  info[j].left = worker[i].L1;
                  info[j].right = worker[i].R1;
                  info[j].ready_flag = 1; 
                  count++;
                  index = j;
                  continue;
               }
               else if(worker[j].flag == 0 && info[j].ready_flag == 0 && count == 1)
               {
                  info[j].left = worker[i].L2;
                  info[j].right = worker[i].R2;
                  info[j].ready_flag = 1; 
                  count++;
                  break;
               }
            }

            if(count <= 1)
            {
               info[index].ready_flag = 0;
			   quickSort(array, worker[i].L1, worker[i].R2);
               break;
            }     
         }
      }


      // Go!
		pthread_barrier_wait(&barrier);
		
      // Ready to go!
      left = input_data->left;
      right = input_data->right;
      ready_flag = input_data->ready_flag;
		
      if(ready_flag == 1)
		{
         if(left < right)
         {
            pivot = partition(array, left, right);
            if(pivot < 0)
               pivot = 0;

            if(right < 0)
               right = 0;

            worker[ID].L1 = left;
            worker[ID].R1 = pivot;
            worker[ID].L2 = pivot + 1;
            worker[ID].R2 = right;
            worker[ID].flag = 1;

            // Job done
            info[ID].ready_flag = 0;
         }
         else
            info[ID].ready_flag = 0;
		}

      // Wait for everyone
		pthread_barrier_wait(&barrier);

      // Check for termination condition
      terminate_condition = 1;
      for(int i=0; i<MAX_THREADS; i++)
      {
         if(worker[i].flag == 1)
         {
            terminate_condition = 0;
            break;
         }
      }

		
      pthread_barrier_wait(&barrier);

      if( terminate_condition == 1 )
         break;
	}
	
    return 0;
}

void* quickSortWorker1(void *data)
{
	struct thread_input_data *input_data = (thread_input_data *)data;
   int id,pivot;
   int *array;
   int left;
   int right;
   int ID;
   int ready_flag;

   array = input_data->array;
   ID = input_data->ID;
	
	while(1)
	{
      // Wait for main thread to populate the input data
		pthread_barrier_wait(&barrier);
      
      // Ready to go!
      left = input_data->left;
      right = input_data->right;
      ready_flag = input_data->ready_flag;
		
      if(ready_flag == 1)
		{
         if(left < right)
         {
            pivot = partition(array, left, right);
            if(pivot < 0)
               pivot = 0;
		   
		    if(right < 0)
               right = 0;

            worker[ID].L1 = left;
            worker[ID].R1 = pivot;
            worker[ID].L2 = pivot + 1;
            worker[ID].R2 = right;
            worker[ID].flag = 1;

            // Job done
            info[ID].ready_flag = 0;
         }
         else
            info[ID].ready_flag = 0;
            
		}

		pthread_barrier_wait(&barrier);

      pthread_barrier_wait(&barrier);
      
      if( terminate_condition == 1 )
         break;
		
	}
   return 0;
}

