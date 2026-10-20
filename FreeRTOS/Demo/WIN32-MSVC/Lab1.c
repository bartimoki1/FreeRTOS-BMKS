```c
/*
 * FreeRTOS V202212.00
 *
 * LABORATORIUM 1
 * Zadania i harmonogramowanie w FreeRTOS
 *
 * Środowisko:
 * Visual Studio + FreeRTOS Windows Simulator
 *
 * Cel laboratorium:
 * - tworzenie zadań,
 * - priorytety zadań,
 * - działanie schedulera,
 * - stany zadań,
 * - vTaskDelay(),
 * - vTaskDelayUntil(),
 * - wykonywanie zadań okresowych,
 * - obserwacja wpływu priorytetów na działanie systemu.
 *
 * UWAGA:
 * Windows Simulator nie zapewnia zachowania czasu rzeczywistego.
 * Czasy należy traktować jako wartości demonstracyjne.
 */

 /* Standard includes. */
#include <stdio.h>

/* Kernel includes. */
#include "FreeRTOS.h"
#include "task.h"

/*-----------------------------------------------------------
 * PARAMETRY ZADAŃ
 *-----------------------------------------------------------*/

 /* Priorytety zadań. */
#define TASK_A_PRIORITY    ( tskIDLE_PRIORITY + 1 )
#define TASK_B_PRIORITY    ( tskIDLE_PRIORITY + 2 )
#define TASK_C_PRIORITY    ( tskIDLE_PRIORITY + 3 )

/* Okresy wykonywania zadań. */
#define TASK_A_PERIOD_MS   ( 500UL )
#define TASK_B_PERIOD_MS   ( 700UL )
#define TASK_C_PERIOD_MS   ( 1000UL )

/*-----------------------------------------------------------
 * DEKLARACJE FUNKCJI ZADAŃ
 *-----------------------------------------------------------*/

    static void prvTaskA(void* pvParameters);
static void prvTaskB(void* pvParameters);
static void prvTaskC(void* pvParameters);

/*-----------------------------------------------------------*/

/*
 * Funkcja uruchamiająca demonstrację Lab 1.
 */
void main_blinky(void)
{
    printf("\r\n");
    printf("============================================\r\n");
    printf("        FreeRTOS - LABORATORIUM 1\r\n");
    printf("        Zadania i harmonogramowanie\r\n");
    printf("============================================\r\n\r\n");

    printf("Uruchamianie trzech zadan...\r\n\r\n");

    /*
     * TASK A
     *
     * Priorytet: 1
     * Okres: 500 ms
     */
    if (xTaskCreate(
        prvTaskA,
        "Task_A",
        configMINIMAL_STACK_SIZE,
        NULL,
        TASK_A_PRIORITY,
        NULL) != pdPASS)
    {
        printf("Blad: nie m
            ```
