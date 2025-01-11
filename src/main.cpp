/**
 * 2024-12-14
 * LED MATRIX + WEMOS
 * 
 * 
 * questa versione quando è posizionata in verticale è instabile
 */

#include<Wire.h>

const int MPU = 0x68;  // I2C address of the MPU-6050

#include <Adafruit_GFX.h>
#include <Adafruit_NeoMatrix.h>
#include <Adafruit_NeoPixel.h>

#include "ap-wifi.h"

#define DATA_PIN D6
#define BTN_PIN D7

#define NUM_PIXELS 512

#define MATRIX_WIDTH 32
#define MATRIX_HEIGHT 16
#define DOTS 50

#define FRICTION 0.85

// Example for NeoPixel Shield.  In this application we'd like to use it
// as a 5x8 tall matrix, with the USB port positioned at the top of the
// Arduino.  When held that way, the first pixel is at the top right, and
// lines are arranged in columns, progressive order.  The shield uses
// 800 KHz (v2) pixels that expect GRB color data.          NEO_MATRIX_COLUMNS + NEO_MATRIX_ZIGZAG,
Adafruit_NeoMatrix matrix = Adafruit_NeoMatrix(32, 8, 1, 2, DATA_PIN,
                            NEO_MATRIX_TOP     + NEO_MATRIX_LEFT +
                            NEO_MATRIX_COLUMNS    + NEO_MATRIX_ZIGZAG,
                            NEO_GRB            + NEO_KHZ800);

int16_t AcX, AcY, AcZ, Tmp, GyX, GyY, GyZ;

struct Point {
  float x;  // posizione nella matrice che tiene conto dei microspostamenti
  float y;
  int px;   // posizione intera nella matrice
  int py;   
  float vx; // vettori delle velocità
  float vy;
};


Point points[DOTS];

int occupancy[32][16];

const uint16_t colors[] = {
  matrix.Color(255, 0, 0), matrix.Color(0, 255, 0), matrix.Color(0, 0, 255), matrix.Color(255, 255, 0), matrix.Color(0, 255, 255), matrix.Color(255, 0, 255), matrix.Color(255, 255, 255)
};


// flag for File SYstem activation
bool FSActive = false;

// timer for showing text 1 char at a second
unsigned long timerSec = 0;

bool flagUpdated = false;


void initializePoints() {
  for (int x = 0; x < MATRIX_WIDTH; x++) {
      for (int y = 0; y < MATRIX_HEIGHT; y++) {
        occupancy[x][y] = -1;
      }
  }
  
  for (int i = 0; i < DOTS; i++) {
    bool isUnique;
    do {
      isUnique = true;
      // Genera valori casuali per x e y attorno al centro (approssimativamente 16, 8)
      points[i].x = random(0, MATRIX_WIDTH );
      points[i].y = random(0, MATRIX_HEIGHT);
      points[i].px = points[i].x;
      points[i].py = points[i].y;
      points[i].vx = 0;
      points[i].vy = 0;
      

      // Controlla se il nuovo punto è sovrapposto a uno già esistente
      for (int j = 0; j < i; j++) {
        if ((int)points[i].x == (int)points[j].x && (int)points[i].y == (int)points[j].y) {
          isUnique = false;
          break;
        }
      }
    } while (!isUnique); // Ripete finché non trova una posizione unica

    occupancy[points[i].px][points[i].py] = i;
  }
  
}

float maxsq = 0;
uint16_t calculateColor(float vx, float vy) {
  // Calcola la velocità al quadrato
  float speedSq = vx * vx + vy * vy;
  if (speedSq > maxsq) {
    maxsq = speedSq; // Aggiorna la velocità massima osservata
  }
  
  // Definisci il massimo valore possibile di velocità al quadrato
  float maxSpeedSq = 15.0; // Quadrato della velocità massima (ad esempio, 5^2)

  // Normalizza la velocità al quadrato tra 0 e 255
  uint8_t normalizedSpeed =  constrain(map(speedSq, 0, maxSpeedSq, 0, 255),0,255);

  // Genera i colori in base alla velocità:
  // Blu dominante per velocità basse, rosso dominante per velocità alte
  uint8_t red = normalizedSpeed;          // Aumenta il rosso con la velocità
  uint8_t blue = 255 - normalizedSpeed;   // Diminuisce il blu con la velocità
  uint8_t green = 0;                      // Verde costante (puoi modificarlo se necessario)

  // Restituisci il colore calcolato
  return matrix.Color(red, green, blue);
}


//
// LEDs in the global strip
uint32_t leds[NUM_PIXELS];

// Setup variables for the rainbow effect
uint8_t hue = 0;

