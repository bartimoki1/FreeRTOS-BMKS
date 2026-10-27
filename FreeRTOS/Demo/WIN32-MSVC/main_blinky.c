#include <stdio.h>
#include <stdint.h>

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "timers.h"

/* Definicja do wyboru mechanizmu ochrony zasobu współdzielonego */
/* Zmień na 0 dla Zadania nr 2 (semafor binarny - inwersja priorytetów) */
/* Zmień na 1 dla Zadania nr 3 (muteks - dziedziczenie priorytetów) */
#define USE_MUTEX 1 

/* Task prototypes. */
static void prvQueueReceiveTask(void* pvParameters);
static void prvQueueSendTask(void* pvParameters);
static void prvAlarmTask(void* pvParameters);
static void prvWatchdogTask(void* pvParameters);
static void prvCheckTask(void* pvParameters);
static void prvCheckTimerCallback(TimerHandle_t xTimer);
static void prvAlarmTimerCallback(TimerHandle_t xTimer);

/* Prototypy dla Zadania nr 5 (Zarządzanie pamięcią) */ // <---- tutaj zmiana (dodano deklaracje nowych zadań eksperymentalnych)
static void prvMemoryTask(void* pvParameters); // <---- tutaj zmiana
static void prvDummyTask(void* pvParameters);  // <---- tutaj zmiana

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
static TimerHandle_t xAlarmTimer = NULL;

/*-----------------------------------------------------------*/

