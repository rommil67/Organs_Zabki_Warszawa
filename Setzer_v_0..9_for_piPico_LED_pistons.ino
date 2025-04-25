// setzer zastosowany w kontuarze organów w Czyżewie
#include <Wire.h> 
#include <LiquidCrystal_I2C.h>
#include <MIDI.h>
MIDI_CREATE_INSTANCE(HardwareSerial, Serial1, MIDI)
// Rotary Encoder Inputs
#define CLK 2
#define DT 3
#define SW 12

#define SetPin A2
#define CrescendoPin A1
#define analogCrescendoReadPin A0 //- dlatego ze bobuino
/*
  SD card read/write

  This example shows how to read and write data to and from an SD card file
  The circuit:
   SD card attached to SPI bus as follows:
   ** MISO - pin 4
   ** MOSI - pin 7
   ** CS   - pin 5
   ** SCK  - pin 6

  created   Nov 2010
  by David A. Mellis
  modified 9 Apr 2012
  by Tom Igoe

  This example code is in the public domain.

*/

// This are GP pins for SPI0 on the Raspberry Pi Pico board, and connect
// to different *board* level pinouts.  Check the PCB while wiring.
// Only certain pins can be used by the SPI hardware, so if you change
// these be sure they are legal or the program will crash.
// See: https://datasheets.raspberrypi.com/picow/PicoW-A4-Pinout.pdf
const int _MISO = 16;
const int _MOSI = 19;
const int _CS = 17;
const int _SCK = 18;

#include <SPI.h>
#include <SD.h>

File dataFile;
// zmienne setzera
int displayNrComb = 1; // Zmienna nrBank używana tylko do wyświetlania
int realNrComb = 1;    // Rzeczywista wartość nrBank używana do zmiany
int displayNrBank = 1; // Zmienna nrBank używana tylko do wyświetlania
int realNrBank = 1;    // Rzeczywista wartość nrBank używana do zmiany
int encoderChange = 0; // Zmienna używana do śledzenia zmian enkodera co 4 odczyty
//int globamNrPitch = 36; // Zmienna urzywana do obsługi Leds pistons przez bezposrnie wywolanie sendMidiOn

unsigned long BAS_channel_1 = 0; // Flags for MIDI-channel 1
unsigned long TREBL_channel_1 = 0;

unsigned long prev_BAS_channel_1 = 0; // Flags for MIDI-channel 1
unsigned long prev_TREBL_channel_1 = 0;

int nr = 1;

int crescendoGradusPrev = 1;
int crescendoGradus = 1;
int progCrescFlag = 0;
bool crescTouche = false;
unsigned long storeBAS_channel_1 = 0;
unsigned long storeTREBL_channel_1 = 0;

int storeDisplayNrComb = 1; // Zmienna nrBank używana tylko do wyświetlania
int storeRealNrComb = 1;    // Rzeczywista wartość nrBank używana do zmiany
int storeDisplayNrBank = 1; // Zmienna nrBank używana tylko do wyświetlania
int storeRealNrBank = 1;
int storeNr = nr;



int counter = 0;
int currentStateCLK;
int lastStateCLK;
String currentDir = "";
unsigned long lastButtonPress = 0;
// Set the LCD address to 0x27 for a 16 chars and 2 line display
LiquidCrystal_I2C lcd(0x27, 16, 2);

void setup()
{
	// initialize the LCD
	lcd.begin();

	// Turn on the blacklight and print a message.
	lcd.backlight();
	lcd.print("Hello, world!");

  MIDI.begin(MIDI_CHANNEL_OMNI); // Initialize the Midi Library.

  MIDI.turnThruOff(); // Nie przesyła komunikatów z wejscia na wyjscie.

  MIDI.setHandleNoteOn(MyHandleNoteOn); // This is important!!
  MIDI.setHandleNoteOff(MyHandleNoteOff); // This command tells the Midi Library
  	
	// Set encoder pins as inputs
	pinMode(CLK,INPUT);
	pinMode(DT,INPUT);
	pinMode(SW, INPUT_PULLUP);

	// Setup Serial Monitor
	Serial.begin(9600);
  
Serial.print("\nInitializing SD card...");
  lcd.setCursor(0, 0);
    lcd.print("Initializing SD card... ");

  // Ensure the SPI pinout the SD card is connected to is configured properly
  SPI.setRX(_MISO);
  SPI.setTX(_MOSI);
  SPI.setSCK(_SCK);

  if (!SD.begin(_CS)) {
    Serial.println("SD card initialization failed!");
    lcd.clear();
    lcd.home();
    lcd.print("Card SD Error!");
  
  } else{
    Serial.println("SD card initialization done.");
    lcd.clear();
    lcd.home();
    lcd.print("Card SD Ok!");
  }
  

	// Read the initial state of CLK
	lastStateCLK = digitalRead(CLK);

   pinMode(SetPin, OUTPUT);
  digitalWrite(SetPin, HIGH);
  pinMode(CrescendoPin, OUTPUT);
  digitalWrite(CrescendoPin, HIGH);
  pinMode(analogCrescendoReadPin, INPUT);
 
    char fileName[13]; // Długość nazwy pliku nie przekracza 8 znaków + 1 znak rozszerzenia + 4 znaki numeru pliku (1, 2, 3...)
    sprintf(fileName, "C%dB%d.txt", nr, realNrBank);
}

