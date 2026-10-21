#include <stdio.h>
#include <stdint.h>

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "timers.h"

/* Task prototypes. */
static void prvQueueReceiveTask(void* pvParameters);
static void prvQueueSendTask(void* pvParameters);
static void prvAlarmTask(void* pvParameters);
static void prvWatchdogTask(void* pvParameters);
static void prvCheckTask(void* pvParameters);
static void prvCheckTimerCallback(TimerHandle_t xTimer);

typedef struct
{
    uint32_t measurementNo;
    float temperature;
} Measurement;

/* Shared RTOS objects. */
static QueueHandle_t xQueue = NULL;
static SemaphoreHandle_t xAlarmSemaphore = NULL;
static SemaphoreHandle_t xConsoleMutex = NULL;
static SemaphoreHandle_t xSensorActivitySemaphore = NULL;
static SemaphoreHandle_t xCheckSemaphore = NULL;
static TimerHandle_t xCheckTimer = NULL;

/*-----------------------------------------------------------*/

void main_blinky(void)
{
    printf("\r\nTemperature control\r\n\r\n");

    /* Queue: maximum 3 measurements. */
    xQueue = xQueueCreate(3, sizeof(Measurement));

    /* Counting semaphore: maximum 5 pending alarm events. */
    xAlarmSemaphore = xSemaphoreCreateCounting(5, 0);

    /* Binary semaphore: Sensor activity notification. */
    xSensorActivitySemaphore = xSemaphoreCreateBinary();

    /* Binary semaphore: periodic system check notification. */
    xCheckSemaphore = xSemaphoreCreateBinary();

    /* Mutex: protects shared console output. */
    xConsoleMutex = xSemaphoreCreateMutex();

    /* Periodic software timer: 2000 ms. */ // <---- tutaj zmiana (zmiana okresu timera na 2000 ms)
    xCheckTimer = xTimerCreate(
        "CheckTimer",
        pdMS_TO_TICKS(2000UL), // <---- tutaj zmiana (timer zgłasza zdarzenie co 2000 ms)
        pdTRUE,
        NULL,
        prvCheckTimerCallback
    );

    /* Check that all RTOS objects were created. */
    if ((xQueue == NULL) ||
        (xAlarmSemaphore == NULL) ||
        (xSensorActivitySemaphore == NULL) ||
        (xCheckSemaphore == NULL) ||
        (xConsoleMutex == NULL) ||
        (xCheckTimer == NULL))
    {
        printf("ERROR: Could not create RTOS objects.\r\n");

        for (;;)
        {
        }
    }

    /* Create Sensor task. */
    if (xTaskCreate(
        prvQueueReceiveTask,
        "Sensor",
        configMINIMAL_STACK_SIZE,
        NULL,
        tskIDLE_PRIORITY + 1,
        NULL) != pdPASS)
    {
        printf("ERROR: Could not create Sensor task.\r\n");
        for (;;)
        {
        }
    }

    /* Create Monitoring task. */
    if (xTaskCreate(
        prvQueueSendTask,
        "Monitoring",
        configMINIMAL_STACK_SIZE,
        NULL,
        tskIDLE_PRIORITY + 2,
        NULL) != pdPASS)
    {
        printf("ERROR: Could not create Monitoring task.\r\n");
        for (;;)
        {
        }
    }

    /* Create Alarm task. */
    if (xTaskCreate(
        prvAlarmTask,
        "Alarm",
        configMINIMAL_STACK_SIZE,
        NULL,
        tskIDLE_PRIORITY + 3,
        NULL) != pdPASS)
    {
        printf("ERROR: Could not create Alarm task.\r\n");
        for (;;)
        {
        }
    }

    /* Create Watchdog task. */
    if (xTaskCreate(
        prvWatchdogTask,
        "Watchdog",
        configMINIMAL_STACK_SIZE,
        NULL,
        tskIDLE_PRIORITY + 2,
        NULL) != pdPASS)
    {
        printf("ERROR: Could not create Watchdog task.\r\n");
        for (;;)
        {
        }
    }

    /* Create periodic system check task. */
    if (xTaskCreate(
        prvCheckTask,
        "CheckTask",
        configMINIMAL_STACK_SIZE,
        NULL,
        tskIDLE_PRIORITY + 1,
        NULL) != pdPASS)
    {
        printf("ERROR: Could not create CheckTask.\r\n");
        for (;;)
        {
        }
    }

    /* Start the scheduler only after creating all objects and tasks. */
    vTaskStartScheduler();

    /* Reaching this point means the scheduler did not start. */
    printf("ERROR: Could not start the scheduler.\r\n");

    for (;;)
    {
    }
}

