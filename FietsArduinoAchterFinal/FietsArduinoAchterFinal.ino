/*  Adruino code voor de Arduino Nano vooraan op de fiets van Morgan.
 *  Deze code leest 2 potentionmeters uit, mapt deze naar dezelfde hoek. Check op een te groot verschil. 
 *  Indien verschil te groot -> Stop, anders wordt het via I2C (wire) doorgestuurd naar de voorste arduino, 
 *  als stelsignaal voor de servo op het stuur
 *  
 *  Dit programma bestaat uit volgende stukken 
 *  - Interrupt code die positie meet van de trappers. Door in het hoofdprogramma deze waarde uit te lezen en daarna op nul te zetten 
 *    weten we hoe snel Morgan trapt en of ze vooruit of achteruit trapt
 *  - Uitlezen 2x potentiometer van stuur, testen en doorsturen via I2C
 */
 

#include <Wire.h>             // we gebruiken I2C (of wire) om de positie van het stuur in te lezen
#include <avr/interrupt.h>    // We gebruiken Timer 2 als watchdogtimer, omdat bij Chinese arduino Nano door fout in bootloader de normale watchdogtimer niet werkt

#define EncPinA  2  // Aan deze pin kan interrupt gekoppeld worden, Pin A van de encoder
#define EncPinB  4  // Een andere input pin, Pin B van de encoder

float Velocity = 127;         // Dit is de snelheid: 127 is stilstaan. Kleiner is achteruit rijden, groter is vooruit
float FilterTrap = 0.1;       // Filterconstante van de filter op de trapsnelheid, om schokken weg te filteren
volatile byte EncPos = 127;   // De positie van de encoder: 127 is nulpositie, kleiner is achteruit bewegen, groter is vooruit. limiet = 0 tot 255
int PWMTrapOut1 = 0;          // Waarde output naar de PWM1 (analoge output)
int PWMTrapOut2 = 0;          // Waarde output naar de PWM2 (analoge output)
bool EnableMotor = false;     // Remmen of uitbollen
int MaxVelocity = 255;        // Maximum snelheid, in te stellen met potentiometer. Om Morgan traag te laten oefenen

int StuurInput1 = 0;          // Meetwaarde stuurpotentiometer 1
int StuurInput2 = 0;          // Meetwaarde stuurpotentiometer 2
int StuurInputAverage = 0;    // Gemiddelde positie
float StuurOutput = 0;        // Output (stelwaarde om door te sturen naar andere Arduino)
int StuurAlarm = 0;           // Check of ze niet te veel verschillen (of er dus een potentiometer is los gekomen of defect is)




void setup() {
  Wire.begin();               // Start I2C Communicatie
  pinMode(EncPinA, INPUT);   digitalWrite(EncPinA, HIGH);       // Schakel pull-up resistor in (encoder heeft open collector output)
  pinMode(EncPinB, INPUT);   digitalWrite(EncPinB, HIGH);       // Schakel pull-up resistor in

  pinMode(5, OUTPUT);         // PWMOUT1 Aandrijfmotor   }
  pinMode(6, OUTPUT);         // PWMOUT2 Aandrijfmotor    }  Deze drie draden gaan naar de H-Brug vooraan van de aandrijfmotor
  pinMode(7, OUTPUT);         // EnableMotor             }
  pinMode(13, OUTPUT);        // LED
  StuurOutput = 128;          // Stuur in het midden om te beginnen
    
  attachInterrupt(0, doEncoder, CHANGE);  // encoder pin op interrupt 0 - pin 2
  Serial.begin (115200);      //  Start seriele poort, om debug info naar PC scherm te sturen
  Serial.println("Morgan");   //  Print "Morgan"  op het scherm 

  //Setup Timer2 om elke 16ms interrupt te genereren als we de timer niet resetten  (code te vinden op internet of zie datasheet Atmega328P)
  TCCR2B = 0x00;        //Disbale Timer2 while we set it up
  TCNT2  = 130;         //Reset Timer Count to 130 out of 255
  TIFR2  = 0x00;        //Timer2 INT Flag Reg: Clear Timer Overflow Flag
  TIMSK2 = 0x01;        //Timer2 INT Reg: Timer2 Overflow Interrupt Enable
  TCCR2A = 0x00;        //Timer2 Control Reg A: Wave Gen Mode normal
  TCCR2B = 0x07;        //Timer2 Control Reg B: Timer Prescaler set to 1024
}