void loop()
{
	 MIDI.read(); // Continuously check if Midi data has been received.
  senderMidi();
  cresc();
	// Read the current state of CLK
	currentStateCLK = digitalRead(CLK);

	// If last and current state of CLK are different, then pulse occurred
	// React to only 1 state change to avoid double count
	if (currentStateCLK != lastStateCLK  && currentStateCLK == 1){

		// If the DT state is different than the CLK state then
		// the encoder is rotating CCW so decrement
		if (digitalRead(DT) != currentStateCLK) {
			counter --;
			currentDir ="CCW";
		} else {
			// Encoder is rotating CW so increment
			counter ++;
			currentDir ="CW";
		}
realNrBank = counter;
// Sprawdź i popraw wartość realNrBank, jeśli przekroczy 256 lub spadnie poniżej 1
      checkNrBankLimits();		
	}
 // Sprawdź, czy rzeczywista wartość realNrBank się zmieniła
  static int lastRealNrBank = 0;
  if (realNrBank != lastRealNrBank)
  {
    // Aktualizuj wartość wyświetlanej zmiennej displayNrBank
    displayNrBank = realNrBank;
    // Wyświetl wartość displayNrBank na drugim wierszu wyświetlacza LCD
    lcd.setCursor(0, 1);
    lcd.print("Bank nr         "); // Czyszczenie starej wartości
    lcd.setCursor(8, 1);
    lcd.print(displayNrBank);

    lastRealNrBank = realNrBank; // Zapisz aktualną wartość realNrBank jako poprzednią
  }

  // Twój pozostały kod
  // ...
	// Remember last CLK state
	lastStateCLK = currentStateCLK;

	// Read the button state
	int btnState = digitalRead(SW);

	//If we detect LOW signal, button is pressed
	if (btnState == LOW) {
		//if 50ms have passed since last LOW pulse, it means that the
		//button has been pressed, released and pressed again
		if (millis() - lastButtonPress > 50) {
			Serial.println("Button pressed!");
		}

		// Remember last button press event
		lastButtonPress = millis();
	}

	// Put in a slight delay to help debounce the reading
	delay(1);
}
void checkNrBankLimits()
{
  // Sprawdź, czy rzeczywista wartość realNrBank przekroczyła 256 lub spadła poniżej 1
  if (realNrBank > 256){
    realNrBank = 1;
    counter = 1;
  }
  if (realNrBank < 1){
    realNrBank = 256;
    counter = 256;
  }
}
// $$$$$$$$$$$$$$$$$$$$$$ GDY MIDI ON $$$$$$$$$$$$$$$$$$$
//__________________________________________________________
//
// MyHandleNoteON is the function that will be called by the Midi Library
// when a MIDI NOTE ON message is received.
// It will be passed bytes for Channel, Pitch, and Velocity

