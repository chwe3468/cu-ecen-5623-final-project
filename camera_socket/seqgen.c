/* ========================================================================== */
/*                                                                            */
// Sam Siewert, December 2017
//
// Sequencer Generic
//
// The purpose of this code is to provide an example for how to best
// sequence a set of periodic services for problems similar to and including
// the final project in real-time systems.
//
// For example: Service_1 for camera frame aquisition
//              Service_2 for image analysis and timestamping
//              Service_3 for image processing (difference images)
//              Service_4 for save time-stamped image to file service
//              Service_5 for save processed image to file service
//              Service_6 for send image to remote server to save copy
//              Service_7 for elapsed time in syslog each minute for debug
//
// At least two of the services need to be real-time and need to run on a single
// core or run without affinity on the SMP cores available to the Linux 
// scheduler as a group.  All services can be real-time, but you could choose
// to make just the first 2 real-time and the others best effort.
//
// For the standard project, to time-stamp images at the 1 Hz rate with unique
// clock images (unique second hand / seconds) per image, you might use the 
// following rates for each service:
//
// Sequencer - 1 Hz 
//                   [gives semaphores to all other services]
// Service_1 - 1 Hz  , every 10th Sequencer loop
//                   [buffers 3 images per second]
// Service_2 - 1 Hz  , every 30th Sequencer loop 
//                   [time-stamp middle sample image with cvPutText or header]
// Service_3 - 1 Hz, every 60th Sequencer loop
//                   [difference current and previous time stamped images]
//
// With the above, priorities by RM policy would be:
//
// Sequencer = RT_MAX	@ 1 Hz
// Servcie_1 = RT_MAX-1	@ 1 Hz
// Service_2 = RT_MAX-2	@ 1 Hz
// Service_3 = RT_MAX-3	@ 1 Hz
//
// Here are a few hardware/platform configuration settings on your Jetson
// that you should also check before running this code:
//
// 1) Check to ensure all your CPU cores on in an online state.
//
// 2) Check /sys/devices/system/cpu or do lscpu.
//
//    Tegra is normally configured to hot-plug CPU cores, so to make all
//    available, as root do:
//
//    echo 0 > /sys/devices/system/cpu/cpuquiet/tegra_cpuquiet/enable
//    echo 1 > /sys/devices/system/cpu/cpu1/online
//    echo 1 > /sys/devices/system/cpu/cpu2/online
//    echo 1 > /sys/devices/system/cpu/cpu3/online
//
// 3) Check for precision time resolution and support with cat /proc/timer_list
//
// 4) Ideally all printf calls should be eliminated as they can interfere with
//    timing.  They should be replaced with an in-memory event logger or at
//    least calls to syslog.
//
// 5) For simplicity, you can just allow Linux to dynamically load balance
//    threads to CPU cores (not set affinity) and as long as you have more
//    threads than you have cores, this is still an over-subscribed system
//    where RM policy is required over the set of cores.

// This is necessary for CPU affinity macros in Linux
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <pthread.h>
#include <sched.h>
#include <time.h>
#include <semaphore.h>
// File related
#include <fcntl.h>
#include <unistd.h>

#include <syslog.h>
#include <sys/time.h>
#include <sys/sysinfo.h>
#include <errno.h>
#include <signal.h>

// Network related
#include <netdb.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define USEC_PER_MSEC (1000)
#define NANOSEC_PER_SEC (1000000000)
#define NUM_CPU_CORES (1)
#define TRUE (1)
#define FALSE (0)
#define CHUTAO_IP_ADDR "10.0.0.89" // local
#define PORT "9000"
#define SAM_IP_ADDR "73.78.219.44" // Sam's public
#define NUM_THREADS (2+1)
#define BUF_SIZE 925696
//*****************************************************************************
//j
// ECEN 5623 related define and global variables: for problem 3
//
//*****************************************************************************