void loop() {

  // Deel Snelheid en richtin aandrijfmotor

  TCNT2 = 00;           // reset timer 2 = onze eigen watchdogtimer
  EncPos = 127;         // 127 = nul, Lager is achteruit, hoger is vooruit
  delay(10);            // Tel pulsen in de interrupt routine gedurende 2 keer 10ms 
  TCNT2 = 00;           // reset timer 2 = onze eigen watchdogtimer, 2 x 10ms om de watchdog tussenin te kunnen resetten
  delay(10);            // Tel pulsen in de interrupt routine gedurende 2 keer 10ms 
  TCNT2 = 00;           // reset timer 2 = onze eigen watchdogtimer
  
  if (analogRead(A2)>900) {EncPos = 127;}  // Zet gevraagde snelheid op nul als dodemansknop niet optrokken is 
  Velocity = (FilterTrap * EncPos + (1 - FilterTrap) * Velocity);  // Filter snelheidsignaal om de schokker eruit te halen
  
  PWMTrapOut1 = 1;        // We gaan er van uit dat de motor moet stilstaan, indien niet wordt dit in de volgende lijnen aangepast
  PWMTrapOut2 = 1;  
  EnableMotor = false;    // H-brug voor aandrijfMotor ook afzetten
  if (Velocity < 124) {PWMTrapOut1 = (2*(127 - Velocity));   PWMTrapOut2 = 1; EnableMotor = true;} // Als we achteruit moeten rijden PMW1 opzetten
  if (Velocity > 130) {PWMTrapOut1 = 1; PWMTrapOut2 = (2 * (Velocity - 127)); EnableMotor = true;} // Als we vooruit moeten rijden PMW2 opzetten

  MaxVelocity = 84+analogRead(A3)/6;  // Uitlezen extra potentiometer voor de maximum snelheid
  if (analogRead(A2)>900) {EnableMotor = true;}// dodemansknop naar voor -> motor zacht vertragen + remmen Hbrug

  //if (analogRead(A2)<900) {delay(20);} //test watchdog interrupt door dodemansknop te gebruiken, dus laat normaal als commentaar staan

  PWMTrapOut1 = constrain(PWMTrapOut1,1,MaxVelocity); // Snelheid achteruit limiteren op de maximumsnelheid
  PWMTrapOut2 = constrain(PWMTrapOut2,1,MaxVelocity); // Snelheid vooruit limiteren op de maximumsnelheid
  
  analogWrite(5, PWMTrapOut1); // PWM1 effectief naar de H-Brug sturen
  analogWrite(6, PWMTrapOut2); // PWM2 effectief naar de H-Brug sturen
  digitalWrite(7, EnableMotor); // H-Brug op of afzetten 

  // Deel stuur

  StuurInput1 = analogRead(A0);      // Inlezen stuurpoteniometer 1
  StuurInput2 = (analogRead(A1)-50); // Inlezen stuurpoteniometer 2, die mechanisch 50/1024 anders gedraaid zit
  StuurOutput = constrain(map((StuurInput1 + StuurInput2), 630, 1100, 255, 0),5,250); // Gemiddelde stuurinput mappen
  
  StuurAlarm = abs(StuurInput1-StuurInput2+25);          // Verschil van beide maken
  if (StuurAlarm > 50) {                                 // Checken
    Serial.println("Verschil Stuur te groot - STOP");    // Via seriele poort foutbootschap sturen
    Velocity = 127;                                      // Snelheid op 0 zetten
  }
  
  TCNT2 = 00;  // reset timer 2 = onze eigen watchdogtimer
  Wire.beginTransmission(1);   // I2C of Wire communicatie starten, stuur naar adres 1
  
  TCNT2 = 00;  // reset timer 2 = onze eigen watchdogtimer
  delay(1);    // Even wachten, blijkt nodig te zijn bij wire library
  Wire.write(char(StuurOutput)); // Gevraagde positie digitaal doorsturen naar de servo Arduino
  
 
  TCNT2 = 00;  // reset timer 2 = onze eigen watchdogtimer
  delay(1);    // Even wachten, blijkt nodig te zijn bij wire library
  Wire.endTransmission();    // afsluiten I2C boodschap
  TCNT2 = 00;  // reset timer 2 = onze eigen watchdogtimer

   /*  // Om te debuggen kan je data doorsturen naar de PC
  Serial.print (int(Velocity));
  Serial.print("\t");
  Serial.print (int(StuurOutput));
  Serial.print("\t");
  Serial.print(analogRead(A2)); 
  Serial.println("");
  */
}
// Einde Hoofdprogramma


// Interrupt Service Routine voor de Encoder uit te lezen en om te zetten naar snelheid met 127 als stilstand
void doEncoder() {
  if (digitalRead(EncPinA) == digitalRead(EncPinB)) {
    if (EncPos < 252) EncPos=EncPos+4;    // We zorgen dat de positie steeds tussen 0 en 255 blijft
  } else {
    if (EncPos > 3)   EncPos=EncPos-4;
  }
}


// Volgende code is voor onze eigen watchdog functie. Als de timer overflowed (dan loopt het programma niet zoals verwacht) resetten we de Arduino
void(* resetFunc) (void) = 0; //declare reset function @ address 0

ISR(TIMER2_OVF_vect) {  // Timer2 = Watchdog interrupt
  TIFR2 = 0x00;         // Timer2 INT Flag Reg: Clear Timer Overflow Flag
  resetFunc();          // call reset, reboot de Arduino
}
