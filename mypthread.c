// List all group member's name: Jishan Desai, Malav Doshi
// username of iLab:
// iLab Server:

#include "mypthread.h"

// INITAILIZE ALL YOUR VARIABLES HERE
// YOUR CODE HERE
uint count = 1;
int thread_id = 1;
struct Queue *queue;
struct Queue *waitQueue;
struct Queue *exitQueue;
struct Queue *blockQueue;
struct scheduler* sched;
struct sigaction sa;
struct itimerval timer;
int first_run = 1;
int init = 1;
int run_sched = 0;
int fromExit = 0;
tcb* currentThread;
tcb* nextThreadToRun;
ucontext_t lib;
tcb* main_thread;
struct Node* curr;
struct Node* nextThread;

/*TCB initialization*/
tcb *initialize_tcb(){

    tcb* new_tcb = (tcb*)malloc(sizeof(tcb));
    new_tcb->id = thread_id++;
    new_tcb->status = 0;
    return new_tcb;
}

/*TIMER FUNCTIONS*/

void Handler(int signum){

    stopTimer();
    //run;
    schedule();
}

void setTimer(){
    memset (&sa, 0, sizeof (sa));
    sa.sa_handler = &Handler;
    sigaction (SIGPROF, &sa, NULL);
}

void startTimer(){

    timer.it_interval.tv_usec = 20000;
    timer.it_interval.tv_sec = 0;

    timer.it_value.tv_usec = 20000;
    timer.it_value.tv_sec = 0;

    setitimer(ITIMER_PROF, &timer, NULL);
}

void stopTimer(){
    timer.it_interval.tv_usec = 0;
    timer.it_interval.tv_sec = 0;

    timer.it_value.tv_usec = 0;
    timer.it_value.tv_sec = 0;

    setitimer(ITIMER_PROF, &timer, NULL);
}


/*SCHEDULER FUNCTIONS*/
void createScheduler(){

    waitQueue = (struct Queue*)malloc(sizeof(struct Queue));
    waitQueue->num_node=0;
    blockQueue = (struct Queue*)malloc(sizeof(struct Queue));
    blockQueue->num_node=0;
    queue = (struct Queue*)malloc(sizeof(struct Queue));
    queue->num_node = 0;
    exitQueue = (struct Queue*)malloc(sizeof(struct Queue));
    exitQueue->num_node = 0;
    sched = (struct scheduler*)malloc(sizeof(struct scheduler));
    sched->queue = queue;

    void* stack = malloc(sizeof(STACK_SIZE));

    getcontext(&sched->schedcontext);
    sched->schedcontext.uc_link = NULL;
    sched->schedcontext.uc_stack.ss_sp = stack;
    sched->schedcontext.uc_stack.ss_size = STACK_SIZE;
    sched->schedcontext.uc_stack.ss_flags = 0;

}


tcb* findThread(mypthread_t thread){
    struct Node* ptr = sched->queue->head;

    if(sched->queue->num_node == 0){
        return NULL;
    }

    if(ptr->thread_block->tid == thread){
        return ptr->thread_block;
    }
    int i = 1;
    while(i != sched->queue->num_node){
        if(ptr->thread_block->tid == thread){
            return ptr->thread_block;
        }
        ptr = ptr->next;
        i++;
    }
    if((ptr!=NULL) && (ptr->thread_block->tid == thread)){
        return ptr->thread_block;
    }
    return NULL;
}
tcb* findExitThread(mypthread_t thread){
    struct Node* ptr = exitQueue->head;
    if(exitQueue->num_node == 0){
        return NULL;
    }


    if(ptr->thread_block->tid == thread){
        return ptr->thread_block;
    }
    int i = 1;
    while(i != exitQueue->num_node){
        if(ptr->thread_block->tid == thread){
            return ptr->thread_block;
        }
        ptr = ptr->next;
        i++;
    }
    if((ptr!=NULL) && (ptr->thread_block->tid == thread)){
        return ptr->thread_block;
    }
    return NULL;
}