void MyHandleNoteOn(byte channel, byte pitch, byte velocity) {

  if (pitch == 87){              // jeśli nacisnięto SET (S)
    digitalWrite(SetPin, LOW);  // Włączenie SET
  }
   
   if (pitch == 89) {
    digitalWrite(CrescendoPin, HIGH); // Anulowanie działania crescendo (A)

     BAS_channel_1 =   storeBAS_channel_1;
     TREBL_channel_1 = storeTREBL_channel_1;

     displayNrComb = storeDisplayNrComb;
     realNrComb = storeRealNrComb;    
     displayNrBank = storeDisplayNrBank;
     realNrBank = storeRealNrBank;
     nr = storeNr;
     
     lcd.setCursor(0, 0);
     lcd.print("Comb nr         "); // Czyszczenie starej wartości
     lcd.setCursor(8, 0);
     lcd.print(nr);

  }
{
   if (pitch == 88) {
    digitalWrite(CrescendoPin, LOW); // Włączenie crescendo (C)
    MIDI.sendNoteOn(88, 127, 13); // komunikat midiOn na 13 kanale dla włączenia LED pistons Cresc (R)
    crescTouche = true; // dopuki trzymamy przycisk C ta flaga jest true
    if (digitalRead(SetPin) == HIGH)
    storeBAS_channel_1 = BAS_channel_1;
    storeTREBL_channel_1 = TREBL_channel_1;

    storeDisplayNrComb = displayNrComb;
    storeRealNrComb = realNrComb;    
    storeDisplayNrBank = displayNrBank;
    storeRealNrBank =  realNrBank;
    storeNr = nr;
    lcd.setCursor(0, 0);
    lcd.print("Cresc         "); // Czyszczenie starej wartości
    lcd.setCursor(8, 0);
    lcd.print(crescendoGradus);
    crescReadToSDCard();
    }
 
  }
 
 
  // *************************************Sekcja gdy ncisięty prycisk SET******************
  if (digitalRead(SetPin) == LOW){
   
    // Programowanie ctescendo
    if (pitch == 90){
     // if (CrescendoPin == LOW && crescTouche == true){
       // progCrescFlag = 1;
        progCrescendo();
       
      //}
    }

    // zapamiętanie wolnych kombinacji nr...
    if (pitch == 76){
    nr = 1;
    writeToSDCard();
    MIDI.sendNoteOn(75 + nr, 127, 13);
 
  }
    if (pitch == 77){
    nr = 2;
    writeToSDCard();
    MIDI.sendNoteOn(75 + nr, 127, 13);
 
 
  }
    if (pitch == 78){
    nr = 3;
    writeToSDCard();
    MIDI.sendNoteOn(75 + nr, 127, 13);
 
 
  }
    if (pitch == 79){
    nr = 4;
    writeToSDCard();
    MIDI.sendNoteOn(75 + nr, 127, 13);
 
 
  }
    if (pitch == 80){
    nr = 5;
    writeToSDCard();
    MIDI.sendNoteOn(75 + nr, 127, 13);
 
 
  }
    if (pitch == 81){
    nr = 6;
    writeToSDCard();
    MIDI.sendNoteOn(75 + nr, 127, 13);

 
 
  }
    if (pitch == 82){
    nr = 7;
    writeToSDCard();
    MIDI.sendNoteOn(75 + nr, 127, 13);
 
 
  }
    if (pitch == 83){
    nr = 8;
    writeToSDCard();
    MIDI.sendNoteOn(75 + nr, 127, 13);
 
 
  }


  // przycisk > (next) + SET zmiana banku na plus
       if (pitch == 85){
       

         
   
 
      realNrBank++;
      if (realNrBank > 256){
        realNrBank = 1;
        displayNrBank = realNrBank;
      }

    if (digitalRead(CrescendoPin) == HIGH ){  
    readToSDCard();
    }else{
      lcd.setCursor(0, 0);
      lcd.print("Step          "); // Czyszczenie starej wartości
      lcd.setCursor(8, 0);
      lcd.print(realNrBank);
    }
  }
//prycisk < (previous) + SET - zmiana banku na minus
    if (pitch == 86){
 
      realNrBank--;
      if (realNrBank < 1){
        realNrBank = 256;
        displayNrBank = realNrBank;
   
    }
     if (digitalRead(CrescendoPin) == HIGH ){  
     readToSDCard();
      }else{
      lcd.setCursor(0, 0);
      lcd.print("Step          "); // Czyszczenie starej wartości
      lcd.setCursor(8, 0);
      lcd.print(realNrBank);
    }
  }
}

//**************KONIEC sekcji z nacisnietym przyciskiem SET*****************
  // włącznie kombinaci CombON  
  if (pitch == 76){
    digitalWrite(CrescendoPin, HIGH); // Anulowanie działania crescendo (A)
    nr = 1;
    readToSDCard();
    MIDI.sendNoteOn(75 + nr, 127, 13);
 
  }
   if (pitch == 77){
    digitalWrite(CrescendoPin, HIGH); // Anulowanie działania crescendo (A)
    nr = 2;
    readToSDCard();
    MIDI.sendNoteOn(75 + nr, 127, 13);
 
  }
     if (pitch == 78){
      digitalWrite(CrescendoPin, HIGH); // Anulowanie działania crescendo (A)
    nr = 3;
    readToSDCard();
    MIDI.sendNoteOn(75 + nr, 127, 13);
 
  }
     if (pitch == 79){
      digitalWrite(CrescendoPin, HIGH); // Anulowanie działania crescendo (A)
    nr = 4;
    readToSDCard();
    MIDI.sendNoteOn(75 + nr, 127, 13);
 
  }
     if (pitch == 80){
      digitalWrite(CrescendoPin, HIGH); // Anulowanie działania crescendo (A)
    nr = 5;
    readToSDCard();
    MIDI.sendNoteOn(75 + nr, 127, 13);
 
  }
     if (pitch == 81){
      digitalWrite(CrescendoPin, HIGH); // Anulowanie działania crescendo (A)
    nr = 6;
    readToSDCard();
    MIDI.sendNoteOn(75 + nr, 127, 13);
 
  }
     if (pitch == 82){
      digitalWrite(CrescendoPin, HIGH); // Anulowanie działania crescendo (A)
    nr = 7;
    readToSDCard();
    MIDI.sendNoteOn(75 + nr, 127, 13);
 
  }
     if (pitch == 83){
      digitalWrite(CrescendoPin, HIGH); // Anulowanie działania crescendo (A)
    nr = 8;
    readToSDCard();
    MIDI.sendNoteOn(75 + nr, 127, 13);
 
  }
  // ksownik
     if (pitch == 84){
      digitalWrite(CrescendoPin, HIGH); // Anulowanie działania crescendo (A)
    //nr = 0;
    //readToSDCard();
    BAS_channel_1=0;
    TREBL_channel_1=0;
    lcd.setCursor(0, 0);
    lcd.print("Kasownik  0     ");
    MIDI.sendNoteOn(84, 127, 13);

   
 

  }
if (digitalRead(SetPin) == HIGH){
  // przycisk > (next)
       if (pitch == 85){
        digitalWrite(CrescendoPin, HIGH); // Anulowanie działania crescendo (A)
    nr++;
    if (nr > 8 ){
      nr = 1;
      realNrBank++;
      if (realNrBank > 256){
        realNrBank = 1;
        displayNrBank = realNrBank;
      }
    }
    readToSDCard();
    MIDI.sendNoteOn(75 + nr, 127, 13);
 
  }
//prycisk < (previous)
    if (pitch == 86){
       digitalWrite(CrescendoPin, HIGH); // Anulowanie działania crescendo (A)
    nr--;
    if (nr < 1 ){
      nr = 8;
      realNrBank--;
      if (realNrBank < 1){
        realNrBank = 256;
        displayNrBank = realNrBank;
      }
    }
    readToSDCard();
    MIDI.sendNoteOn(75 + nr, 127, 13);
 
  }
}
//----------------Aktualizacja flag kanałwych------------  
  if (channel == 1){ //ustawianie flag dla kanału 1
   
          if (pitch >= 36 && pitch <= 67){
             if ((bitRead(BAS_channel_1, pitch - 36)) == LOW){
             bitSet(BAS_channel_1, pitch - 36);
             } else {
               bitClear(BAS_channel_1, pitch - 36);
               }
          }
 
          if (pitch >= 68 && pitch <= 100){
            if ((bitRead(TREBL_channel_1, pitch - 68)) == LOW){
            bitSet(TREBL_channel_1, pitch - 68);
            } else {
              bitClear(TREBL_channel_1, pitch - 68);
            }
          }
      }
//-----------------------------Koniec Aktualizacji flag Kanałowych--------------          
}

