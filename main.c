#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <sys/time.h>
#include <time.h>
#include <pthread.h>
#include <stdint.h>

#include "tick.h"
#include "BrickPi.h"

#include <linux/i2c-dev.h>
#include <fcntl.h>

#define threads_num 5

#define M1_PORT		PORT_A						// Right
#define M2_PORT		PORT_B						// Left

#define B1_PORT		PORT_1                       // Touch button 1 BACK_THEN_RIGHT
#define B2_PORT		PORT_2                       // Touch button 2 BACK_THEN_LEFT
#define US_PORT		PORT_3                       // For the FW Ultrasonic sensor support, use port 3

#define BACK_DUR	50//5000000
#define SPEED_STD	200

#define NSEC_PER_SEC 1000000000ULL
static inline void timespec_add_us(struct timespec * t, uint64_t d)
{
	d *= 1000;
	d += t->tv_nsec;
	while (d >= NSEC_PER_SEC)
	{
		d -= NSEC_PER_SEC;
		t->tv_sec += 1;
	}
	t->tv_nsec = d;
}

// Compile Using:
// sudo gcc -o lab5 BrickPi.c -lrt -lm -pthread
// Run the compiled program using
// sudo ./lab5

pthread_t threads[threads_num];

enum commandenum {STILL, FORWARD, LEFT, RIGHT, BACK_THEN_LEFT, BACK_THEN_RIGHT};

struct order
{
	int urgent_level;
	int duration;
	enum commandenum command;
	int speed;
	int random;
} order_status;

/* struct periodic_data {
	int index;
	long period_us;
	int wcet_sim;
}; */

pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;

int result 			= 0;
int updateResult 	= 0;
int runMotor 		= 0;
int randomCmd 		= 0;
int stop 			= 0;
int motor1Speed 	= 0;
int motor2Speed 	= 0;
int button1Pushed 	= 0;
int button2Pushed 	= 0;
int USensor			= 0;


void *ultrasonic(void * arg);
void *motor (void * arg);
void *button (void * arg);
void *randomThread (void * arg);
void *periodic (void * arg);
void order_update(int u, int d, enum commandenum c, int s, int r);
int load();

int main()
{
	ClearTick();
	srand(time(NULL));

	struct timespec interval;

	clock_gettime(CLOCK_REALTIME, &interval);

	result = BrickPiSetup();
	printf("BrickPiSetup: %d\n", result);
 	if(result)
    {
    	printf("Error: BrickPiSetup");
    	return 0;
    }

	BrickPi.Address[0] = 1;
	BrickPi.Address[1] = 2;

	BrickPi.MotorEnable[M1_PORT] = 1;
	BrickPi.MotorEnable[M2_PORT] = 1;
	BrickPi.SensorType[US_PORT] = TYPE_SENSOR_ULTRASONIC_CONT;
	BrickPi.SensorType[B1_PORT] = TYPE_SENSOR_TOUCH;
	BrickPi.SensorType[B2_PORT] = TYPE_SENSOR_TOUCH;

	result = BrickPiSetupSensors();
	if(result)
	{
		printf("Error: BrickPiSetupSensors %d", result);
		return 0;
	}

	pthread_mutex_init(&m, 0);

	pthread_attr_t attr_motor, attr_ultrasonic, attr_button, attr_random, attr_periodic;

	pthread_attr_init(&attr_motor);
	pthread_attr_init(&attr_ultrasonic);
	pthread_attr_init(&attr_button);
	pthread_attr_init(&attr_random);
	pthread_attr_init(&attr_periodic);

	pthread_attr_setschedpolicy(&attr_motor, SCHED_RR);
	pthread_attr_setschedpolicy(&attr_ultrasonic, SCHED_RR);
	pthread_attr_setschedpolicy(&attr_button, SCHED_RR);
	pthread_attr_setschedpolicy(&attr_random, SCHED_RR);
	pthread_attr_setschedpolicy(&attr_periodic, SCHED_RR);

	pthread_attr_setinheritsched(&attr_motor, PTHREAD_EXPLICIT_SCHED);
	pthread_attr_setinheritsched(&attr_ultrasonic, PTHREAD_EXPLICIT_SCHED);
	pthread_attr_setinheritsched(&attr_button, PTHREAD_EXPLICIT_SCHED);
	pthread_attr_setinheritsched(&attr_random, PTHREAD_EXPLICIT_SCHED);
	pthread_attr_setinheritsched(&attr_periodic, PTHREAD_EXPLICIT_SCHED);

	struct sched_param sp1, sp2, sp3, sp4, sp5;

	sp1.sched_priority = 4;
	sp2.sched_priority = 5;
	sp3.sched_priority = 3;
	sp4.sched_priority = 2;
	sp5.sched_priority = 1;

	pthread_attr_setschedparam(&attr_motor, &sp1);
	pthread_attr_setschedparam(&attr_ultrasonic, &sp2);
	pthread_attr_setschedparam(&attr_button, &sp3);
	pthread_attr_setschedparam(&attr_random, &sp4);
	pthread_attr_setschedparam(&attr_periodic, &sp5);

	pthread_create(&threads[0], &attr_motor, motor, NULL);
	pthread_create(&threads[1], &attr_ultrasonic, ultrasonic, NULL);
	pthread_create(&threads[2], &attr_button, button, NULL);
	pthread_create(&threads[3], &attr_random , randomThread, NULL);
	pthread_create(&threads[4], &attr_periodic, periodic, NULL);

	while(1)
	{
		timespec_add_us(&interval, 10 * 1000);

		clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &interval, NULL);

		updateResult = BrickPiUpdateValues();

		if (!updateResult)
		{
			system("clear");
			printf("B1: %d \tM1: %d\n", button1Pushed, BrickPi.MotorSpeed[M1_PORT]);
			printf("B2: %d \tM2: %d\n", button2Pushed, BrickPi.MotorSpeed[M2_PORT]);
			printf("US: %d\n", USensor);
			printf("Cmd: %d\n", order_status.command);
			printf("Random: %d\n", order_status.random);
		}
	}

	stop = 1;
	int i;
	for (i = 0; i < threads_num; i++)
	{
		pthread_join(threads[i], NULL);
	}
}