void main_blinky(void)
{
    printf("\r\nTemperature control\r\n\r\n");

    /* Queue: maximum 3 measurements. */
    xQueue = xQueueCreate(3, sizeof(Measurement));

    /* Binary semaphore for alarm timer event */
    xAlarmSemaphore = xSemaphoreCreateBinary();

    /* Binary semaphore: Sensor activity notification. */
    xSensorActivitySemaphore = xSemaphoreCreateBinary();

    /* Binary semaphore: periodic system check notification. */
    xCheckSemaphore = xSemaphoreCreateBinary();

#if USE_MUTEX == 1 
    /* Mutex: protects shared console output. Wspiera dziedziczenie priorytetów. */
    xConsoleMutex = xSemaphoreCreateMutex();
#else 
    /* Binary semaphore: protects shared console output. Powoduje inwersję priorytetów (Zadanie 2). */
    xConsoleMutex = xSemaphoreCreateBinary();
    if (xConsoleMutex != NULL)
    {
        xSemaphoreGive(xConsoleMutex);
    }
#endif 

    /* Periodic software timer: 2000 ms. */
    xCheckTimer = xTimerCreate(
        "CheckTimer",
        pdMS_TO_TICKS(2000UL),
        pdTRUE,
        NULL,
        prvCheckTimerCallback
    );

    /* Software timer for alarm events: 1000 ms */
    xAlarmTimer = xTimerCreate(
        "AlarmTimer",
        pdMS_TO_TICKS(1000UL),
        pdTRUE,
        NULL,
        prvAlarmTimerCallback
    );

    /* Check that all RTOS objects were created. */
    if ((xQueue == NULL) ||
        (xAlarmSemaphore == NULL) ||
        (xSensorActivitySemaphore == NULL) ||
        (xCheckSemaphore == NULL) ||
        (xConsoleMutex == NULL) ||
        (xCheckTimer == NULL) ||
        (xAlarmTimer == NULL))
    {
        printf("ERROR: Could not create RTOS objects.\r\n");

        for (;;)
        {
        }
    }

    /* Create Sensor task. (TaskLow - priority 1) */
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

    /* Create Monitoring task. (TaskMedium - priority 2) */
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

    /* Create Alarm task. (TaskHigh - priority 3) */
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

    /* Create Watchdog task. (TaskMedium - priority 2) */
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

    /* Create periodic system check task. (TaskLow - priority 1) */
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

    /* Create Memory experiment task (Zadanie nr 5). */ // <---- tutaj zmiana (utworzenie zadania testującego zarządzanie pamięcią)
    if (xTaskCreate(
        prvMemoryTask,
        "MemoryTask",
        configMINIMAL_STACK_SIZE,
        NULL,
        tskIDLE_PRIORITY + 1,
        NULL) != pdPASS)
    {
        printf("ERROR: Could not create MemoryTask.\r\n");
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

/* Generates measurements. */
static void prvQueueSendTask(void* pvParameters)
{
    TickType_t xNextWakeTime;
    const TickType_t xPeriod = pdMS_TO_TICKS(500UL);

    uint32_t counter = 0;
    Measurement m;

    (void)pvParameters;

    xNextWakeTime = xTaskGetTickCount();

    if (xTimerStart(xAlarmTimer, 0) != pdPASS)
    {
        if (xSemaphoreTake(xConsoleMutex, portMAX_DELAY) == pdPASS)
        {
            printf("Monitoring: ERROR - could not start AlarmTimer.\r\n");
            xSemaphoreGive(xConsoleMutex);
        }
    }

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
                    "ALARM: timer event received! "
                    "Handled alarm events: %lu\r\n",
                    (unsigned long)handledAlarms
                );

                xSemaphoreGive(xConsoleMutex);
            }

            // vTaskDelay(pdMS_TO_TICKS(1500UL)); 
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

/* Performs a periodic system check every 2000 ms. */
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

static void prvAlarmTimerCallback(TimerHandle_t xTimer)
{
    (void)xTimer;

    (void)xSemaphoreGive(xAlarmSemaphore);
}

/*-----------------------------------------------------------*/
// <---- tutaj zmiana (początek implementacji dla Zadania 5)

/* Zadanie pomocnicze, nic nie robi, zajmuje tylko pamięć w celu demonstracji */
static void prvDummyTask(void* pvParameters)
{
    (void)pvParameters;
    for (;;)
    {
        /* Zwykłe oczekiwanie w pętli */
        vTaskDelay(pdMS_TO_TICKS(1000UL));
    }
}

/* Zadanie realizujące scenariusz z Zadania nr 5 - wykonuje się tylko raz po starcie systemu */
static void prvMemoryTask(void* pvParameters)
{
    TaskHandle_t xDummyHandle = NULL;
    size_t xFreeHeap;

    (void)pvParameters;

    /* Dajemy chwilę na pełne uruchomienie innych zadań */
    vTaskDelay(pdMS_TO_TICKS(100UL));

    /* 1. Odczytaj ilość wolnej pamięci przed utworzeniem zadania */
    xFreeHeap = xPortGetFreeHeapSize();
    if (xSemaphoreTake(xConsoleMutex, portMAX_DELAY) == pdPASS)
    {
        printf("Zadanie 5: Wolna pamiec przed xTaskCreate(): %u bajtow\r\n", (unsigned int)xFreeHeap);
        xSemaphoreGive(xConsoleMutex);
    }

    /* 2. Utwórz dodatkowe zadanie za pomocą xTaskCreate() i sprawdź wynik */
    if (xTaskCreate(prvDummyTask, "Dummy", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY + 1, &xDummyHandle) == pdPASS)
    {
        /* 3. Ponownie odczytaj ilość wolnej pamięci i porównaj z pomiarem początkowym */
        xFreeHeap = xPortGetFreeHeapSize();
        if (xSemaphoreTake(xConsoleMutex, portMAX_DELAY) == pdPASS)
        {
            printf("Zadanie 5: Wolna pamiec po utworzeniu zadania Dummy: %u bajtow\r\n", (unsigned int)xFreeHeap);
            xSemaphoreGive(xConsoleMutex);
        }

        /* 4. Sprawdź, co dzieje się po usunięciu zadania za pomocą vTaskDelete() */
        vTaskDelete(xDummyHandle);
        if (xSemaphoreTake(xConsoleMutex, portMAX_DELAY) == pdPASS)
        {
            printf("Zadanie 5: Usunieto zadanie Dummy (vTaskDelete). Czekam na reakcje zadania Idle...\r\n");
            xSemaphoreGive(xConsoleMutex);
        }

        /* 5. Czekamy, aby zadanie Idle (priorytet 0) otrzymało czas procesora i zwolniło pamięć */
        vTaskDelay(pdMS_TO_TICKS(100UL));

        /* 6. Odczyt wolnej pamięci po zwolnieniu zasobów przez zadanie Idle */
        xFreeHeap = xPortGetFreeHeapSize();
        if (xSemaphoreTake(xConsoleMutex, portMAX_DELAY) == pdPASS)
        {
            printf("Zadanie 5: Wolna pamiec po interwencji zadania Idle: %u bajtow\r\n", (unsigned int)xFreeHeap);
            xSemaphoreGive(xConsoleMutex);
        }
    }
    else
    {
        if (xSemaphoreTake(xConsoleMutex, portMAX_DELAY) == pdPASS)
        {
            printf("Zadanie 5: BLAD! xTaskCreate() zwrocil blad (np. z powodu braku pamieci na stercie).\r\n");
            xSemaphoreGive(xConsoleMutex);
        }
    }

    /* Zakończ zadanie eksperymentalne, zwalniając je (pamięć z niego zostanie zwolniona w kolejnym wywołaniu Idle taska) */
    vTaskDelete(NULL);
}
// <---- tutaj zmiana (koniec implementacji dla Zadania 5)