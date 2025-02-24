#include <Arduino.h>

#include <Adafruit_seesaw.h>

Adafruit_seesaw ss;
Adafruit_seesaw ss2;

#define BUTTON_X         6
#define BUTTON_Y         2
#define BUTTON_A         5
#define BUTTON_B         1
#define BUTTON_SELECT    0
#define BUTTON_START    16
uint32_t button_mask = (1UL << BUTTON_X) | (1UL << BUTTON_Y) | (1UL << BUTTON_START) |
                       (1UL << BUTTON_A) | (1UL << BUTTON_B) | (1UL << BUTTON_SELECT);

uint32_t button_mask2 = (1UL << BUTTON_X) | (1UL << BUTTON_Y) | (1UL << BUTTON_START) |
                       (1UL << BUTTON_A) | (1UL << BUTTON_B) | (1UL << BUTTON_SELECT);

//#define IRQ_PIN   5


void setup() {
  Serial.begin(115200);

  while(!Serial) {
    delay(10);
  }

  Serial.println("Gamepad QT example!");
  
  if(!ss.begin(0x50)){
    Serial.println("ERROR! seesaw not found");
    while(1) delay(1);
  }


  if(!ss2.begin(0x51)){
    Serial.println("ERROR! seesaw not found");
    while(1) delay(1);
  }
  Serial.println("seesaw started");
  uint32_t version = ((ss.getVersion() >> 16) & 0xFFFF);
  if (version != 5743) {
    Serial.print("Wrong firmware loaded? ");
    Serial.println(version);
    while(1) delay(10);
  }
  Serial.println("Found Product 5743");
  
  ss.pinModeBulk(button_mask, INPUT_PULLUP);
  ss.setGPIOInterrupts(button_mask, 1);
  ss2.pinModeBulk(button_mask2, INPUT_PULLUP);
  ss2.setGPIOInterrupts(button_mask2, 1);
#if defined(IRQ_PIN)
  pinMode(IRQ_PIN, INPUT);
#endif
}


int last_Lx = 0, last_Ly = 0;
int last_Rx = 0, last_Ry = 0;

void loop() {
  delay(10); // delay in loop to slow serial output
  
  // Reverse x/y values to match joystick orientation
  int Lx = 1023 - ss.analogRead(14);
  int Ly = 1023 - ss.analogRead(15);
  int Rx = 1023 - ss2.analogRead(14);
  int Ry = 1023 - ss2.analogRead(15);
  
  if ( (abs(Lx - last_Lx) > 3)  ||  (abs(Ly - last_Ly) > 3)) {
    Serial.print("Lx: "); Serial.print(Lx); Serial.print(", "); Serial.print("Ly: "); Serial.println(Ly);
    last_Lx = Lx;
    last_Ly = Ly;
  }
  if ( (abs(Rx - last_Rx) > 3)  ||  (abs(Ry - last_Ry) > 3)) {
    Serial.print("Rx: "); Serial.print(Rx); Serial.print(", "); Serial.print("Ry: "); Serial.println(Ry);
    last_Rx = Rx;
    last_Ry = Ry;
  }

#if defined(IRQ_PIN)
  if(!digitalRead(IRQ_PIN)) {
    return;
  }
#endif

    uint32_t buttonsL = ss.digitalReadBulk(button_mask);
    uint32_t buttonsR = ss2.digitalReadBulk(button_mask2);

    //Serial.println(buttons, BIN);

    if (! (buttonsL & (1UL << BUTTON_A))) {
      Serial.println("Button L_A pressed");
    }
    if (! (buttonsL & (1UL << BUTTON_B))) {
      Serial.println("Button L_B pressed");
    }
    if (! (buttonsL & (1UL << BUTTON_Y))) {
      Serial.println("Button L_Y pressed");
    }
    if (! (buttonsL & (1UL << BUTTON_X))) {
      Serial.println("Button L_X pressed");
    }
    if (! (buttonsL & (1UL << BUTTON_SELECT))) {
      Serial.println("Button L_SELECT pressed");
    }
    if (! (buttonsL & (1UL << BUTTON_START))) {
      Serial.println("Button L_START pressed");
    }

    if (! (buttonsR & (1UL << BUTTON_A))) {
      Serial.println("Button R_A pressed");
    }
    if (! (buttonsR & (1UL << BUTTON_B))) {
      Serial.println("Button R_B pressed");
    }
    if (! (buttonsR & (1UL << BUTTON_Y))) {
      Serial.println("Button R_Y pressed");
    }
    if (! (buttonsR & (1UL << BUTTON_X))) {
      Serial.println("Button R_X pressed");
    }
    if (! (buttonsR & (1UL << BUTTON_SELECT))) {
      Serial.println("Button R_SELECT pressed");
    }
    if (! (buttonsR & (1UL << BUTTON_START))) {
      Serial.println("Button R_START pressed");
    }
}
