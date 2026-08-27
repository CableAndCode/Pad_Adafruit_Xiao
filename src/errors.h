#ifndef ERRORS_H
#define ERRORS_H

#include <Arduino.h>

// ====== Global error tracking variables (declared extern) ======
// Liczniki dotyczą jedynego partnera Pada, czyli platformy. Liczniki
// monitora zniknęły razem z wysyłką pod jego adres.
extern volatile uint32_t ESP_NOW_Platform_Send_Error_Counter;
extern volatile bool     ESP_NOW_Platform_Error;

#endif // ERRORS_H