void rainbowEffect() {
    // Ciclo attraverso tutti i LED della striscia
    for (int i = 0; i < NUM_PIXELS; i++) {
        leds[i] = matrix.ColorHSV( hue * (255.0*i / NUM_PIXELS), 255, atoi(userdata.brightness) );
    }

    for(int j=0; j<NUM_PIXELS; j++) matrix.setPixelColor(j, leds[j]);
    matrix.show();  // Mostra i colori sulla striscia
    hue++;           // Incrementa la tonalità per creare l'effetto di movimento
    delay(10);  // Ritardo per controllare la velocità dell'effetto
}



uint32_t setPixelHeatColor(byte temperature) {
  // Rescale heat from 0-255 to 0-191
  byte t192 = round((temperature / 255.0) * 191);
  
  // Calculate ramp up from
  byte heatramp = t192 & 0x3F; // 0...63
  heatramp <<= 2; // scale up to 0...252
  
  // Figure out which third of the spectrum we're in:
  if(t192 > 0x80) {                    // hottest
    return matrix.Color(255, 255, heatramp);
  }
  else if(t192 > 0x40) {               // middle
    return matrix.Color(255, heatramp, 0);
     }
  else {                               // coolest
    return matrix.Color(heatramp, 0, 0);
  }
}

// FlameHeight - Use larger value for shorter flames, default=50.
// Sparks - Use larger value for more ignitions and a more active fire (between 0 to 255), default=100.
// DelayDuration - Use larger value for slower flame speed, default=10.

void Fire(int FlameHeight, int Sparks, int DelayDuration) {
  static byte heat[MATRIX_WIDTH][MATRIX_HEIGHT];
  int cooldown;
  
  // Cool down each cell a little
  for(int i = 0; i < MATRIX_HEIGHT; i++) {
    cooldown = random(0, ((FlameHeight * 10) / MATRIX_HEIGHT) + 2);
    if(cooldown > heat[0][i]) {
      heat[0][i] = 0;
    }
    else {
      heat[0][i] = heat[0][i] - cooldown;
    }
  }
  
  // Heat from each cell drifts up and diffuses slightly
  for(int k = (MATRIX_HEIGHT - 1); k >= 2; k--) {
    heat[0][k] = (heat[0][k - 1] + heat[0][k - 2] + heat[0][k - 2]) / 3;
  }
  
  // Randomly ignite new Sparks near bottom of the flame
  if(random(255) < Sparks) {
    int y = random(7);
    heat[0][y] = heat[0][y] + random(160, 255);
  }
  
  // Convert heat to LED colors
  for(int j = 0; j < MATRIX_HEIGHT; j++) {
    matrix.drawPixel(0,MATRIX_HEIGHT - j, setPixelHeatColor(heat[0][j]));
  }
  
  // for(int j=0; j<NUM_PIXELS; j++) matrix.setPixelColor(j, leds[j]);
  matrix.show();
  delay(DelayDuration);
}


void pixelRun() {
  for(int i = 0; i < NUM_PIXELS; i++) {
    leds[i] = 0;
  }
  int k = (millis() / 50) % NUM_PIXELS;
  leds[k] = Adafruit_NeoPixel::Color(255,255,255);
  for(int j=0; j<NUM_PIXELS; j++) matrix.setPixelColor(j, leds[j]);
  matrix.show();
}


//Funzione per il calcolo degli angoli Pitch e Roll
float FunctionsPitchRoll(float A, float B, float C) {
  float DatoA, DatoB, Value;
  DatoA = A;
  DatoB = (B * B) + (C * C);
  DatoB = sqrt(DatoB);

  Value = atan2(DatoA, DatoB);
  Value = Value * 180 / 3.14;

  return Value;
}