// volatile unsigned long long seqCnt=0;
// bool abortS1,abortS2, abortS3, abortS4, abortS5, abortS6,abortS7;
// bool threadcomplete[8];
// void Service_1(void *threadp);
// void Service_2(void *threadp);
// void Service_3(void *threadp);
// void Service_4(void *threadp);
// void Service_5(void *threadp);
// void Service_6(void *threadp);
// void Service_7(void *threadp);

#define FRAME_NUM 10
#define SEQ_NUM 10


#ifdef SEQ_HZ_LARGER_THAN_1
// Case where SEQ_HZ is larger than 1
#define SPEED_UP_MUL    10
#define SEQ_HZ  (1  *   SPEED_UP_MUL)
#define SEQ_IN_SEC (0)
#define SEQ_IN_MSEC (100)

#define SEV1_HZ (1 *   SPEED_UP_MUL)
#define SEV2_HZ (1 *   SPEED_UP_MUL)

#define SEV1_RATIO ((long long unsigned)(SEQ_HZ/SEV1_HZ))
#define SEV2_RATIO ((long long unsigned)(SEQ_HZ/SEV2_HZ))
#define SEQ_PERIOD_MSEC 100
#define SEV1_PERIOD_MSEC 100
#define SEV2_PERIOD_MSEC 100

#else

// Case where SEQ_HZ is equal to 1
#define SPEED_UP_MUL    1
#define SEQ_HZ  (1  *   SPEED_UP_MUL)
#define SEQ_IN_SEC (1)
#define SEQ_IN_MSEC (0)

#define SEV1_HZ (1 *   SPEED_UP_MUL)
#define SEV2_HZ (1 *   SPEED_UP_MUL)

#define SEV1_RATIO ((long long unsigned)(SEQ_HZ/SEV1_HZ))
#define SEV2_RATIO ((long long unsigned)(SEQ_HZ/SEV2_HZ))

#define SEQ_PERIOD_MSEC 1000
#define SEV1_PERIOD_MSEC 1000
#define SEV2_PERIOD_MSEC 1000

// Case where SEQ_HZ is larger than 1 is not developed yet
#endif


//*****************************************************************************
//
// Math function
//
//*****************************************************************************

int average(int arr[], uint32_t n)
{
    int i;
    int average = arr[0];
    for (i = 0; i < n; i++)
        average = average + arr[i];
    return (average/n);
}
// source: https://www.geeksforgeeks.org/c-program-find-largest-element-array/
int max(int arr[], uint32_t n)
{
    int i;
    int max = arr[0];
    for (i = 1; i < n; i++)
        if (arr[i] > max)
            max = arr[i];
    return max;
}

int min(int arr[], uint32_t n)
{
    int i;
    int min = arr[0];
    for (i = 1; i < n; i++)
        if (arr[i] < min)
            min = arr[i];
    return min;
}

void rebase_timeval(struct timeval * target_timeval, struct timeval* base_timeval)
{
    if(target_timeval->tv_usec < base_timeval->tv_usec)
    {
        target_timeval->tv_sec = target_timeval->tv_sec - base_timeval->tv_sec - 1;
        target_timeval->tv_usec = (target_timeval->tv_usec+1000000) - base_timeval->tv_usec;
    }
    else
    {
        target_timeval->tv_sec = target_timeval->tv_sec - base_timeval->tv_sec;
        target_timeval->tv_usec = target_timeval->tv_usec - base_timeval->tv_usec;
    }
    
}

int time_val_to_msec(struct timeval target_timeval)
{
    return (target_timeval.tv_sec*1000+target_timeval.tv_usec/1000);
}

int C_calculate(int sta_time, int end_time)
{
    return end_time - sta_time;
}
int D_calculate(int sta_time, int period)
{
    return sta_time + period;
}

//*****************************************************************************
//
// Function to print running history of the system, especially timestamp
//
//*****************************************************************************
// all time are in unit of msec
typedef struct service_info
{
	int sta_time;
	int end_time;
	int C;
    int T;
    int D;
}service_info_t;