//$$$$$$$$$$$$$$$$$ KONIEC midi IN $$$$$$$$$$$$$$$$$$$$$$
//______________________________________________________

// ###################### GDY MIDI OFF ###################
// MyHandleNoteOFF is the function that will be called by the Midi Library
// when a MIDI NOTE OFF message is received.
// * A NOTE ON message with Velocity = 0 will be treated as a NOTE OFF message *
// It will be passed bytes for Channel, Pitch, and Velocity

void MyHandleNoteOff(byte channel, byte pitch, byte velocity) {
  if (pitch == 88){
    crescTouche = false; // gdy puścimy  przycisk C ta flaga staje się false
    progCrescFlag = 0;
  }


  if (pitch == 87){
    digitalWrite(SetPin, HIGH);
 
  }

/*
  // Obsługa noteOFF
  if (channel == 1) {
   
   if (pitch >= 36 && pitch <= 67){ // gaszenie flag Bas
      bitClear(BAS_channel_1, pitch - 36);
    }
 
    if (pitch >= 68 && pitch <= 101){ // gaszenie flag Trebll
      bitClear(TREBL_channel_1, pitch - 68);
    }      
  }
  */  
}
// #################### Koniec midi off ###############
void senderMidi(){
  int pitch = 0;
  int channel = 1;
  int velocity = 127;
  if (prev_BAS_channel_1 != BAS_channel_1) {
 
     for (int i = 36; i <= 67; i++){
          if ((bitRead(BAS_channel_1, i - 36) == HIGH) && (bitRead(prev_BAS_channel_1, i - 36) == LOW)) {
          pitch =  i;
          channel = 1;
          MIDI.sendNoteOn (pitch, velocity, channel);
          }
          if ((bitRead(BAS_channel_1, i - 36) == LOW) && (bitRead(prev_BAS_channel_1, i - 36) == HIGH)) {
          pitch =  i;
          channel = 1;
          MIDI.sendNoteOff (pitch, velocity, channel);
          }
      }
    prev_BAS_channel_1 = BAS_channel_1;
    }


   if (prev_TREBL_channel_1 != TREBL_channel_1) {
 
       for (int i = 68; i <= 100; i++){
          if ((bitRead(TREBL_channel_1, i - 68) == HIGH) & (bitRead(prev_TREBL_channel_1, i - 68) == LOW)) {
          pitch =  i;
          channel = 1;
          MIDI.sendNoteOn (pitch, velocity, channel);
          }
          if ((bitRead(TREBL_channel_1, i - 68) == LOW) & (bitRead(prev_TREBL_channel_1, i - 68) == HIGH)) {
          pitch =  i;
          channel = 1;
          MIDI.sendNoteOff (pitch, velocity, channel);
          }
      }
   prev_TREBL_channel_1 = TREBL_channel_1;
   }

}