void enqueue_in_wait(struct Node* node){
    if(waitQueue->num_node == 0){
        waitQueue->head = node;
        waitQueue->tail = node;
        waitQueue->num_node++;
    }else{
        waitQueue->tail->next = node;
        waitQueue->tail = node;
        waitQueue->num_node++;
    }


}

/*
 * Dequeues that node with the same join_id
 * from the wait queue.
*/
struct Node* dequeue_from_wait(int join_id){

    struct Node* ptr = waitQueue->head;
    struct Node* prev = NULL;
    if((ptr!=NULL) && (ptr->thread_block->joinId == join_id)){
        waitQueue->head = NULL;
        waitQueue->tail = NULL;
        waitQueue->num_node -- ;
        return ptr;

    }

    while((ptr!=NULL) && (ptr->thread_block->joinId != join_id)){
        prev=ptr;
        ptr = ptr->next;
    }
    if(ptr==NULL)return NULL; //empty list or not found
    else{
        prev->next=ptr->next;
        ptr->next=NULL;
        return ptr;
    }
}

/*ENQUEUE */

void enqueue(struct Queue *this_queue, struct Node* node){

    node->next = NULL;

    if(this_queue->num_node == 0){

        this_queue->head = node;
        this_queue->tail = node;
        this_queue->num_node++;

        return;
    }
    else if(this_queue->num_node == 0 && fromExit){
        this_queue->head = node;
        this_queue->tail = node;
        this_queue->num_node++;
        return;
    }

    this_queue->tail->next = node;
    this_queue->tail = node;
    this_queue->num_node++;
}

struct Node* dequeue(struct Queue* this_queue){

    struct Node* ptr = sched->queue->head;

    if(this_queue->head == NULL){
        printf("Queue is NULL. Cannot dequeue() \n");
        return NULL;
    }

    if(this_queue->head->next == NULL){
        this_queue->tail = NULL;
        this_queue->head = NULL;
        this_queue->num_node -- ;
        return ptr;
    }
    if(this_queue->num_node == 2){
        this_queue->head = this_queue->head->next;
        this_queue->tail = this_queue->head;
        this_queue->num_node -- ;
        return ptr;
    }
    this_queue->head = this_queue->head->next;
    this_queue->num_node --;
    return ptr;
}


struct Node* removeNode(mypthread_t thread, struct Queue* this_queue){
    struct Node* ptr = this_queue->head;
    struct Node* prev = NULL;
    int i;

    while(ptr != NULL){//Get the particular node
        if(thread == ptr->thread_block->tid){
            break;
        }
        prev = ptr;
        ptr = ptr->next;
    }

    if(prev == NULL){//Head needs to be removed
        return dequeue(this_queue);
    }
    else if(this_queue->num_node == 1){//If the last node needs to be removed
        this_queue->num_node --;
        prev->next = NULL;
        this_queue->tail = prev;
        return ptr;
    }

    else{
        prev->next = ptr->next;
        this_queue->num_node --;
        return ptr;
    }

}

struct Node* peek(struct Queue* this_queue){
    if(this_queue->head != NULL){
        return this_queue->head;
    }
    return NULL;
}

tcb* getMainThread(){
    struct Node* ptr = sched->queue->head;

    while(ptr != NULL){
        if(ptr->thread_block->isMain){
            return ptr->thread_block;
        }
        ptr = ptr->next;
    }
    return NULL;
}



