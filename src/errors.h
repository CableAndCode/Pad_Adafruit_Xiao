#ifndef ERRORS_H
#define ERRORS_H

#include <Arduino.h>

// ====== Global error tracking variables (declared extern) ======
extern volatile uint32_t ESP_NOW_Monitor_Send_Error_Counter;
extern volatile uint32_t ESP_NOW_Platform_Send_Error_Counter;
extern volatile uint32_t ESP_NOW_Monitor_Receive_Error_Counter;
extern volatile uint32_t ESP_NOW_Platform_Receive_Error_Counter;

extern volatile bool ESP_NOW_Monitor_Error;
extern volatile bool ESP_NOW_Platform_Error;

#endif // ERRORS_H