void liquidPixels() {
  Wire.beginTransmission(MPU);
  Wire.write(0x3B); // starting with register 0x3B (ACCEL_XOUT_H)
  Wire.endTransmission(false);
  Wire.requestFrom((uint8_t)MPU,(size_t)14,true); // request a total of 14 registers
  AcX=Wire.read()<<8|Wire.read(); // 0x3B (ACCEL_XOUT_H) & 0x3C (ACCEL_XOUT_L)
  AcY=Wire.read()<<8|Wire.read(); // 0x3D (ACCEL_YOUT_H) & 0x3E (ACCEL_YOUT_L)
  AcZ=Wire.read()<<8|Wire.read(); // 0x3F (ACCEL_ZOUT_H) & 0x40 (ACCEL_ZOUT_L)
  //Tmp=Wire.read()<<8|Wire.read(); // 0x41 (TEMP_OUT_H) & 0x42 (TEMP_OUT_L)
  //GyX=Wire.read()<<8|Wire.read(); // 0x43 (GYRO_XOUT_H) & 0x44 (GYRO_XOUT_L)
  //GyY=Wire.read()<<8|Wire.read(); // 0x45 (GYRO_YOUT_H) & 0x46 (GYRO_YOUT_L)
  //GyZ=Wire.read()<<8|Wire.read(); // 0x47 (GYRO_ZOUT_H) & 0x48 (GYRO_ZOUT_L)
 
  
  // calibration 
  // Roll:3.58  Pitch: -4.30

  int Roll = int(FunctionsPitchRoll(AcX, AcY, AcZ) - 3.58);   //Calcolo angolo Roll
  int Pitch = int(FunctionsPitchRoll(AcY, AcX, AcZ) + 4.30);  //Calcolo angolo Pitch
  // Serial.println("Roll:" + (String)Roll +"  Pitch: " + (String)Pitch) ;
  byte btn = digitalRead(BTN_PIN);


  // Calcolo dei vettori di movimento dovuti all'inclinazione del dispositivo
  float dx = -sin(Roll * PI / 180.0);
  float dy = sin(Pitch * PI / 180.0);
   
  matrix.fillScreen(0);

  
  for (int i = 0; i < DOTS; i++) {

    float tx = points[i].x + points[i].vx;
    float ty = points[i].y + points[i].vy;

    // new position to check
    int ntx = round(tx);
    int nty = round(ty);

    if ( (-1 == occupancy[ntx][nty] || occupancy[ntx][nty] == i) && ntx >=0 && ntx < 32 && nty >=0 && nty < 16) {
      occupancy[points[i].px][points[i].py] = -1;
      // free, new position is available
      points[i].x = tx;
      points[i].y = ty;
      points[i].px = ntx;
      points[i].py = nty;
      points[i].vx = (points[i].vx  + dx)* FRICTION;
      points[i].vy = (points[i].vy  + dy)* FRICTION;
      occupancy[ntx][nty] = i;
      //Serial.println(" FREE");
    } else {

      // forse questo bloco va fatto prima e poi
      // va fatto il riposizionamento.
      // in entrambi i casi devo ricalcolare posizioni, non ha senso che qui faccio solo vettori
      // (o forse ha senso per risparmiare calcoli)


      if (ntx<0 || ntx>31 || nty<0 || nty>15) {

        if (ntx > 31) {
          points[i].vx = -points[i].vx * FRICTION;
        }
        if (ntx < 0) {
          points[i].vx = -points[i].vx * FRICTION;
        }

        if (nty > 15.0) {
          points[i].vy = -points[i].vy * FRICTION;
        }
        if (nty < 0.0) {
          points[i].vy = -points[i].vy * FRICTION;
        }

      } else {
        
        int j = occupancy[ntx][nty];
        // CALC relative velocity for collision
        float Dvx= points[j].vx  -points[i].vx;
        float Dvy= points[j].vy  -points[i].vy;

        points[i].vx = (points[i].vx + Dvx)* FRICTION ;
        points[i].vy = (points[i].vy + Dvy)* FRICTION ;
        
        points[j].vx = (points[j].vx  - Dvx) * FRICTION;
        points[j].vy = (points[j].vy  - Dvy) * FRICTION;          

      }
    
      
    }
  }

  for (int i = 0; i < DOTS; i++) {
    uint16_t color = calculateColor(points[i].vx, points[i].vy);
    matrix.drawPixel(points[i].px, points[i].py, color);
  }
  matrix.show();

}

int scrollXpos = 32767;
void setString(String s, uint32_t color, bool scroll) {
    // char per line
    matrix.setTextSize(1);
    matrix.setTextColor( color );
    matrix.clear();
    if(s.length()*6 <= MATRIX_WIDTH) {
      // scritta corta
      int totPix = 6*s.length();
      int px = (MATRIX_WIDTH - totPix ) / 2;
      matrix.setCursor(px, MATRIX_HEIGHT/2 - 4 );
      matrix.print( s );
      matrix.show();
    } else {
      if(!scroll) {
        // non scrollare
        matrix.setCursor(0,0);
        matrix.setTextWrap(true);
        matrix.print( s );
        matrix.show();
      } else {
        // scrollare
        matrix.setTextWrap(false);
        int totScroll = 6*s.length();
        if(scrollXpos == 32767) {
          scrollXpos = MATRIX_WIDTH;
        }

        matrix.clear();
        matrix.setCursor(scrollXpos,MATRIX_HEIGHT/2 - 4);
        matrix.print( s );
        matrix.show();
        
        scrollXpos--;
        if (scrollXpos == -totScroll)
        {
          scrollXpos = MATRIX_WIDTH;
        }
        

        

      }

    } 
}




void resetFlag(int i) {
  Serial.println("resetFlag = " + String(i));
  if( i == 5) {
    flagUpdated = false;
    Serial.println("flagUpdated reset");
    matrix.clear();
    matrix.setBrightness(atoi(userdata.brightness));
    delay(1000);
    // show();
  }

  // printDebug();
}