/* create a new thread */
int mypthread_create(mypthread_t * thread, pthread_attr_t * attr, void*(*function)(void*), void * arg){
    // create Thread Control Block
    // create and initialize the context of this thread
    // allocate space of stack for this thread to run
    // after everything is all set, push this thread int
    // YOUR CODE HERE
    fromExit = 0;
    stopTimer();
    tcb *exit_thread;

    if(init){
        getcontext(&lib);
        setTimer();
        createScheduler();
    }

    void *stack=malloc(STACK_SIZE);

    /*Create a tcb for new thread */

    void* func = (void(*)(void))function;

    tcb *new_tcb = initialize_tcb();


    getcontext(&new_tcb->tctx);
    new_tcb->tctx.uc_link = NULL;
    new_tcb->tctx.uc_stack.ss_sp = stack;
    new_tcb->tctx.uc_stack.ss_size = STACK_SIZE;
    new_tcb->tctx.uc_stack.ss_flags = 0;

    if(arg != NULL){
        makecontext(&new_tcb->tctx, func, 1, arg);
    }
    else{
        makecontext(&new_tcb->tctx, func, 0);
    }

    *thread = count;
    new_tcb->tid = count;
    count++;

    struct Node *new_node = (struct Node*)malloc(sizeof(struct Node));
    new_node->thread_block = new_tcb;
    sched_stcf(sched->queue, new_node);
    if(init){
        main_thread = initialize_tcb();
        main_thread->tctx = lib;
        main_thread->isMain = 1;
        struct Node* mainNode = (struct Node*) malloc(sizeof(struct Node));
        mainNode->thread_block = main_thread;
        enqueue(sched->queue, mainNode);
        init=0;
        startTimer();
    }

    

    return 0;
};

/* give CPU possession to other user-level threads voluntarily */
int mypthread_yield() {

// change thread state from Running to Ready
    tcb * curr_thread_block = sched->current;
    curr_thread_block->status = 0;

// save context of this thread to its thread control block
    if (getcontext(&(curr_thread_block->tctx))==-1){
        perror("getcontext failed");return 0;
    }
// switch from thread context to scheduler context
    if(getcontext(&(sched->schedcontext))==-1){
        perror("getcontext failed"); return 0;

    }
    setcontext(&(sched->schedcontext));
    //basically set a timer prematurely to stop executing the current thread

// YOUR CODE HERE
    return 0;
};

/* terminate a thread */
void mypthread_exit(void *value_ptr) {
// Deallocated any dynamic memory created when starting this thread
    stopTimer();
    if(currentThread == NULL){ //if current thread is null, change it to main thread.
        currentThread = getMainThread();
    }
    struct Node* WaitingThread = dequeue_from_wait(currentThread->tid);
    if(WaitingThread == NULL){ //Exit is called before join
        currentThread->retVal = value_ptr;
        currentThread->status = -1;
        struct Node *new_node = (struct Node*)malloc(sizeof(struct Node));
        new_node->thread_block = currentThread;
        enqueue(exitQueue, new_node);
        curr = dequeue(sched->queue);//Get the first thread in queue
        currentThread = curr->thread_block;//store thread_block it in a global tcb*
        run_sched++;
        startTimer();
        //currentThread->time++;
        swapcontext(&new_node->thread_block->tctx,&currentThread->tctx);
    }else{//Exit is called after join
        WaitingThread->thread_block->status = 0; //Wake up the thread
         sched_stcf(sched->queue, WaitingThread);//put in scheduler. So, it can run now.
        currentThread->retVal = value_ptr;
        currentThread->status = -1;
        if(value_ptr!=NULL)
            *WaitingThread->thread_block->ret_val_addr = value_ptr;
        struct Node *new_node = (struct Node*)malloc(sizeof(struct Node));
        new_node->thread_block = currentThread;
        enqueue(exitQueue, new_node);
        curr = dequeue(sched->queue);//Get the first thread in queue
        currentThread = curr->thread_block;//store thread_block it in a global tcb*
        run_sched++;
        startTimer();
        //currentThread->time++;
        swapcontext(&new_node->thread_block->tctx,&currentThread->tctx);
    }
   // schedule();
// YOUR CODE HERE
};