void *ultrasonic (void * arg)
{
	cpu_set_t cpuset;
	CPU_ZERO(&cpuset);
	CPU_SET(0, &cpuset);
	pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);

	struct timespec next;
	clock_gettime(CLOCK_REALTIME, &next);

	while (!stop)
	{
		timespec_add_us(&next, 75 * 1000);	//75 ms

		clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &next, NULL);
		if (!updateResult)
		{
			USensor = BrickPi.Sensor[US_PORT];

			if (USensor <= 10)
			{
				pthread_mutex_lock(&m);
				order_update(6, 10, STILL, 0, 0);
				pthread_mutex_unlock(&m);
			}
		}
		//load();
	}

	pthread_exit(NULL);
}

void *motor (void * arg)
{
	cpu_set_t cpuset;
	CPU_ZERO(&cpuset);
	CPU_SET(0,&cpuset);
	pthread_setaffinity_np(pthread_self(),sizeof(cpu_set_t),&cpuset);

	struct timespec next;
	clock_gettime(CLOCK_REALTIME, &next);

	while(!stop)
	{
		timespec_add_us(&next, 50 * 1000); //50 ms
		clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &next, NULL);

		if(!updateResult)
		{
			pthread_mutex_lock(&m);
			if (order_status.duration > 0)
			{
				if (order_status.command == STILL)
				{
					BrickPi.MotorSpeed[M1_PORT] = order_status.speed;
					BrickPi.MotorSpeed[M2_PORT] = order_status.speed;
				}
				else if (order_status.command == FORWARD)
				{
					BrickPi.MotorSpeed[M1_PORT] = order_status.speed;
					BrickPi.MotorSpeed[M2_PORT] = order_status.speed;
				}
				else if (order_status.command == LEFT)
				{
					BrickPi.MotorSpeed[M1_PORT] = order_status.speed;
					BrickPi.MotorSpeed[M2_PORT] = -order_status.speed;
				}
				else if (order_status.command == RIGHT)
				{
					BrickPi.MotorSpeed[M1_PORT] = -order_status.speed;
					BrickPi.MotorSpeed[M2_PORT] = order_status.speed;
				}
				else if (order_status.command == BACK_THEN_LEFT)
				{
					if (order_status.duration >= BACK_DUR / 2)
					{
						BrickPi.MotorSpeed[M1_PORT] = -order_status.speed;
						BrickPi.MotorSpeed[M2_PORT] = -order_status.speed;
					} else {
						BrickPi.MotorSpeed[M1_PORT] = order_status.speed;
						BrickPi.MotorSpeed[M2_PORT] = -order_status.speed;
					}
				}
				else if (order_status.command == BACK_THEN_RIGHT)
				{
					if (order_status.duration >= BACK_DUR / 2)
					{
						BrickPi.MotorSpeed[M1_PORT] = -order_status.speed;
						BrickPi.MotorSpeed[M2_PORT] = -order_status.speed;
					} else {
						BrickPi.MotorSpeed[M1_PORT] = -order_status.speed;
						BrickPi.MotorSpeed[M2_PORT] = order_status.speed;
					}
				}
				order_status.duration--;
			}
			else
			{
				BrickPi.MotorSpeed[M1_PORT] = 0;
				BrickPi.MotorSpeed[M2_PORT] = 0;
				order_status.urgent_level = 0;
			}
			pthread_mutex_unlock(&m);
		}
		load();
	}

	pthread_exit(NULL);
}

