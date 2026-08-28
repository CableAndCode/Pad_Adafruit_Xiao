#include "errors.h"

// Definition of global error tracking variables
volatile uint32_t ESP_NOW_Platform_Send_Error_Counter = 0;
volatile bool     ESP_NOW_Platform_Error = true;