typedef struct all_service_info
{
	service_info_t Seq[FRAME_NUM+1];
    service_info_t S1[FRAME_NUM];
    service_info_t S2[FRAME_NUM];
    service_info_t S3[FRAME_NUM];
}all_service_info_t;

all_service_info_t info;

void print_all_info(void)
{
    int i;
    // Service 1
    printf("For Service 1\n");
    printf("Service count\t\tD(msec)\t\t\tC(msec)\t\t\tT(msec)\t\t\t\n");
    for (i=0;i<FRAME_NUM;i++)
    {
        printf("%d\t\t\t%d\t\t\t%d\t\t\t%d\t\t\t\n", i+1, info.S1[i].D, info.S1[i].C, info.S1[i].T);
    }
    printf("\n");
    // Service 2
    printf("For Service 2\n");
    printf("Service count\t\tD(msec)\t\t\tC(msec)\t\t\tT(msec)\t\t\t\n");
    for (i=0;i<FRAME_NUM;i++)
    {
        printf("%d\t\t\t%d\t\t\t%d\t\t\t%d\t\t\t\n", i+1, info.S2[i].D, info.S2[i].C, info.S2[i].T);
    }
    printf("\n");

}
void print_all_info_to_csv(void)
{
    int i;
    char my_buf[256];
    /* open csv file */
    int fd = open("record.csv",
            O_WRONLY|O_CREAT,
            S_IRWXU|S_IRWXG|S_IRWXO);
    sprintf(my_buf,"Sevice Name, Count, Start Time, End Time, C, T, D\n");
    int write_size = write(fd, my_buf, strlen(my_buf));
    // Sequencer
    for (i=0;i<FRAME_NUM;i++)
    {
        sprintf(my_buf,"Seq, %d, %d, %d, %d, %d, %d\n",i+1,info.Seq[i].sta_time, info.Seq[i].end_time, info.Seq[i].C, info.Seq[i].T, info.Seq[i].D);

        write_size = write(fd, my_buf, strlen(my_buf));
    }

    // Service 1

    for (i=0;i<FRAME_NUM;i++)
    {
        sprintf(my_buf,"S1, %d, %d, %d, %d, %d, %d\n",i+1,info.S1[i].sta_time, info.S1[i].end_time, info.S1[i].C, info.S1[i].T, info.S1[i].D);
        write_size = write(fd, my_buf, strlen(my_buf));
    }
    
    // Service 2
    for (i=0;i<FRAME_NUM;i++)
    {
        sprintf(my_buf,"S2, %d, %d, %d, %d, %d, %d\n",i+1,info.S2[i].sta_time, info.S2[i].end_time, info.S2[i].C, info.S2[i].T, info.S2[i].D);
        write_size = write(fd, my_buf, strlen(my_buf));
    }

    close(fd);
}
//*****************************************************************************
//
// Capture related
//
//*****************************************************************************
int capture_write(int dev, char * filename);

//*****************************************************************************
//
// Timer related
//
//*****************************************************************************
pthread_mutex_t timer_flag;
pthread_mutex_t image_lock;
static inline void timespec_add( struct timespec *result,
                        const struct timespec *ts_1, const struct timespec *ts_2)
{
    result->tv_sec = ts_1->tv_sec + ts_2->tv_sec;
    result->tv_nsec = ts_1->tv_nsec + ts_2->tv_nsec;
    if( result->tv_nsec > 1000000000L ) {
        result->tv_nsec -= 1000000000L;
        result->tv_sec ++;
    }
}
static void timer_thread ()
{
    pthread_mutex_unlock(&timer_flag);
}

int delete_periodic_timer(timer_t * timerid)
{
    // delete timer
    if (timer_delete(*timerid) != 0)
    {
        printf("Error %d (%s) deleting timer!\n",errno,strerror(errno));
        return -1;
    }
    return 0;
}