void writeToSDCard(){
   char fileName[13]; // Długość nazwy pliku nie przekracza 8 znaków + 1 znak rozszerzenia + 4 znaki numeru pliku (1, 2, 3...)
   sprintf(fileName, "C%dB%d.txt", nr, realNrBank);
   if (SD.exists(fileName)){
     SD.remove(fileName);
   }
   dataFile = SD.open(fileName, FILE_WRITE);

     if (dataFile) {
      dataFile.print(BAS_channel_1);
      dataFile.print(",");
      dataFile.println(TREBL_channel_1);
      dataFile.close();
      lcd.home();
      lcd.print("Zapisano OK.");
      delay(100);
  } else {
    lcd.home();
    lcd.print("Error open file!");
      delay(100);
 
  }
lcd.setCursor(0, 0);
lcd.print("Comb nr         "); // Czyszczenie starej wartości
lcd.setCursor(8, 0);
lcd.print(nr);
}

void readToSDCard(){
  char fileName[13]; // Długość nazwy pliku nie przekracza 8 znaków + 1 znak rozszerzenia + 4 znaki numeru pliku (1, 2, 3...)
  sprintf(fileName, "C%dB%d.txt", nr, realNrBank);
  dataFile = SD.open(fileName);

  if (dataFile) {
    // Odczytaj dane z pliku i przypisz je do zmiennych BAS_channel_1 iTREBL_channel_1
    char buffer[30]; // Bufor na odczytane dane
    dataFile.readBytesUntil('\n', buffer, sizeof(buffer));

    // Użyj funkcji sscanf do analizy danych w buforze
    sscanf(buffer, "%lu,%lu", &BAS_channel_1, &TREBL_channel_1);

    dataFile.close();
  } else {
    lcd.home();
    lcd.print("File no exist. Tworzę nowy plik.");

    // Jeśli plik nie istnieje, utwórz go i wypełnij zerami
    dataFile = SD.open(fileName, FILE_WRITE);
    if (dataFile) {
      dataFile.print(0);
      dataFile.print(",");
      dataFile.println(0);
      dataFile.close();
      // a nastepnie odczytaj
       if (dataFile) {
       // Odczytaj dane z pliku i przypisz je do zmiennych BAS_channel_1 iTREBL_channel_1
       char buffer[30]; // Bufor na odczytane dane
       dataFile.readBytesUntil('\n', buffer, sizeof(buffer));

       // Użyj funkcji sscanf do analizy danych w buforze
       sscanf(buffer, "%lu,%lu", &BAS_channel_1, &TREBL_channel_1);

       dataFile.close();
    }
    lcd.home();
      lcd.print("Reading OK.");
  } else {
    lcd.home();
    lcd.print("Error open file!");
    }
  }

lcd.setCursor(0, 0);
lcd.print("Comb nr         "); // Czyszczenie starej wartości
lcd.setCursor(8, 0);
lcd.print(nr);


}
void cresc(){
  if (digitalRead(CrescendoPin) == LOW && digitalRead(SetPin) == HIGH){
   
     crescendoGradus = ((analogRead(analogCrescendoReadPin) * 20.0) / 1024.0);
     if (crescendoGradus != crescendoGradusPrev){
      //tu wykonue procedure odczytu ustawień z karty SC dla cresc
      crescReadToSDCard();
     
    }
  }
crescendoGradusPrev = crescendoGradus;
}