/* Wait for thread termination */
int mypthread_join(mypthread_t thread, void **value_ptr) {

//  wait for a specific thread to terminate
// de-allocate any dynamic memory created by the joining thread

    stopTimer();
    tcb *threadToJoin = findExitThread(thread);
    if(threadToJoin == NULL){
        threadToJoin = findThread(thread);
    }
    if(currentThread == NULL){//no current thread is running. So, main called join
        if(threadToJoin->status == -1){//Thread to join has terminated
            if(value_ptr!=NULL)
                *value_ptr = threadToJoin->retVal;
            removeNode(thread,exitQueue);
            
            schedule();
        }else{
            tcb * main = getMainThread();
            main->status=2;//wait state
            main->joinId = thread;
            main->ret_val_addr = value_ptr;

            struct Node * main_node = removeNode(0,sched->queue); //remove main thread from scheduler
            main_node->next=NULL;
            curr = dequeue(sched->queue);
            currentThread = curr->thread_block;
            enqueue_in_wait(main_node);//enqueue it in wait list.
            run_sched++;
            startTimer();
            main_node->thread_block->time++; //currentThread->time++;
            swapcontext(&main_node->thread_block->tctx,&currentThread->tctx);
            



        } 
       }else{
        if(threadToJoin->status == -1){//Thread to join has terminated
            if(value_ptr != NULL)
                *value_ptr=threadToJoin->retVal;
           
	    schedule();
        }else{
            currentThread->status=2;
            currentThread->joinId = thread;
            struct Node * curr_node = (struct Node*)malloc(sizeof(struct Node*));
            curr_node->thread_block = currentThread;
            enqueue_in_wait(curr_node);//enqueue it in wait list.
            curr = dequeue(sched->queue);
            currentThread = curr->thread_block;
            run_sched++;
            startTimer();
            curr_node->thread_block->time++; //currentThread->time++;
            swapcontext(&curr_node->thread_block->tctx,&currentThread->tctx);

        }

    }

    // YOUR CODE HERE
    return 0;
};
/* initialize the mutex lock */
int mypthread_mutex_init(mypthread_mutex_t *mutex,
                         const pthread_mutexattr_t *mutexattr) {
//initialize data structures for this mutex
    mutex = (mypthread_mutex_t*)malloc(sizeof(mypthread_mutex_t));
    mutex->tid = -1;
    mutex->isLocked = 0;
// YOUR CODE HERE
    return 0;
};

/* aquire the mutex lock */
int mypthread_mutex_lock(mypthread_mutex_t *mutex) {
    // use the built-in test-and-set atomic function to test the mutex
    // if the mutex is acquired successfully, enter the critical section
    // if acquiring mutex fails, push current thread into block list and //
    // context switch to the scheduler thread
    stopTimer();

    if(!mutex->isLocked){
        __sync_lock_test_and_set(&mutex->isLocked, 1);
        mutex->tid = currentThread->tid;
        startTimer();
        return 0;
    }
    else{//Add the node to blocked queue on mutex
        struct Node* node = (struct Node*)malloc(sizeof(struct Node));
        node->thread_block = currentThread;
        node->thread_block->status = 3;
        enqueue(blockQueue, node);
        curr = dequeue(sched->queue);
        currentThread = curr->thread_block;
        run_sched++;
        startTimer();
        node->thread_block->time++; //currentThread->time++;
        swapcontext(&node->thread_block->tctx,&currentThread->tctx);//Switch to scheduler

    }
    // YOUR CODE HERE
    return 0;
};

/* release the mutex lock */
int mypthread_mutex_unlock(mypthread_mutex_t *mutex) {
// Release mutex and make it available again.
// Put threads in block list to run queue
// so that they could compete for mutex later.

    stopTimer();

    if(mutex->isLocked){
        __sync_lock_release(&mutex->isLocked);
    }

    if(blockQueue->num_node == 0){
        struct Node* node = (struct Node*)malloc(sizeof(struct Node));
        node->thread_block = currentThread;
        node->thread_block->status = 0;
         sched_stcf(sched->queue,node);
        curr = dequeue(sched->queue);
        currentThread = curr->thread_block;
        run_sched++;
        startTimer();
        currentThread->time++;
        swapcontext(&node->thread_block->tctx,&currentThread->tctx);//Switch to scheduler

        return 0;
    }
    else{
        struct Node* ptr = blockQueue->head;
        while(ptr != NULL){
            ptr->thread_block->status = 0;
              sched_stcf(sched->queue, ptr);
            ptr = ptr->next;
        }

        struct Node* node = (struct Node*)malloc(sizeof(struct Node));
        node->thread_block = currentThread;
        node->thread_block->status = 0;
         sched_stcf(sched->queue, node);
        curr = dequeue(sched->queue);
        currentThread = curr->thread_block;
        run_sched++;
        startTimer();
        currentThread->time++;
        swapcontext(&node->thread_block->tctx,&currentThread->tctx);//Switch to scheduler
// YOUR CODE HERE
    }
//  startTimer();
    return 0;
}


