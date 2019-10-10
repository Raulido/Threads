//Raul Pulido 5/14/19

#include <pthread.h>
#include <signal.h>
#include <iostream>
#include <setjmp.h>
#include <stdlib.h>
#include <cstring>
#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>
#include <semaphore.h>
#include <errno.h>
#include <vector>
#define EDEADLK 35
#define ESRCH 3
#define EINVAL 22
#define EOVERFLOW 75
#define THREAD_SIZE 32767
#define THREAD_MAX 128
#define SEM_VALUE_MAX 65536
using namespace std;
int pthread_join(pthread_t thread, void **value_ptr);
int sem_init(sem_t *sem, int pshared, unsigned int value);
int sem_destroy(sem_t *sem);
int sem_wait(sem_t *sem);
int sem_post(sem_t *sem);
void nextThread();
struct itimerval zero_timer = { 0 };
struct sigaction sa;
struct itimerval it;
bool is_first = true;
bool sema_is_first = true;
int threads = 0;
long unsigned threadID = 0;
void lock(){
  setitimer(ITIMER_REAL, &zero_timer, &it);
}
void unlock(){
  setitimer(ITIMER_REAL, &it, NULL);
}
typedef struct TCB *node;
struct TCB{
  int status;
  pthread_t TID;
  jmp_buf buf;
  struct TCB *next;
  void *(*function) (void *);
  void *arg;
  void* stack;
  void* collected;
  void* returnVal;
  node hunting_ptr;
  node being_hunted_ptr;
};

void deleteNode(node delete_ptr);
node thread_ptr;
node head_ptr;
jmp_buf BUF;

//----------------------------------------------
struct Semaphore{
  unsigned int value;
  int status;
  vector<node> waitList;
};
//----------------------------------------------
static long int i64_ptr_mangle(long int p){
	long int ret;
	asm(" mov %1, %%rax;\n"
		" xor %%fs:0x30, %%rax;"
		" rol $0x11, %%rax;"
		" mov %%rax, %0;"
	: "=r"(ret)
	: "r"(p)
	: "%rax"
	);
	return ret;
}
void handler(int signum){
  setitimer(ITIMER_REAL, &zero_timer, &it);
  //cout << "signal" << endl;
  if(setjmp(thread_ptr->buf) == 0){
    nextThread();
    setjmp(BUF);
    setitimer(ITIMER_REAL, &it, NULL);
    //cout << "jumping too " << thread_ptr->TID << endl;
    longjmp(thread_ptr->buf, 1);
  }
  else{
    if(thread_ptr->TID == 0 && threads == 1){
      free(thread_ptr);
      is_first = true;
    }
    setitimer(ITIMER_REAL, &it, NULL);
    return;
  }
}
void start(){
  sigemptyset(&sa.sa_mask);
  sa.sa_handler = handler;
  sa.sa_flags = SA_NODEFER;
  sigaction(SIGALRM,&sa,NULL);
  it.it_value.tv_sec = 50/1000;
  it.it_value.tv_usec = (50*1000)%1000000;
  it.it_interval = it.it_value;
  setitimer(ITIMER_REAL, &it, NULL);
}
void init(){
  is_first = false;
  node temp;
	temp = (node)malloc(sizeof(struct TCB));
	temp->TID = 0;
  threadID++;
  threads++;
	temp->next = NULL;
  thread_ptr = temp;
  head_ptr = temp;
  setjmp(temp->buf);
  start();
  return;
}
void wrapper(){
  void * temp = thread_ptr->function((thread_ptr->arg));
  pthread_exit(temp);
}
int pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine) (void *), void *arg){
  setitimer(ITIMER_REAL, &zero_timer, &it);
  if(is_first){
    init();
  }
  node noob;
  noob = (node)malloc(sizeof(struct TCB));
  noob->next = head_ptr;
  head_ptr = noob;
  void* thread_stack = (void*)malloc(32767);
  noob->stack = thread_stack;
  memset(thread_stack, 0, 32767);
  void* top_of_stack = (long int*)thread_stack + 32767/8;
  noob->buf[0].__jmpbuf[6] = i64_ptr_mangle((long int)((long int*)top_of_stack-2));
  noob->buf[0].__jmpbuf[7] = i64_ptr_mangle((long int)(&wrapper));
  noob->function = start_routine;
  noob->arg = arg;
  noob->TID = threadID;
  *thread = threadID;
  threadID++;
  threads++;
  setitimer(ITIMER_REAL, &it, NULL);
  return 0;
}
void pthread_exit(void *retval){
  //cout << "this thread exiting " << thread_ptr->TID << endl;
  setitimer(ITIMER_REAL, &zero_timer, &it);
  thread_ptr->returnVal = retval;
  thread_ptr->status = -2;
  if(thread_ptr->being_hunted_ptr != NULL){
    thread_ptr->being_hunted_ptr->status = 0;
  }
  nextThread();
  setitimer(ITIMER_REAL, &it, NULL);
  longjmp(thread_ptr->buf,1);
  while(1){}
}
pthread_t pthread_self(void){
  return thread_ptr->TID;
}