void crescReadToSDCard(){
  char fileName[13]; // Długość nazwy pliku nie przekracza 8 znaków + 1 znak rozszerzenia + 4 znaki numeru pliku (1, 2, 3...)
  sprintf(fileName, "Cr%d.txt", crescendoGradus);
  dataFile = SD.open(fileName);

  if (dataFile) {
    // Odczytaj dane z pliku i przypisz je do zmiennych BAS_channel_1 iTREBL_channel_1
    char buffer[30]; // Bufor na odczytane dane
    dataFile.readBytesUntil('\n', buffer, sizeof(buffer));

    // Użyj funkcji sscanf do analizy danych w buforze
    sscanf(buffer, "%lu,%lu", &BAS_channel_1, &TREBL_channel_1);

    dataFile.close();
  } else {
    lcd.home();
    lcd.print("File no exist. Tworzę nowy plik.");

    // Jeśli plik nie istnieje, utwórz go i wypełnij zerami
    dataFile = SD.open(fileName, FILE_WRITE);
    if (dataFile) {
      dataFile.print(0);
      dataFile.print(",");
      dataFile.println(0);
      dataFile.close();
      // a nastepnie odczytaj
       if (dataFile) {
       // Odczytaj dane z pliku i przypisz je do zmiennych BAS_channel_1 iTREBL_channel_1
       char buffer[30]; // Bufor na odczytane dane
       dataFile.readBytesUntil('\n', buffer, sizeof(buffer));

       // Użyj funkcji sscanf do analizy danych w buforze
       sscanf(buffer, "%lu,%lu", &BAS_channel_1, &TREBL_channel_1);

       dataFile.close();
    }
    lcd.home();
      lcd.print("Zapisano OK.");
  } else {
    lcd.home();
    lcd.print("Error open file!");
    }
  }

lcd.setCursor(0, 0);
lcd.print("Cresc         "); // // zapis do LCD
lcd.setCursor(8, 0);
lcd.print(crescendoGradus);


}
void progCrescendo(){
 // if (progCrescFlag = 1){
  if (realNrBank <= 20 && realNrBank > 0){
     lcd.setCursor(0, 0);
     lcd.print("ProgCrescStep        "); // zapis do LCD
     lcd.print(realNrBank);
     delay (100);
     crescendoWriteToSDCard();
     }
 // progCrescFlag = 0;
 //}
}



void crescendoWriteToSDCard(){          // programowanie crescendo - procedura zapisu do  SD_card
 
   char fileName[13]; // Długość nazwy pliku nie przekracza 8 znaków + 1 znak rozszerzenia + 4 znaki numeru pliku (1, 2, 3...)
   sprintf(fileName, "Cr%d.txt", realNrBank);
   if (SD.exists(fileName)){
     SD.remove(fileName);
   }
   dataFile = SD.open(fileName, FILE_WRITE); // otwarcie  pliku
   

     if (dataFile) {
                                  // zapis to SD_card -
      dataFile.print(BAS_channel_1);
      dataFile.print(",");
      dataFile.println(TREBL_channel_1);
      dataFile.close();         // zamkniecie pliku
      lcd.home();               // zapis do LCD -
      lcd.print("Zapisano OK.");
      delay(100);
  } else {
    lcd.home();
    lcd.print("Error open file!");
   delay (100);
  }
   lcd.setCursor(0, 0);         // zapis do LCD -
   lcd.print("Cresc         ");
   lcd.setCursor(8, 0);
   lcd.print(crescendoGradus);
 
     lcd.setCursor(0, 0);       // zapis do LCD -
     lcd.print("Zap. cresc st        ");
     lcd.setCursor(14, 0);
     lcd.print(realNrBank);
     delay(100);
         realNrBank++;

 
}