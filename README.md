# Overview
This repository contains laboratory projects focusing on core RTOS architecture, task management, inter-task communication, and synchronization in embedded systems.
## Key Implemented Features
- **Task Management & Scheduling:** Task creation (`xTaskCreate`), termination (`vTaskDelete`), priority-based scheduling, and execution control.
- **Timing & Delays:** Implementation of precise periodic execution using `vTaskDelayUntil` (preventing timing drift) vs. relative delays (`vTaskDelay`).
- **Inter-Task Communication (Queues & Mailbox):**
  - Safe data streaming via **FIFO queues** (`xQueueSend`, `xQueueReceive`).
  - Handling queue overflow states and non-blocking transmissions (`0` tick timeouts).
  - Implementation of a single-element **Mailbox** pattern with `xQueueOverwrite()` to retain latest measurements.
- **Resource Protection & Synchronization:**
  - Resource access serialization using **Mutexes** (`xSemaphoreCreateMutex`, `xSemaphoreTake`, `xSemaphoreGive`) to prevent race conditions (e.g., console output protection).
  - Event signaling and task unblocking via **Binary Semaphores**.
