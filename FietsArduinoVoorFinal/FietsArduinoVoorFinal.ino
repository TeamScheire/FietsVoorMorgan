/* Adruino code voor de Arduino Nano vooraan op de fiets van Morgan. Dit is een servo motor controller die de stelwaarde via I2C krijgt

 */

#include <Wire.h>             // We gebruiken I2C of wire
#include <avr/interrupt.h>    // We gebruiken Timer 2 als watchdogtimer, omdat bij Chinese arduino Nano door fout in bootloader de normale watchdogtimer niet werkt

const int ErrorS = 2;         // Definieren van marges van de servocontroller
const int ErrorM = 5;         // We werken met 3 zones aan elke kant, met 3 verschillende snelheden
const int ErrorL = 10; 
const int PWMValueS = 25;     // De 3 snelheden. Deze 6 parameters dienen om het servostuur te tunen dat het vlot en snel beweegt zonder schokken
const int PWMValueM = 40; 
const int PWMValueL = 100; 

float FilteredReqPos = 0;     // gefilterde waarde van de gevraagde stuurpositie
int MeasuredPosition = 0;     // Gemeten positie
int PosError = 0;             // Verschil tussen gemeten en gewenste positie
int PWMout1 = 0;              // Waarde output naar de PWM1 (analoge output)
int PWMout2 = 0;              // Waarde output naar de PWM2 (analoge output)
volatile int ReqPos = 0;      // gewenste positie die binnenkomt via I2C

void setup() {
  //Setup Timer2 om elke 16ms interrupt te genereren als we de timer niet resetten  (code te vinden op internet of zie datasheet Atmega328P)
  TCCR2B = 0x00;        //Disbale Timer2 while we set it up
  TCNT2  = 130;         //Reset Timer Count to 130 out of 255
  TIFR2  = 0x00;        //Timer2 INT Flag Reg: Clear Timer Overflow Flag
  TIMSK2 = 0x01;        //Timer2 INT Reg: Timer2 Overflow Interrupt Enable
  TCCR2A = 0x00;        //Timer2 Control Reg A: Wave Gen Mode normal
  TCCR2B = 0x07;        //Timer2 Control Reg B: Timer Prescaler set to 1024

  Wire.begin(1);                // Start I2C Communicatie, met adres 1
  Wire.onReceive(receiveEvent); // Koppel de I2C interrupt
  Serial.begin(115200);         //  Start seriele poort, om debug info naar PC scherm te sturen
}

void loop() {
  TCNT2 = 00;  // reset timer 2 = onze eigen watchdogtimer

  MeasuredPosition = constrain(map(analogRead(A0),196,845,0,255),0,255); // Meet de positie van de servo = positie van het stuur
  PosError = 1024 + ReqPos - MeasuredPosition;    // Bereken het verschil
  PWMout1 = 1;        // Zet beide PWM's voorlopig uit
  PWMout2 = 1;        // Zet beide PWM's voorlopig uit
  
  if(PosError > (1024 + ErrorS)) {PWMout1 = PWMValueS; PWMout2 = 1;  }    // Kijk naar het verschil en neem de juiste actie
  if(PosError > (1024 + ErrorM)) {PWMout1 = PWMValueM; PWMout2 = 1;  }
  if(PosError > (1024 + ErrorL)) {PWMout1 = PWMValueL; PWMout2 = 1;  }
     
  if(PosError < (1024 - ErrorS)) {PWMout1 = 1; PWMout2 = PWMValueS;  }
  if(PosError < (1024 - ErrorM)) {PWMout1 = 1; PWMout2 = PWMValueM;  }
  if(PosError < (1024 - ErrorL)) {PWMout1 = 1; PWMout2 = PWMValueL;  }

  // send the PWM values to the pins:
  analogWrite(5, PWMout1);    // PWM1 effectief naar de H-Brug sturen
  analogWrite(6, PWMout2);    // PWM2 effectief naar de H-Brug sturen


/* // Om te debuggen kan je data doorsturen naar de PC
  Serial.print("R ");
  Serial.print(ReqPos);
  Serial.print("\t M ");
  Serial.print(MeasuredPosition);
  Serial.println("");
  
  Serial.print("\t PWM1 ");
  Serial.print(PWMout1);
  Serial.print("\t PWM2 ");
  Serial.println(PWMout2);
*/

}
// Einde Hoofdprogramma

// IC2 Interrupt: Lees de inkomende data in1 byte en stop die in ReqPos = de gewenste stuurpositie
void receiveEvent(int howMany) {
  ReqPos = Wire.read(); 
  if (ReqPos < 5) {ReqPos = 5;}
  if (ReqPos > 250) {ReqPos = 250;}
}

// Volgende code is voor onze eigen watchdog functie. Als de timer overflowed (dan loopt het programma niet zoals verwacht) resetten we de Arduino
void(* resetFunc) (void) = 0; //declare reset function @ address 0

ISR(TIMER2_OVF_vect) {  // Timer2 = Watchdog interrupt
  TIFR2 = 0x00;         // Timer2 INT Flag Reg: Clear Timer Overflow Flag
  resetFunc();          // call reset, reboot de Arduino
}