// Funzione per convertire un colore esadecimale in un valore a 32 bit
uint32_t hexToColor(const String &hexColor) {
    // Rimuovi il simbolo '#' dalla stringa, se presente
    String cleanHex = hexColor.startsWith("#") ? hexColor.substring(1) : hexColor;

    // Converti i componenti RGB
    uint8_t r = strtol(cleanHex.substring(0, 2).c_str(), nullptr, 16);
    uint8_t g = strtol(cleanHex.substring(2, 4).c_str(), nullptr, 16);
    uint8_t b = strtol(cleanHex.substring(4, 6).c_str(), nullptr, 16);

    // Serial.println("R: " + String(r) + " G: " + String(g) + " B: " + String(b));

    // Usa la funzione Color della libreria NeoPixel
    return matrix.Color(r, g, b);
}

void show() { 


  // the settings page is saved, we can show something
  if(userdata.check[1] == '1') {

    // // text char by char in a second
    // if((userdata.effect[0] == 't')) {
    //   if ( !flagUpdated ) {
    //     String s = String(userdata.text);
    //     int i = (millis() / 1000 ) % s.length();
    //     Serial.println(s.charAt(i));
    //     matrix.clear();
    //     delay(50);
    //     // @todo
        
    //     // setDigit(0, s.charAt(i), Adafruit_NeoPixel::Color(255,255,255), leds);
    //     for(int j=0; j<NUM_PIXELS; j++) matrix.setPixelColor(j, leds[j]);
    //     matrix.show();
    //     delay(50);

    //     flagUpdated = true;           
    //   } else {
    //     if(millis() > timerSec) {
    //       flagUpdated = false;
    //       timerSec = millis() + 1000;
    //     }
    //   }
    // }
  
    // show the text (if the text is longer than 1 char)
    if((userdata.effect[0] == 'x' || userdata.effect[0] == 's') && !flagUpdated) {
      
      uint32_t color = hexToColor(userdata.color);
      
      String s = String(userdata.text);
      if( userdata.effect[0] == 'x' ) {
        setString(s, color, false);
        flagUpdated = true;
        if(!AP_STATUS) { delay(70);  }
      } else {
        // scroll
        setString(s, color, true);
        flagUpdated = false;
        if(!AP_STATUS) { delay(70);  }
      }

    } 
    if(userdata.effect[0] == 'r'){
      rainbowEffect();
      flagUpdated = false;
    } 

    if(userdata.effect[0] == 'f'){
      Fire(50,50,10);
      flagUpdated = false;
    } 

    if(userdata.effect[0] == 'p'){
      pixelRun();
      flagUpdated = false;
    } 

    if(userdata.effect[0] == 'l'){
      liquidPixels();
      flagUpdated = false;
    } 
    
  }


  int x = digitalRead(BTN_PIN);
  if( x == 0 ) {
    Serial.println("Button pressed");
    if(!AP_STATUS) {
      Serial.println("Start AP");
      setupPortal([]() { show(); });
    }
  }

}






void setup() {

  Serial.begin(115200);
  Serial.println("Hello World!");

  //
  // turn off the blue led of Wemos power
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

  //
  // accelerometer connection
  Wire.begin();
  Wire.beginTransmission(MPU);
  Wire.write(0x6B);  // PWR_MGMT_1 register
  Wire.write(0);     // set to zero (wakes up the MPU-6050)
  Wire.endTransmission(true);

  // read data from EEPROM
  readUserData();

  //
  // button
  pinMode(BTN_PIN,INPUT_PULLUP);

  //
  // LED matrix initialization
  initializePoints();
  matrix.begin();
  matrix.setBrightness(atoi(userdata.brightness));
  matrix.clear();
  delay(500);

  Serial.println( atoi(userdata.brightness) );

  //
  // LittleFS
  // Start the file subsystem, used to store HTML
  // CSS and other files for captive portal AP
  if (LittleFS.begin()) {
      Serial.println("LittleFS Active");
      FSActive = true;
  } else {
      Serial.println("Unable to activate LittleFS");
  }


  //
  // Try to use user an password to connect
  // to internet, result is stored in global wifi var
  connectToWifi();

  //
  // if wifi access is available, set time and date
  // from internet
  if(WIFI_STATUS) {

    // OK @todo set time and date

  } else {
    // NO wifi
  }  
}



void loop() {
  // display_scrollText();
  show( );
  // Fire(MATRIX_HEIGHT, 50, 10);
  // setString("Rosso", matrix.Color(255,0,0), false);
  // setString("Verde", matrix.Color(0,255,0), false);
  // setString("Blu", matrix.Color(0,0,255), false);


}