/* destroy the mutex */
int mypthread_mutex_destroy(mypthread_mutex_t *mutex) {
// Deallocate dynamic memory created in mypthread_mutex_ini
    stopTimer();
    return 0;
};

/* scheduler */
static void schedule() {
// Every time when timer interrup happens, your thread library
// should be contexted switched from thread context to this
// schedule function

// Invoke different actual scheduling algorithms
// according to policy (STCF or MLFQ)

// if (sched == STCF)
// sched_stcf();
// else if (sched == MLFQ)
// sched_mlfq();
 if(currentThread == NULL){//Runs first time
        curr = dequeue(sched->queue);//Get the first thread in queue
        currentThread = curr->thread_block;//store thread_block it in a global tcb*
        sched->current = curr->thread_block;//store tcb in scheduler
        nextThread = peek(sched->queue);//peeks at the next thread, does not remove it from queue
        nextThreadToRun = nextThread->thread_block;//store the thread_block in a global tcb*
        startTimer();

        run_sched++;
        nextThreadToRun->time++; //currentThread->time++;
        swapcontext(&nextThreadToRun->tctx,&currentThread->tctx);
    }
    else{

        if(sched->queue->num_node == 0){
            run_sched++;
            startTimer();
            currentThread->time++;
            setcontext(&currentThread->tctx);
        }

        else{
            struct Node* node = (struct Node*)malloc(sizeof(struct Node));
            node->thread_block = currentThread;
            node->thread_block->time++;
             sched_stcf(sched->queue, node);
            nextThread = dequeue(sched->queue);
            nextThreadToRun = nextThread->thread_block;
            currentThread = nextThreadToRun;
            node->thread_block->time++;
            startTimer();
            swapcontext(&node->thread_block->tctx, &currentThread->tctx);
        }
    }

// schedule policy
#ifndef MLFQ
// Choose STCF
#else
    // Choose MLFQ
#endif

}

/* Preemptive SJF (STCF) scheduling algorithm */
static void sched_stcf(struct Queue *this_queue, struct Node* node) {
// Your own implementation of STCF
// (feel free to modify arguments and return types)
  if(node == NULL) return;// Should never happen, but just in case.
    node->next=NULL;
    if(this_queue->num_node == 0){
        this_queue->head = node;
        this_queue->tail = node;
        this_queue->num_node++;
        return;
    }
    struct Node * ptr = this_queue->head;
    struct Node * prev = NULL;
    while((ptr!=NULL)&& (node->thread_block->time > ptr->thread_block->time )){
        prev = ptr;
        ptr = ptr->next;
    }
    if(ptr == this_queue->head){ //Insert before head
        node->next = ptr;
        this_queue->head=node;
        this_queue->num_node++;
        return;
    }else if(ptr == NULL){ //Insert after last node
        prev->next=node;
        this_queue->tail = node;
        this_queue->num_node++;
        return;
    }else{  //Inserted in middle

        node->next = prev->next;
        prev->next = node;
        this_queue->num_node++;
        return;
    }

// YOUR CODE HERE
}

/* Preemptive MLFQ scheduling algorithm */
static void sched_mlfq() {
// Your own implementation of MLFQ
// (feel free to modify arguments and return types)

// YOUR CODE HERE
}


// Feel free to add any other functions you need