int init_periodic_timer (timer_t * timerid, time_t second, long msec)
{
    // set up input arguments for timer_create()
    struct sigevent sev;
    memset(&sev,0,sizeof(struct sigevent));
    sev.sigev_notify = SIGEV_THREAD;
    sev.sigev_value.sival_ptr = NULL;
    sev.sigev_notify_function = timer_thread;

    // create timer
    if ( timer_create(CLOCK_REALTIME,&sev,timerid) != 0 )
    {
        printf("Error %d (%s) creating timer!\n",errno,strerror(errno));
        exit(1);
    }

    // set up input arguments for timer_settime()
    struct timespec start_time;
    if ( clock_gettime(CLOCK_REALTIME,&start_time) != 0 ) {
        printf("Error %d (%s) getting clock %d time\n",errno,strerror(errno),CLOCK_MONOTONIC);
        exit(1);
    }
    struct itimerspec new_value;
    new_value.it_interval.tv_sec = 1;
    new_value.it_interval.tv_nsec = 0;
    timespec_add(&new_value.it_value,&start_time,&new_value.it_interval);
    // set timer

    if( timer_settime(*timerid, TIMER_ABSTIME, &new_value, NULL ) != 0 ) {
        perror("Error setting timer");
        exit(1);
    }

    return 0;
}
//*****************************************************************************
//
// Network
//
//*****************************************************************************
struct addrinfo * res;
int sockfd;
void send_thread(char * filename)
{
    struct addrinfo * p = res;
    // inifite while loop for sending image
    int error_code = 0;

    /* Open image file at /var/tmp/cap_stamped.ppm */
    int fd = open(filename,
            O_RDONLY,
            S_IRWXU|S_IRWXG|S_IRWXO);


    /* Read all content */
    // create a buffer for sending message
    void * local_buf = malloc(BUF_SIZE);
    if(local_buf == NULL)
    {
        printf("no more space\n");
    }
    int num_read = 1;
    bool EOF_flag = false;
    ssize_t read_size;
    ssize_t total_read_size;// assume ssize_t never overflow
    while(EOF_flag==false)
    {
        read_size = read(fd, (local_buf+(num_read-1)*BUF_SIZE), (size_t) BUF_SIZE);
        if(read_size < 0)
        {
            printf("error while reading capture image");
        }
        // update total_size
        total_read_size = ((num_read-1)*BUF_SIZE)+read_size;
        if(read_size < BUF_SIZE)
        {
            EOF_flag = true;
        }
        else
        {
            num_read++;
            local_buf = realloc(local_buf,num_read*BUF_SIZE);
            if(local_buf == NULL)
            {
                printf("no more space\n");
            }
        }
    }
    /* Add '\n''#''EOF' at the end of buffer */
    total_read_size = total_read_size + 3;
    local_buf = realloc(local_buf,total_read_size);
    ((char *)local_buf)[total_read_size-3] = '\n';
    ((char *)local_buf)[total_read_size-2] = '#';
    ((char *)local_buf)[total_read_size-1] = 0x4;

    /* Close /var/tmp/cap_stamped.ppm */
    error_code = close(fd);

    /* check NULL */
    if(p == NULL){
        syslog(LOG_ERR, "client: client failed to connect: %s", strerror(errno));
    }

    if((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1){
            perror("client: socket");
    }

    /* Connect to Target ip else just dont send anything*/
    if(connect(sockfd, p->ai_addr, p->ai_addrlen) == -1)
    {
        close(sockfd);
        perror("client: connection failed");
    }
    else
    {
        /* Send image to Sam over TCP */
        ssize_t send_size = send(sockfd,local_buf,total_read_size,0);
        if (send_size<0)
        {
            printf("send wrong\n");
        }
        syslog(LOG_USER, "Image sent:send_size = %ld",send_size);
    }


    /* Send image to Sam over TCP */
    ssize_t send_size = send(sockfd,local_buf,total_read_size,0);
    if(send_size < 0)
    {
        printf("error while reading capture image");
    }
    syslog(LOG_USER, "Image sent:send_size = %ld",send_size);

    close(sockfd);
    // free local_buf
    free(local_buf);

}

int abortTest=FALSE;
int abortS1=FALSE, abortS2=FALSE, abortS3=FALSE;
sem_t semS1, semS2, semS3;
struct timeval start_time_val;

typedef struct
{
    int threadIdx;
    unsigned long long sequencePeriods;
} threadParams_t;


void *Sequencer(void *threadp);

void *Service_1(void *threadp);
void *Service_2(void *threadp);
double getTimeMsec(void);
void print_scheduler(void);


void main(void)
{
    /********************************************************************************/
    /**************************** Network section ***********************************/

    // Use getaddrinfo to get socket_addr
    struct addrinfo hints;
    memset(&hints,0,sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    //CHUTAO_IP_ADDR
    int error_code = getaddrinfo(NULL, PORT, &hints, &res);
    // Check for error
    if (error_code != 0)
    {
        // Use errno to print error
        perror("getaddrinfo error");
        // Use gai_strerror
        printf("%s",gai_strerror(error_code));
        exit(1);
    }

    /********************************************************************************/
    struct timeval current_time_val;
    int i, rc, scope;
    cpu_set_t threadcpu;
    pthread_t threads[NUM_THREADS];
    threadParams_t threadParams[NUM_THREADS];
    pthread_attr_t rt_sched_attr[NUM_THREADS];
    int rt_max_prio, rt_min_prio;
    struct sched_param rt_param[NUM_THREADS];
    struct sched_param main_param;
    pthread_attr_t main_attr;
    pid_t mainpid;
    cpu_set_t allcpuset;

    printf("Starting Sequencer Demo\n");
    gettimeofday(&start_time_val, (struct timezone *)0);
    gettimeofday(&current_time_val, (struct timezone *)0);
    syslog(LOG_CRIT, "Sequencer Application Started\n");
    syslog(LOG_CRIT, "Sequencer @ sec=%d, msec=%d\n", (int)(current_time_val.tv_sec-start_time_val.tv_sec), (int)current_time_val.tv_usec/USEC_PER_MSEC);

   printf("System has %d processors configured and %d available.\n", get_nprocs_conf(), get_nprocs());

   CPU_ZERO(&allcpuset);

   for(i=0; i < NUM_CPU_CORES; i++)
       CPU_SET(i, &allcpuset);

   printf("Using CPUS=%d from total available.\n", CPU_COUNT(&allcpuset));


    // initialize the sequencer semaphores
    //
    if (sem_init (&semS1, 0, 0)) { printf ("Failed to initialize S1 semaphore\n"); exit (-1); }
    if (sem_init (&semS2, 0, 0)) { printf ("Failed to initialize S2 semaphore\n"); exit (-1); }
    if (sem_init (&semS3, 0, 0)) { printf ("Failed to initialize S3 semaphore\n"); exit (-1); }

    mainpid=getpid();

    rt_max_prio = sched_get_priority_max(SCHED_FIFO);
    rt_min_prio = sched_get_priority_min(SCHED_FIFO);

    rc=sched_getparam(mainpid, &main_param);
    main_param.sched_priority=rt_max_prio;
    rc=sched_setscheduler(getpid(), SCHED_FIFO, &main_param);
    if(rc < 0) perror("main_param");
    print_scheduler();


    pthread_attr_getscope(&main_attr, &scope);

    if(scope == PTHREAD_SCOPE_SYSTEM)
      printf("PTHREAD SCOPE SYSTEM\n");
    else if (scope == PTHREAD_SCOPE_PROCESS)
      printf("PTHREAD SCOPE PROCESS\n");
    else
      printf("PTHREAD SCOPE UNKNOWN\n");

    printf("rt_max_prio=%d\n", rt_max_prio);
    printf("rt_min_prio=%d\n", rt_min_prio);

    for(i=0; i < NUM_THREADS; i++)
    {

      CPU_ZERO(&threadcpu);
      CPU_SET(3, &threadcpu);

      rc=pthread_attr_init(&rt_sched_attr[i]);
      rc=pthread_attr_setinheritsched(&rt_sched_attr[i], PTHREAD_EXPLICIT_SCHED);
      rc=pthread_attr_setschedpolicy(&rt_sched_attr[i], SCHED_FIFO);
      //rc=pthread_attr_setaffinity_np(&rt_sched_attr[i], sizeof(cpu_set_t), &threadcpu);

      rt_param[i].sched_priority=rt_max_prio-i;
      pthread_attr_setschedparam(&rt_sched_attr[i], &rt_param[i]);

      threadParams[i].threadIdx=i;
    }
   
    printf("Service threads will run on %d CPU cores\n", CPU_COUNT(&threadcpu));

    // Create Service threads which will block awaiting release for:
    //

    // Servcie_1 = RT_MAX-1	@ 3 Hz
    //
    rt_param[1].sched_priority=rt_max_prio-1;
    pthread_attr_setschedparam(&rt_sched_attr[1], &rt_param[1]);
    rc=pthread_create(&threads[1],               // pointer to thread descriptor
                      &rt_sched_attr[1],         // use specific attributes
                      //(void *)0,               // default attributes
                      Service_1,                 // thread function entry point
                      (void *)&(threadParams[1]) // parameters to pass in
                     );
    if(rc < 0)
        perror("pthread_create for service 1");
    else
        printf("pthread_create successful for service 1\n");


    // Service_2 = RT_MAX-2	@ 1 Hz
    //
    rt_param[2].sched_priority=rt_max_prio-2;
    pthread_attr_setschedparam(&rt_sched_attr[2], &rt_param[2]);
    rc=pthread_create(&threads[2], &rt_sched_attr[2], Service_2, (void *)&(threadParams[2]));
    if(rc < 0)
        perror("pthread_create for service 2");
    else
        printf("pthread_create successful for service 2\n");




    // Wait for service threads to initialize and await release by sequencer.
    //
    // Note that the sleep is not necessary of RT service threads are created wtih 
    // correct POSIX SCHED_FIFO priorities compared to non-RT priority of this main
    // program.
    //
    // usleep(1000000);
 
    // Create Sequencer thread, which like a cyclic executive, is highest prio
    printf("Start sequencer\n");
    threadParams[0].sequencePeriods=SEQ_NUM;

    // Sequencer = RT_MAX	@ 30 Hz
    //
    rt_param[0].sched_priority=rt_max_prio;
    pthread_attr_setschedparam(&rt_sched_attr[0], &rt_param[0]);
    rc=pthread_create(&threads[0], &rt_sched_attr[0], Sequencer, (void *)&(threadParams[0]));
    if(rc < 0)
        perror("pthread_create for sequencer service 0");
    else
        printf("pthread_create successful for sequeencer service 0\n");


    for(i=0;i<NUM_THREADS;i++)
        pthread_join(threads[i], NULL);
    
    
    // freeaddrinfo so that no memory leak

    freeaddrinfo(res);
    print_all_info();

    print_all_info_to_csv();

    printf("\nTEST COMPLETE\n");
}


void *Sequencer(void *threadp)
{
    struct timeval current_time_val;
    struct timeval sta_timeval;
    struct timeval end_timeval;
    capture_write(0,"test_image.ppm");
    double current_time;
    double residual;
    int rc, delay_cnt=0;
    unsigned long long seqCnt=0;
    threadParams_t *threadParams = (threadParams_t *)threadp;

    gettimeofday(&current_time_val, (struct timezone *)0);
    syslog(LOG_CRIT, "Sequencer thread @ sec=%d, msec=%d\n", (int)(current_time_val.tv_sec-start_time_val.tv_sec), (int)current_time_val.tv_usec/USEC_PER_MSEC);
    printf("Sequencer thread @ sec=%d, msec=%d\n", (int)(current_time_val.tv_sec-start_time_val.tv_sec), (int)current_time_val.tv_usec/USEC_PER_MSEC);


    pthread_mutex_init(&timer_flag, NULL);
    pthread_mutex_lock(&timer_flag);
    pthread_mutex_init(&image_lock, NULL);
    pthread_mutex_lock(&image_lock);
    // Initialize the timer for period PERIOD_T sec
    timer_t timer_id = NULL;
    int error_code = init_periodic_timer(&timer_id,SEQ_IN_SEC,SEQ_PERIOD_MSEC);
    if(error_code < 0)
    {
        printf("cannot create periodic timer");
        while(1);
    }
    do
    {
        pthread_mutex_lock(&timer_flag);

        gettimeofday(&sta_timeval, (struct timezone *)0);
        rebase_timeval(&sta_timeval,&start_time_val);
        info.Seq[seqCnt].sta_time = time_val_to_msec(sta_timeval);
        info.Seq[seqCnt].T = SEQ_PERIOD_MSEC;
        info.Seq[seqCnt].D = D_calculate(info.Seq[seqCnt].sta_time,SEQ_PERIOD_MSEC);


        if(delay_cnt > 1) printf("Sequencer looping delay %d\n", delay_cnt);


        // Release each service at a sub-rate of the generic sequencer rate

        // Servcie_1 = RT_MAX-1	@ 1 Hz
        if((seqCnt % SEV1_RATIO) == 0) sem_post(&semS1);

        // Service_2 = RT_MAX-2	@ 1 Hz
        if((seqCnt % SEV2_RATIO) == 0) sem_post(&semS2);

        gettimeofday(&end_timeval, (struct timezone *)0);
        rebase_timeval(&end_timeval,&start_time_val);
        info.Seq[seqCnt].end_time = time_val_to_msec(end_timeval);
        info.Seq[seqCnt].C = C_calculate(info.Seq[seqCnt].sta_time, info.Seq[seqCnt].end_time);

        seqCnt++;

    } while(!abortTest && (seqCnt < threadParams->sequencePeriods));

    sem_post(&semS1); sem_post(&semS2); sem_post(&semS3);
    
    abortS1=TRUE; abortS2=TRUE; abortS3=TRUE;
    
    // delete the timer for period 10 sec
    if (timer_id == NULL)
    {
        syslog(LOG_USER, "Cannot Delete timer");
        exit(1);
    }
    
    error_code = delete_periodic_timer(&timer_id);

    pthread_exit((void *)0);
}



void *Service_1(void *threadp)
{
    struct timeval current_time_val;
    double current_time;
    unsigned long long S1Cnt=0;
    threadParams_t *threadParams = (threadParams_t *)threadp;

    gettimeofday(&current_time_val, (struct timezone *)0);
    syslog(LOG_CRIT, "Frame Sampler thread @ sec=%d, usec=%d\n", (int)(current_time_val.tv_sec-start_time_val.tv_sec), (int)current_time_val.tv_usec/USEC_PER_MSEC);
    printf("Frame Sampler thread @ sec=%d, usec=%d\n", (int)(current_time_val.tv_sec-start_time_val.tv_sec), (int)current_time_val.tv_usec/USEC_PER_MSEC);

    struct timeval sta_timeval;
    struct timeval end_timeval;
    while(!abortS1)
    {
        sem_wait(&semS1);
        gettimeofday(&sta_timeval, (struct timezone *)0);
        rebase_timeval(&sta_timeval,&start_time_val);
        info.S1[S1Cnt].sta_time = time_val_to_msec(sta_timeval);
        info.S1[S1Cnt].T = SEV1_PERIOD_MSEC;
        info.S1[S1Cnt].D = D_calculate(info.S1[S1Cnt].sta_time,SEV1_PERIOD_MSEC);

        // workload here
        char filename[30];
        sprintf(filename, "./images/cap_%06lld.ppm",S1Cnt);
        capture_write(0, filename);
        pthread_mutex_unlock(&image_lock);

        gettimeofday(&end_timeval, (struct timezone *)0);
        rebase_timeval(&end_timeval,&start_time_val);
        info.S1[S1Cnt].end_time = time_val_to_msec(end_timeval);
        info.S1[S1Cnt].C = C_calculate(info.S1[S1Cnt].sta_time, info.S1[S1Cnt].end_time);
        S1Cnt++;
/*        syslog(LOG_CRIT, "Frame Sampler release %llu @ sec=%d, msec=%d\n", S1Cnt, 
            (int)(current_time_val.tv_sec-start_time_val.tv_sec), (int)current_time_val.tv_usec/USEC_PER_MSEC);*/
    }

    pthread_exit((void *)0);
}


void *Service_2(void *threadp)
{
    struct timeval current_time_val;
    double current_time;
    unsigned long long S2Cnt=0;
    threadParams_t *threadParams = (threadParams_t *)threadp;

    gettimeofday(&current_time_val, (struct timezone *)0);
    syslog(LOG_CRIT, "Time-stamp with Image Analysis thread @ sec=%d, usec=%d\n", (int)(current_time_val.tv_sec-start_time_val.tv_sec), (int)current_time_val.tv_usec/USEC_PER_MSEC);
    printf("Time-stamp with Image Analysis thread @ sec=%d, usec=%d\n", (int)(current_time_val.tv_sec-start_time_val.tv_sec), (int)current_time_val.tv_usec/USEC_PER_MSEC);

    struct timeval sta_timeval;
    struct timeval end_timeval;
    while(!abortS2)
    {
        sem_wait(&semS2);
        pthread_mutex_lock(&image_lock);
        gettimeofday(&sta_timeval, (struct timezone *)0);
        rebase_timeval(&sta_timeval,&start_time_val);
        
        info.S2[S2Cnt].sta_time = time_val_to_msec(sta_timeval);
        info.S2[S2Cnt].T = SEV2_PERIOD_MSEC;
        info.S2[S2Cnt].D = info.S1[S2Cnt].D;
        

        // workload here
        char filename[30];
        sprintf(filename, "./images/cap_%06lld.ppm",S2Cnt);
        send_thread(filename);

        gettimeofday(&end_timeval, (struct timezone *)0);
        rebase_timeval(&end_timeval,&start_time_val);
        info.S2[S2Cnt].end_time = time_val_to_msec(end_timeval);
        info.S2[S2Cnt].C = C_calculate(info.S2[S2Cnt].sta_time, info.S2[S2Cnt].end_time);
        S2Cnt++;
        //syslog(LOG_CRIT, "Time-stamp with Image Analysis release %llu @ sec=%d, msec=%d\n", S2Cnt, (int)(current_time_val.tv_sec-start_time_val.tv_sec), (int)current_time_val.tv_usec/USEC_PER_MSEC);
    }

    pthread_exit((void *)0);    
}



double getTimeMsec(void)
{
  struct timespec event_ts = {0, 0};

  clock_gettime(CLOCK_MONOTONIC, &event_ts);
  return ((event_ts.tv_sec)*1000.0) + ((event_ts.tv_nsec)/1000000.0);
}


void print_scheduler(void)
{
   int schedType;

   schedType = sched_getscheduler(getpid());

   switch(schedType)
   {
       case SCHED_FIFO:
           printf("Pthread Policy is SCHED_FIFO\n");
           break;
       case SCHED_OTHER:
           printf("Pthread Policy is SCHED_OTHER\n"); exit(-1);
         break;
       case SCHED_RR:
           printf("Pthread Policy is SCHED_RR\n"); exit(-1);
           break;
       default:
           printf("Pthread Policy is UNKNOWN\n"); exit(-1);
   }
}


