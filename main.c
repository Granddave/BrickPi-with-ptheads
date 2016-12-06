#define _GNU_SOURCE

#include <stdio.h>
#include <math.h>
#include <time.h>
#include <pthread.h>

#include "tick.h"
#include "BrickPi.h"
 
#include <linux/i2c-dev.h>  
#include <fcntl.h>

#define threads_num 1

#define M1_PORT		PORT_A
#define M2_PORT		PORT_B

#define B1_PORT		PORT_1                       // Touch button 1
#define B2_PORT		PORT_2                       // Touch button 2
#define US_PORT		PORT_3                       // For the FW Ultrasonic sensor support, use port 3

// Compile Using:
// sudo gcc -o lab5 lab5.c -lrt -lm -pthread
// Run the compiled program using
// sudo ./lab5

pthread_t threads;//[threads_num];

int result = 0;
int updateResult = 0;
int runMotor = 0;
int stop = 0;
int motor1Speed = 0;
int motor2Speed = 0;
int button1Pushed = 0;
int button2Pushed = 0;


void *ultrasonic(void * arg);

int main()
{
	ClearTick();

	result = BrickPiSetup();
	printf("BrickPiSetup: %d\n", result);
 	if(result)
    {
    	printf("Error: BrickPiSetup")
    	return 0;
    }
	
	BrickPi.Address[0] = 1;
	BrickPi.Address[1] = 2;

	BrickPi.SensorType[US_PORT] = TYPE_SENSOR_ULTRASONIC_CONT;
	result = BrickPiSetupSensors();
	if(result)
	{
		printf("Error: BrickPiSetupSensors %d", result);
		return 0;
	}

	int w = 0;
	//pthread_create(&threads[0], NULL, motor, NULL);
	pthread_create(&threads, NULL, ultrasonic, (void *) w);
	//pthread_create(&threads[2], NULL,##Funktion##, NULL);
	//pthread_create(&threads[3], NULL,##Funktion##, NULL);
	//pthread_create(&threads[4], NULL,##Funktion##, NULL);
	
	while(1)
	{
		usleep(10000);
		updateResult = BrickPiUpdateValues();
		if (!updateResult)
			printf("%d\n", updateResult);

	}

	stop = 1;

	pthread_join(threads, NULL);
	/*for (int i = 0; i < threads_num; i++)
	{
		pthread_join(threads[i], NULL);
	}*/
}

void *ultrasonic (void * arg)
{
	int val;
	cpu_set_t cpuset;
	CPU_ZERO(&cpuset);
	CPU_SET(0, &cpuset);
	pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);

	//int i = (long int) arg;	// unused for now
	struct timespec interval;

	usleep(10000);
	
	while (!stop)
	{
		usleep(10000);
		if (!updateResult)
		{
			val = BrickPi.Sensor[US_PORT];
			if(val!=255 && val!=127)
			{
				if (val <= 10)
					runMotor = 0;
				else
					runMotor = 1;
			}
		printf("sensor = %d\n", val);
		}
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
			if(runMotor) 
			{
				if button1Pushed
					BrickPi.MotorSpeed[M1_PORT]=motor1Speed;		// 200 från början 
				else 
					BrickPi.MotorSpeed[M1_PORT]=-motor1Speed;		
				if button2Pushed
					BrickPi.MotorSpeed[M2_PORT]=motor2Speed;
				else
					BrickPi.MotorSpeed[M2_PORT]=-motor2Speed;

				BrickPiUpdateValues();
			} else {
				BrickPi.MotorSpeed[M1_PORT]=0;
				BrickPi.MotorSpeed[M2_PORT]=0;
				BrickPiUpdateValues();
			}
		}
	usleep(10000);
	}
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
		}
		
		usleep(10000);
	}
}
