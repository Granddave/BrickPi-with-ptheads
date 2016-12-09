/*
*TODO: random function that will take a value between 0 to 5
* 		Create a "if statement" between all values that takes
* 		care of all the commandenum, STILL --> BACK_THEN_RIGHT
* 		
* 		Create a periodic function that will give command forward

*/

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <pthread.h>

#include "tick.h"
#include "BrickPi.h"
 
#include <linux/i2c-dev.h>  
#include <fcntl.h>

#define threads_num 3

#define M1_PORT		PORT_A						// Right
#define M2_PORT		PORT_B						// Left

#define B1_PORT		PORT_1                       // Touch button 1 BACK_THEN_RIGHT
#define B2_PORT		PORT_2                       // Touch button 2 BACK_THEN_LEFT
#define US_PORT		PORT_3                       // For the FW Ultrasonic sensor support, use port 3

#define BACK_DUR	10000//5000000
#define SPEED_STD	200

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
} order_status;

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
void order_update(int u, int d, enum commandenum c, int s);
int load();

int main()
{  
	ClearTick();

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
	
	pthread_attr_t attr_motor;
	pthread_attr_t attr_ultrasonic;
	pthread_attr_t attr_button;
	pthread_attr_t attr_random;
	pthread_attr_t attr_periodic;
	
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
	
	sp1.sched_priority = 5;
	sp2.sched_priority = 4;
	sp3.sched_priority = 3;
	sp4.sched_priority = 1;
	sp5.sched_priority = 1;
	
	pthread_attr_setschedparam(&attr_motor, &sp1);
	pthread_attr_setschedparam(&attr_ultrasonic, &sp2);
	pthread_attr_setschedparam(&attr_button, &sp3);
	pthread_attr_setschedparam(&attr_random, &sp4);
	pthread_attr_setschedparam(&attr_periodic, &sp5);
	//osv
	
	pthread_create(&threads[0], &attr_motor, motor, NULL);
	pthread_create(&threads[1], &attr_ultrasonic, ultrasonic, NULL);
	pthread_create(&threads[2], &attr_button, button, NULL);
	//pthread_create(&threads[3], NULL,##Funktion##, NULL);
	//pthread_create(&threads[4], NULL,##Funktion##, NULL);
	
	while(1)
	{
		usleep(10000);
		updateResult = BrickPiUpdateValues();
		
		if (!updateResult)
		{
			system("clear");
			printf("B1: %d \tM1: %d\n", button1Pushed, BrickPi.MotorSpeed[M1_PORT]);
			printf("B2: %d \tM2: %d\n", button2Pushed, BrickPi.MotorSpeed[M2_PORT]);
			printf("US: %d\n", USensor);
			printf("Cmd: %d\n", order_status.command);
		} 
	}

	stop = 1;

	//pthread_join(threads, NULL);
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
	
	//int i = (long int) arg;	// unused for now
	struct timespec interval;

	
	while (!stop)
	{
		if (!updateResult)
		{
			USensor = BrickPi.Sensor[US_PORT];
			
			if (USensor <= 10)
				order_update(6, 10, STILL, 0);
			
			
			/*USensor = BrickPi.Sensor[US_PORT];
			if(USensor!=255 && USensor!=127)
			{
				if (USensor <= 10)
					BrickPi.MotorSpeed[M1_PORT] = 0;
					BrickPi.MotorSpeed[M2_PORT] = 0;
				else
					BrickPi.MotorSpeed[M1_PORT] = order_status.speed;
					BrickPi.MotorSpeed[M2_PORT] = order_status.speed;
			}*/
		}
		usleep(500);
	}	
	
	pthread_exit(NULL);
}

void *motor (void * arg)
{
	cpu_set_t cpuset;
	CPU_ZERO(&cpuset);
	CPU_SET(0,&cpuset);
	pthread_setaffinity_np(pthread_self(),sizeof(cpu_set_t),&cpuset);
	
	//int val = (long int)arg;	// unused for now

	while(!stop)
	{
		if(!updateResult){
			
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
		}
		usleep(500);
	}
  
	pthread_exit(NULL);
}

void *button (void * arg)
{
	cpu_set_t cpuset;
	CPU_ZERO(&cpuset);
	CPU_SET(0,&cpuset);
	pthread_setaffinity_np(pthread_self(),sizeof(cpu_set_t),&cpuset);

	//int val = (long int)arg;	// unused for now

	while(!stop)
	{
		if(!updateResult)
		{	
			button1Pushed = BrickPi.Sensor[B1_PORT];
			button2Pushed = BrickPi.Sensor[B2_PORT];

			if (button1Pushed)
				order_update(3, 0, BACK_THEN_RIGHT, SPEED_STD);
			if (button2Pushed)
				order_update(3, 0, BACK_THEN_LEFT, SPEED_STD);
		}	
		usleep(500);	
	}
  pthread_exit(NULL);
}

void order_update(int u, int d, enum commandenum c, int s)
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
	}
}

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
