#include "errors.h"

// Definicja zmiennych globalnych
volatile uint32_t ESP_NOW_Monitor_Send_Error_Counter = 0;
volatile uint32_t ESP_NOW_Platform_Send_Error_Counter = 0;
volatile uint32_t ESP_NOW_Monitor_Receive_Error_Counter = 0;
volatile uint32_t ESP_NOW_Platform_Receive_Error_Counter = 0;
volatile bool ESP_NOW_Monitor_Error = true;
volatile bool ESP_NOW_Platform_Error = true;