int pthread_join(pthread_t thread, void **value_ptr){
  setitimer(ITIMER_REAL, &zero_timer, &it);
  node temp_ptr2 = head_ptr;
  if(thread_ptr->TID == thread){
    return EDEADLK;
  }
  else{
    node temp_ptr = head_ptr;
    while(temp_ptr->TID != thread){
      if(temp_ptr->next == NULL){
        return ESRCH;
      }
      temp_ptr = temp_ptr->next;
    }
    if(temp_ptr->being_hunted_ptr != NULL){
      return EINVAL;
    }
    thread_ptr->hunting_ptr = temp_ptr;
    temp_ptr->being_hunted_ptr = thread_ptr;
    thread_ptr->status = -1;
    setjmp(thread_ptr->buf);
    if(temp_ptr->status == -2){
      thread_ptr->status = 0;
      thread_ptr->collected = thread_ptr->hunting_ptr->returnVal;
      if(value_ptr != NULL){
        *value_ptr = thread_ptr->collected;
      }
      deleteNode(thread_ptr->hunting_ptr);
    }
    else if(thread_ptr->status == -1 || thread_ptr->status == -2){
      setitimer(ITIMER_REAL, &it, NULL);
      nextThread();
      longjmp(thread_ptr->buf,1);
    }
    else{
      thread_ptr->collected = thread_ptr->hunting_ptr->returnVal;
      if(value_ptr != NULL){
        *value_ptr = thread_ptr->collected;
      }
      deleteNode(thread_ptr->hunting_ptr);
    }
  }
  return 0;
}

void deleteNode(node delete_ptr){
  setitimer(ITIMER_REAL, &zero_timer, &it);
  //cout << "deleting this thread"<< delete_ptr->TID << endl;
  thread_ptr->hunting_ptr == NULL;
  node temp_ptr = delete_ptr;
  node other_ptr = head_ptr;
  if(head_ptr == temp_ptr){
    if(temp_ptr->next == NULL){
      threads--;
    }
    else{
      head_ptr = head_ptr->next;
      threads--;
    }
  }
  else if(temp_ptr->next == NULL){
    while(other_ptr->next != temp_ptr){
      other_ptr = other_ptr->next;
    }
    other_ptr->next = NULL;
    threads--;
  }
  else if(temp_ptr->next != NULL){
    while(other_ptr->next != temp_ptr){
      other_ptr = other_ptr->next; 
    }
    other_ptr->next = temp_ptr->next;
    threads--;
  }
  free(temp_ptr->stack);
  free(temp_ptr);
  setitimer(ITIMER_REAL, &it, NULL);
  return;
}

//Good
void nextThread(){
  int counterL = 0;
  jmp_buf BUFFER;
  setjmp(BUFFER);
  if(counterL == 3){
    //cout << "Deadlock";
    exit(1);
  }
  if(thread_ptr->next == NULL){
    thread_ptr = head_ptr;
    counterL++;
  }
  else{
    thread_ptr = thread_ptr->next;
  }
  //cout << thread_ptr->TID << endl;
  if(thread_ptr->status != -1 && thread_ptr->status != -2){
    return;
  }
  if(thread_ptr->status == -1 || thread_ptr->status == -2){
    longjmp(BUFFER,0);
  }
}

int sem_init(sem_t *sem, int pshared, unsigned int value){
  if(sem == NULL){
    return -1;
  }
  Semaphore *my_sem = (Semaphore *) malloc(sizeof(struct Semaphore));
  sem->__align = (long int) my_sem;
  if(SEM_VALUE_MAX < value || value < 0){
    return -1;
  }
  my_sem->value = value;
  my_sem->status = 25;
  if(is_first == true){
    init();
  }
  return 0;
}
int sem_destroy(sem_t *sem){
  if(sem == NULL){
    return -1;
  }
  Semaphore *temp = (Semaphore*) sem->__align;
  if((temp->waitList.size() > 0) || (temp->status != 25)){
    return -1;
  }
  else{
    free(temp);
  }
  return 0;
}

int sem_wait(sem_t *sem){
  lock();
  if(sem == NULL){
    unlock();
    return -1;
  }
  Semaphore *temp = (Semaphore*) sem->__align;
  if(temp->status != 25){
    unlock();
    return -1;
  }
  if(temp->value > 0){
    --temp->value;
    unlock();
    return 0;
  }
  if(temp->value == 0){
    temp->waitList.push_back(thread_ptr);
    thread_ptr->status = -1;
    nextThread();
    longjmp(thread_ptr->buf,1);
  }
  return 0;
}
int sem_post(sem_t *sem){
  if(sem == NULL){
    unlock();
    return -1;
  }
  Semaphore *temp = (Semaphore*) sem->__align;
  if(temp->value >= SEM_VALUE_MAX){
    return -1;
  }
  if(temp->status != 25){
    unlock();
    return -1;
  }
  if(temp->value > 0){
    temp->value = temp->value + 1;
    unlock();
    return 0;
  }
  if(temp->value == 0){
    temp->value = temp->value + 1;
    if(temp->waitList.size() > 0){
      node RMV = temp->waitList.front();
      temp->waitList.erase(temp->waitList.begin());
      RMV->status = 0;
    }
    unlock();
  }
  return 0;
}