void *button (void * arg)
{
	cpu_set_t cpuset;
	CPU_ZERO(&cpuset);
	CPU_SET(0,&cpuset);
	pthread_setaffinity_np(pthread_self(),sizeof(cpu_set_t),&cpuset);

	struct timespec next;
	clock_gettime(CLOCK_REALTIME, &next);

	while(!stop)
	{
		timespec_add_us(&next, 85 * 1000); //85ms
		clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &next, NULL);

		if(!updateResult)
		{
			button1Pushed = BrickPi.Sensor[B1_PORT];
			button2Pushed = BrickPi.Sensor[B2_PORT];

			if (button1Pushed)
			{
				pthread_mutex_lock(&m);
				order_update(3, 0, BACK_THEN_RIGHT, SPEED_STD, 0);
				pthread_mutex_unlock(&m);
			}
			if (button2Pushed)
			{
				pthread_mutex_lock(&m);
				order_update(3, 0, BACK_THEN_LEFT, SPEED_STD, 0);
				pthread_mutex_unlock(&m);
			}

		}
		//load();
	}
  pthread_exit(NULL);
}

void *randomThread (void * arg)
{
	cpu_set_t cpuset;
	CPU_ZERO(&cpuset);
	CPU_SET(0,&cpuset);
	pthread_setaffinity_np(pthread_self(),sizeof(cpu_set_t),&cpuset);

	struct timespec next;
	clock_gettime(CLOCK_REALTIME, &next);

	int randomTime;
	while(!stop)
	{
		randomTime = 5000 + (rand() % 5000); //random time between 5000-10000 ms
		timespec_add_us(&next, randomTime * 1000);

		clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &next, NULL);

		if(!updateResult)
		{
			pthread_mutex_lock(&m);
			order_update(2, 5000, rand() % 6, SPEED_STD, 1); //Send random cmd
			pthread_mutex_unlock(&m);

		}
		//load();
	}
  pthread_exit(NULL);
}

void *periodic (void * arg)
{
	cpu_set_t cpuset;
	CPU_ZERO(&cpuset);
	CPU_SET(0,&cpuset);
	pthread_setaffinity_np(pthread_self(),sizeof(cpu_set_t),&cpuset);

	struct timespec next;
	clock_gettime(CLOCK_REALTIME, &next);
	while(!stop)
	{
		timespec_add_us(&next, 243 * 1000);
		clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &next, NULL);

		if(!updateResult)
		{
			pthread_mutex_lock(&m);
			order_update(1, 1000, FORWARD, SPEED_STD, 0);
			pthread_mutex_unlock(&m);

		}
		//load();
	}
  pthread_exit(NULL);
}


// Function to change the urgent level, duration, command and speed
void order_update(int u, int d, enum commandenum c, int s, int r)
{
	if (u > order_status.urgent_level)
	{
		order_status.urgent_level = u;
		if (c == BACK_THEN_LEFT || c == BACK_THEN_RIGHT)
			order_status.duration = BACK_DUR;
		else
			order_status.duration = d;
		order_status.command = c;
		if (c == STILL)
			order_status.speed = 0;
		else
			order_status.speed = s;
		order_status.random = r;
	}
}

// function for load the CPU
int load()
{
	int i, num =1, primes = 0;
	while (num <= 2000)
	{
		i = 2;
		while(i <= num)
		{
			if (num % i ==0)
				break;
			i++;
		}
		if (i == num)
			primes++;
		num++;
	}
	return primes;
}
