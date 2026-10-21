/*
 * FreeRTOS V202212.00
 * Copyright (C) 2020 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * https://www.FreeRTOS.org
 * https://github.com/FreeRTOS
 *
 */

 /* Standard includes. */
#include <stdio.h>
#include <conio.h>
#include <stdint.h>

/* Kernel includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "semphr.h"

/* Priorities at which the tasks are created. */
#define mainQUEUE_RECEIVE_TASK_PRIORITY    ( tskIDLE_PRIORITY + 2 )
#define mainQUEUE_SEND_TASK_PRIORITY       ( tskIDLE_PRIORITY + 1 )

/* The rate at which data is sent to the queue.  The times are converted from
 * milliseconds to ticks using the pdMS_TO_TICKS() macro. */
#define mainTASK_SEND_FREQUENCY_MS         pdMS_TO_TICKS( 200UL )
#define mainTIMER_SEND_FREQUENCY_MS        pdMS_TO_TICKS( 2000UL )

 /* The number of items the queue can hold at once. */
#define mainQUEUE_LENGTH                   ( 2 )

/* The values sent to the queue receive task from the queue send task and the
 * queue send software timer respectively. */
#define mainVALUE_SENT_FROM_TASK           ( 100UL )
#define mainVALUE_SENT_FROM_TIMER          ( 200UL )

 /* This demo allows for users to perform actions with the keyboard. */
#define mainNO_KEY_PRESS_VALUE             ( -1 )
#define mainRESET_TIMER_KEY                ( 'r' )

/*-----------------------------------------------------------*/

static void prvQueueReceiveTask(void* pvParameters);
static void prvQueueSendTask(void* pvParameters);
static void prvAlarmTask(void* pvParameters);

/*
 * The callback function executed when the software timer expires.
 */
static void prvQueueSendTimerCallback(TimerHandle_t xTimerHandle);

/*-----------------------------------------------------------*/



typedef struct
{
    uint32_t measurementNo;
    float temperature;
} Measurement;

static QueueHandle_t xQueue = NULL;
static SemaphoreHandle_t xAlarmSemaphore = NULL;
static SemaphoreHandle_t xConsoleMutex = NULL;
typedef enum // <--- tutaj modyfikacja.
{
    alarmTemperature, // <--- tutaj modyfikacja.
    sensorFailure, // <--- tutaj modyfikacja.
    sensorRecovery // <--- tutaj modyfikacja.
} AlarmEvent_t; // <--- tutaj modyfikacja.

static volatile AlarmEvent_t xAlarmEvent = alarmTemperature; // <--- tutaj modyfikacja.
static volatile BaseType_t xSensorFailureDetected = pdFALSE; // <--- tutaj modyfikacja.
/*-----------------------------------------------------------*/


void main_blinky(void)
{
    printf("\r\nTemperature control\r\n\r\n");

    /* Create the queue for Measurement structures. */
    xQueue = xQueueCreate(1, sizeof(Measurement)); // <--- tutaj modyfikacja.

    /* Create a counting semaphore and a mutex. */
    xAlarmSemaphore = xSemaphoreCreateCounting(5, 0);
    xConsoleMutex = xSemaphoreCreateMutex();

    if ((xQueue != NULL) &&
        (xAlarmSemaphore != NULL) &&
        (xConsoleMutex != NULL))
    {
        xTaskCreate(
            prvQueueReceiveTask,
            "Sensor",
            configMINIMAL_STACK_SIZE,
            NULL,
            tskIDLE_PRIORITY + 1,
            NULL
        );

        xTaskCreate(
            prvQueueSendTask,
            "Monitoring",
            configMINIMAL_STACK_SIZE,
            NULL,
            tskIDLE_PRIORITY + 2,
            NULL
        );

        xTaskCreate(
            prvAlarmTask,
            "Alarm",
            configMINIMAL_STACK_SIZE,
            NULL,
            tskIDLE_PRIORITY + 3,
            NULL
        );

        vTaskStartScheduler();
    }
    else
    {
        printf("Error creating queue or semaphore\n");
    }

    /* Reaching this point means the scheduler did not start. */
    for (;;)
    {
    }
}

/*-----------------------------------------------------------*/