/*-----------------------------------------------------------*/

/* Generates measurements and signals alarm events. */
static void prvQueueSendTask(void* pvParameters)
{
    TickType_t xNextWakeTime;
    const TickType_t xPeriod = pdMS_TO_TICKS(500UL);

    uint32_t counter = 0;
    Measurement m;

    (void)pvParameters;

    xNextWakeTime = xTaskGetTickCount();

    for (;;)
    {
        vTaskDelayUntil(&xNextWakeTime, xPeriod);

        counter++;
        m.measurementNo = counter;
        m.temperature = 65.0f + (float)(counter % 25);

        /* Send measurement without waiting for free queue space. */
        if (xQueueSend(xQueue, &m, 0) == pdPASS)
        {
            if (xSemaphoreTake(xConsoleMutex, portMAX_DELAY) == pdPASS)
            {
                printf(
                    "Monitoring: measurement %lu, temperature %.1f C\r\n",
                    (unsigned long)m.measurementNo,
                    (double)m.temperature
                );

                xSemaphoreGive(xConsoleMutex);
            }
        }
        else
        {
            if (xSemaphoreTake(xConsoleMutex, portMAX_DELAY) == pdPASS)
            {
                printf(
                    "Monitoring: queue full; measurement %lu was not sent\r\n",
                    (unsigned long)m.measurementNo
                );

                xSemaphoreGive(xConsoleMutex);
            }
        }

        /* Signal an alarm for each generated measurement above 75 C. */
        if (m.temperature > 75.0f)
        {
            if (xSemaphoreGive(xAlarmSemaphore) == pdPASS)
            {
                if (xSemaphoreTake(xConsoleMutex, portMAX_DELAY) == pdPASS)
                {
                    printf(
                        "Monitoring: alarm event registered, measurement %lu\r\n",
                        (unsigned long)m.measurementNo
                    );

                    xSemaphoreGive(xConsoleMutex);
                }
            }
            else
            {
                if (xSemaphoreTake(xConsoleMutex, portMAX_DELAY) == pdPASS)
                {
                    printf("Monitoring: counting semaphore is full!\r\n");
                    xSemaphoreGive(xConsoleMutex);
                }
            }
        }
    }
}

/*-----------------------------------------------------------*/

/* Consumes pending alarm signals and counts handled events. */
static void prvAlarmTask(void* pvParameters)
{
    uint32_t handledAlarms = 0;

    (void)pvParameters;

    for (;;)
    {
        if (xSemaphoreTake(xAlarmSemaphore, portMAX_DELAY) == pdPASS)
        {
            handledAlarms++;

            if (xSemaphoreTake(xConsoleMutex, portMAX_DELAY) == pdPASS)
            {
                printf(
                    "ALARM: temperature exceeded 75 C! "
                    "Handled alarm events: %lu\r\n",
                    (unsigned long)handledAlarms
                );

                xSemaphoreGive(xConsoleMutex);
            }

            /*
             * Experimental delay: allows alarm events to accumulate.
             * Remove it after completing the semaphore experiment.
             */
            vTaskDelay(pdMS_TO_TICKS(1500UL));
        }
    }
}

/*-----------------------------------------------------------*/

/* Receives measurements and signals Sensor activity. */
static void prvQueueReceiveTask(void* pvParameters)
{
    Measurement m;

    (void)pvParameters;

    for (;;)
    {
        if (xQueueReceive(xQueue, &m, portMAX_DELAY) == pdPASS)
        {
            if (xSemaphoreTake(xConsoleMutex, portMAX_DELAY) == pdPASS)
            {
                printf(
                    "Sensor: received measurement %lu, temperature %.1f C\r\n",
                    (unsigned long)m.measurementNo,
                    (double)m.temperature
                );

                xSemaphoreGive(xConsoleMutex);
            }

            /* Notify Watchdog that Sensor received a measurement. */
            (void)xSemaphoreGive(xSensorActivitySemaphore);
        }
    }
}

/*-----------------------------------------------------------*/

/* Detects if Sensor stops receiving measurements for 1500 ms. */
static void prvWatchdogTask(void* pvParameters)
{
    BaseType_t xSensorFailed = pdFALSE;

    (void)pvParameters;

    for (;;)
    {
        BaseType_t xReceived = xSemaphoreTake(
            xSensorActivitySemaphore,
            pdMS_TO_TICKS(1500UL)
        );

        if (xReceived == pdPASS)
        {
            if (xSensorFailed == pdTRUE)
            {
                xSensorFailed = pdFALSE;

                if (xSemaphoreTake(xConsoleMutex, portMAX_DELAY) == pdPASS)
                {
                    printf("Watchdog: sensor has resumed operation\r\n");
                    xSemaphoreGive(xConsoleMutex);
                }
            }
        }
        else
        {
            if (xSensorFailed == pdFALSE)
            {
                xSensorFailed = pdTRUE;

                if (xSemaphoreTake(xConsoleMutex, portMAX_DELAY) == pdPASS)
                {
                    printf(
                        "Watchdog: SENSOR FAILURE - "
                        "no measurement for 1500 ms!\r\n"
                    );

                    xSemaphoreGive(xConsoleMutex);
                }
            }
        }
    }
}

/*-----------------------------------------------------------*/

/* Called by the keyboard simulator in the Win32 demo. */
void vBlinkyKeyboardInterruptHandler(int xKeyPressed)
{
    (void)xKeyPressed;
}

/*-----------------------------------------------------------*/

/* Timer callback: signals CheckTask; does not print to console. */
static void prvCheckTimerCallback(TimerHandle_t xTimer)
{
    (void)xTimer;

    (void)xSemaphoreGive(xCheckSemaphore);
}

/*-----------------------------------------------------------*/

/* Performs a periodic system check every 2000 ms. */ // <---- tutaj zmiana (aktualizacja opisu częstotliwości CheckTask)
static void prvCheckTask(void* pvParameters)
{
    uint32_t checkNo = 0;

    (void)pvParameters;

    /* Start the timer after the scheduler has started. */
    if (xTimerStart(xCheckTimer, 0) != pdPASS)
    {
        if (xSemaphoreTake(xConsoleMutex, portMAX_DELAY) == pdPASS)
        {
            printf("CheckTask: ERROR - could not start timer.\r\n");
            xSemaphoreGive(xConsoleMutex);
        }

        vTaskDelete(NULL);
        return;
    }

    for (;;)
    {
        if (xSemaphoreTake(xCheckSemaphore, portMAX_DELAY) == pdPASS)
        {
            checkNo++;

            if (xSemaphoreTake(xConsoleMutex, portMAX_DELAY) == pdPASS)
            {
                printf(
                    "CheckTask: periodic system check no. %lu\r\n",
                    (unsigned long)checkNo
                );

                xSemaphoreGive(xConsoleMutex);
            }
        }
    }
}

static void prvAlarmTimerCallback(void* pvParameters)
{

}