static void prvQueueSendTask(void* pvParameters)
{
    TickType_t xNextWakeTime;
    const TickType_t xPeriod = pdMS_TO_TICKS(2000);  // <--- tutaj modyfikacja.

    uint32_t counter = 0;
    Measurement m;

    (void)pvParameters;

    xNextWakeTime = xTaskGetTickCount();

    for (;;)
    {
        vTaskDelayUntil(&xNextWakeTime, xPeriod);

        /* Generate the next measurement. */
        counter++;
        m.measurementNo = counter;
        m.temperature = 65.0f + (float)(counter % 25);

        /* Send the measurement to the queue. */
        if (xQueueOverwrite(xQueue, &m) == pdPASS) // <--- tutaj modyfikacja.
        {
            if (xSemaphoreTake(xConsoleMutex, portMAX_DELAY) == pdPASS)
            {
                printf(
                    "Monitoring: measurement %lu, temperature %.1f C\n",
                    (unsigned long)m.measurementNo,
                    (double)m.temperature
                );
                
                xSemaphoreGive(xConsoleMutex);
            }

            /* Send several signals if temperature exceeds 75 C. */
            if (m.temperature > 75.0f) // <--- tutaj modyfikacja.
            {
                xAlarmEvent = alarmTemperature; // <--- tutaj modyfikacja.
                xSemaphoreGive(xAlarmSemaphore);
                xSemaphoreGive(xAlarmSemaphore);
                xSemaphoreGive(xAlarmSemaphore);
            }
        }
        else
        {
            if (xSemaphoreTake(xConsoleMutex, portMAX_DELAY) == pdPASS)
            {
                printf("Monitoring: queue is full!\n");
                xSemaphoreGive(xConsoleMutex);
            }
        }
    }
}


static void prvAlarmTask(void* pvParameters)
{
    (void)pvParameters;

    for (;;)
    {
        if (xSemaphoreTake(
            xAlarmSemaphore,
            portMAX_DELAY) == pdPASS)
        {
            if (xSemaphoreTake(
                xConsoleMutex,
                portMAX_DELAY) == pdPASS)
            {
                printf(
                    xAlarmEvent == alarmTemperature
                        ? "\n!!! ALARM: temperatura przekroczyla prog alarmowy 75 C \n\n" // <--- tutaj modyfikacja.
                        : (xAlarmEvent == sensorFailure
                            ? "\n!!! AWARIA: brak nowych pomiarow z czujnika !!!\n\n" // <--- tutaj modyfikacja.
                            : "\nCzujnik ponownie wysyla pomiary. Powrot do pracy.\n\n") // <--- tutaj modyfikacja.
                );

                xSemaphoreGive(xConsoleMutex);
            }
        }
    }
}
/*-----------------------------------------------------------*/


static void prvQueueReceiveTask(void* pvParameters)
{
    Measurement m;

    (void)pvParameters;

    for (;;)
    {
        /* Wait for a measurement from the queue. */
        if (xQueueReceive( 
            xQueue,
            &m,
            pdMS_TO_TICKS(1500)) == pdPASS) // <--- tutaj modyfikacja system zglasza awarie po 1500 ms.
        {
            if (xSensorFailureDetected == pdTRUE) // <--- tutaj modyfikacja.
            {
                xSensorFailureDetected = pdFALSE; // <--- tutaj modyfikacja.
                xAlarmEvent = sensorRecovery; // <--- tutaj modyfikacja.
                xSemaphoreGive(xAlarmSemaphore); // <--- tutaj modyfikacja.
            }

            if (xSemaphoreTake(
                xConsoleMutex,
                portMAX_DELAY) == pdPASS)
            {
                printf(
                    "Sensor: received measurement %lu, temperature %.1f C\n",
                    (unsigned long)m.measurementNo,
                    (double)m.temperature
                );

                xSemaphoreGive(xConsoleMutex);
            }
        }
        else 
        {
            if (xSensorFailureDetected == pdFALSE) // <--- tutaj modyfikacja.
            {
                xSensorFailureDetected = pdTRUE; // <--- tutaj modyfikacja.
                xAlarmEvent = sensorFailure; // <--- tutaj modyfikacja.
                xSemaphoreGive(xAlarmSemaphore); // <--- tutaj modyfikacja.
            }
        }
    }
}

/*-----------------------------------------------------------*/

/* Called from prvKeyboardInterruptSimulatorTask(), which is defined in main.c. */
void vBlinkyKeyboardInterruptHandler(int xKeyPressed)
{
    (void)xKeyPressed;
}