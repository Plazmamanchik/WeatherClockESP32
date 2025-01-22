#include <Adafruit_ST7735.h> // Hardware-specific library for ST7735
#include <Adafruit_ST7789.h> // Hardware-specific library for ST7789
#include <SPI.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Arduino.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <SD.h> // Бібліотека для роботи з SD-картою
#include <time.h>
#include <Button.h>
#include <DS18B20.h>
#include <OneWire.h>



// Color definitions
#define BLACK       0x0000
#define NAVY        0x000F
#define DARKGREEN   0x03E0
#define DARKCYAN    0x03EF
#define MAROON      0x7800
#define PURPLE      0x780F
#define OLIVE       0x7BE0
#define LIGHTGREY   0xC618
#define DARKGREY    0x7BEF
#define BLUE        0x001F
#define GREEN       0x07E0
#define CYAN        0x07FF
#define RED         0xF800
#define MAGENTA     0xF81F
#define YELLOW      0xFFE0
#define WHITE       0xFFFF
#define ORANGE      0xFD40
#define GREENYELLOW 0xAFE5
#define PINK        0xF81F

#define BACKGROUND BLACK

//DISPLAY
#define TFT_DC 33//12 //A0
#define TFT_CS 25 //CS
#define TFT_MOSI 22//14 //SDA
#define TFT_CLK 27 //SCK
#define TFT_RST 26


//SD-Card
#define CS 5
#define DI 23
#define DO 19
#define SCK 18


//BUZZER
#define buzzer 14

//BUTTONS
Button SETbtn(34);
Button PLUSbtn(21);
Button MINUSbtn(32);
Button PAGEbtn(35);


DS18B20 dsIN(17);
DS18B20 dsOUT(13);


Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_MOSI, TFT_CLK, TFT_RST);

// WiFi
String ssid = "WeatherESP";
char* password = "";

String ssid_;
String password_;

WebServer webServer(80);

const char* apiKey = "10ae0d692b581fadc3fc40e1b85a807c";  // Ваш API ключ
char* city = "Siegsdorf"; // Назва міста
char* units = "metric";  // Одиниці виміру: "metric" для Цельсія або "imperial" для Фаренгейта



bool do_alarm_clock;//будильнийк

char page = 0;// cnjhsyrf

String timeString;

//SD налаштування часів
bool WifiClock;
bool alarm_clock;


//змінні для офлайн годинника
uint8_t hour = 0;//години
uint8_t minut = 0;//хвилини
uint8_t old_minut;//старі хвилини
uint8_t sec = 0;//секунди

//змінні для будильника
uint8_t AlarmHour = 0;//години
uint8_t AlarmMinut = 0;//хвилини
bool alarmTriggered = false;  // Прапорець для перевірки, чи будильник вже спрацював



char point = '>';//курсор


bool setClock; //встановлення чесу щоб не рухався курсор
uint8_t line_page_1 = 0;//строка по Y на сторінці 1
uint8_t str_time_page_1 = 0;//лінії на строці часу


//затримки для оновлення 
const long intervalWeather = 150 * 1000; //Затримка для оновлення онлайн
unsigned long long previousMillisWeather = 0;//Збереження millis для оновлення погоди 

unsigned long long previousMillisTime = 0;//Збереження millis для оновлення часу онлайн 
unsigned long long previousMillisTimeOffline = 0;//Збереження millis для оновлення годин офлайн



unsigned long long previousAlarmSignal = 0;//Збереження millis для сигналу будильника
unsigned long long previousTemperaturOffline = 0;//Збереження millis для оновлень датчиків температури


float old_dsIN;
float old_dsOUT;
float dsSaveIN = dsIN.getTempC();
float dsSaveOUT = dsOUT.getTempC();


void setup() {
  Serial.begin(115200);
  tft.initR(INITR_BLACKTAB);
  tft.setRotation(1);
  tft.fillScreen(BACKGROUND);
  tft.setCursor(0,0);
    
  if (!SD.begin(CS)) {
    Serial.println("Помилка ініціалізації SD-карти!");
    tone(buzzer,1000,1000);

  }else{
    

    // Перевірка наявності файлу та його створення, якщо він не існує
    if (!SD.exists("/clock_setting.json")) {
      Serial.println("Файл clock_setting.json не знайдено. Створюємо файл...");
      File file = SD.open("/clock_setting.json", FILE_WRITE);
      if (file) {
        // Записуємо стандартні налаштування у файл
        StaticJsonDocument<256> doc;
        doc["alarm_clock"] = false;
        doc["wificlock"] = true;

        doc["AlarmHour"] = 0;
        doc["AlarmMinut"] = 0;
        // Серіалізуємо дані у JSON-формат і записуємо у файл
        if (serializeJson(doc, file) == 0) {
          Serial.println(F("Помилка запису JSON у файл!"));
        }
        file.close();
        Serial.println("Файл успішно створено.");
      } else {
        Serial.println("Не вдалося створити файл!");
      }
    } else {
      Serial.println("Файл clock_setting.json знайдено.");
    }

    

    // Завантажуємо налаштування з файлу
    loadClockSettings();
  }


  WiFi.softAP(ssid, password);
  IPAddress IP = WiFi.softAPIP();
  Serial.print("Access Point IP address: ");
  Serial.println(IP);
  tft.setCursor(35, 5);
  tft.setTextColor(RED);
  tft.setTextSize(1);
  tft.print(IP);


  

  
  SETbtn.begin();
  PLUSbtn.begin();
  MINUSbtn.begin();
  PAGEbtn.begin();


  

  webServer.on("/", handleRoot);
  webServer.on("/connect", handleConnect);
  webServer.begin();

  readStoredCredentials();

  pinMode(buzzer,OUTPUT);
  tone(buzzer,1000,50);
  getWeatherData();
  if(WifiClock){
    displayTimeWiFi();
  }else{
    timeString = String(hour) + ":" + String(minut);
    tft.fillRect(3, 70, 170, 30, BLACK);
    tft.setCursor(30, 70);
    tft.setTextSize(4);
    tft.setTextColor(0x07DF);
    tft.print(timeString);
  }

  getWeatherData();
  
}

void loop() {
  webServer.handleClient();//Веб сторінка
  BTNS();//Кнопки
  RunFunctions();//Отримання данних
}



void loadClockSettings() {
  File file = SD.open("/clock_setting.json", FILE_READ);

  if (!file) {
    Serial.println("Не вдалося відкрити файл clock_setting.json!");
    return;
  }

  // Буфер для зберігання JSON-даних
  StaticJsonDocument<256> doc;
  // Читаємо JSON з файлу
  DeserializationError error = deserializeJson(doc, file);
  if (error) {
    Serial.print(F("Помилка розбору JSON: "));
    Serial.println(error.f_str());
    return;
  }

  file.close();

  // Закриваємо файл після читання
  // Читаємо значення з JSON і присвоюємо змінним
  WifiClock = doc["wificlock"] | true;  // Якщо ключ не знайдено, використаємо значення за замовчуванням (false)
  alarm_clock = doc["alarm_clock"] | false;

  AlarmHour = doc["AlarmHour"];
  AlarmMinut = doc["AlarmMinut"];

  
  Serial.print("Налаштування завантажені: WifiClock = ");
  Serial.print(WifiClock);
  Serial.print(", AlarmClock = ");
  Serial.println(alarm_clock);
  Serial.println("");

  Serial.print("Налаштування завантажені: AlarmHour = ");
  Serial.print(AlarmHour);
  Serial.print(", AlarmMinut = ");
  Serial.println(AlarmMinut);
  Serial.println("");

}








void RunFunctions(){
  if (page == 0){
    if (WiFi.isConnected()) {
      //unsigned long currentMillisWeather = millis();
      if (millis() - previousMillisWeather >= intervalWeather) {
        previousMillisWeather = millis();
        getWeatherData(); // Виводимо погоду
      }
      if(WifiClock){
        if (millis() - previousMillisTime >= 15*1000) {
          previousMillisTime = millis();
          displayTimeWiFi(); // Виводимо час
        }
      }
    }
  }
  if(!WifiClock){
      //Serial.println("fewfwef");
    if (millis() - previousMillisTimeOffline >= 1000) {
      previousMillisTimeOffline = millis();
      sec++;
      if(sec == 60){
        sec = 0;
        minut++;
      }
      if(page == 0){
        if(old_minut != minut){
          timeString = String(hour) + ":" + String(minut);
          tft.fillRect(3, 70, 170, 30, BLACK);
          tft.setCursor(17, 70);
          tft.setTextSize(4);
          tft.setTextColor(0x07DF);
          tft.print(timeString);

          

          old_minut = minut;
        }
      }
     //Serial.print("Sec: ");
     //Serial.println(sec);
     //Serial.print("Min: ");
     //Serial.println(minut);
     //Serial.print("Hour: ");
     //Serial.println(hour);
    }
    if(!setClock){
      if(minut == 60){
        hour++;
        minut = 0;
      }
      if(hour == 24){
        hour = 0;
      }
    }
  }
  
  if(do_alarm_clock){
    if (millis() - previousAlarmSignal >= 1800) {
      previousAlarmSignal = millis();
      tone(buzzer, 2000, 900);
    }
  }
   if (alarm_clock) {
    if (hour == AlarmHour && minut == AlarmMinut && !alarmTriggered) {
      // Виконуємо дію будильника
      Serial.println("Alarm Clock");
      do_alarm_clock = true;
      
      alarmTriggered = true;
    }

    if (hour != AlarmHour || minut != AlarmMinut) {
      alarmTriggered = false;
    }
  }



  if (millis() - previousTemperaturOffline >= 5000) {
      previousTemperaturOffline = millis();

      if(page == 0){
        

        if(old_dsIN != dsSaveIN){
          //Serial.print("In: ");
          //Serial.println(dsSaveIN);
          tft.setTextColor(PINK);
          tft.fillRect(4,44,100,12,BACKGROUND);
          tft.setTextSize(1);
          tft.setCursor(5, 45);
          tft.print("Temp.In: ");
          tft.setTextColor(getColorForTemperature(dsSaveIN));
          tft.print(dsSaveIN);
          tft.setTextColor(PINK);
          tft.print(" C");
          old_dsIN = dsSaveIN;
        }


        if(old_dsOUT != dsSaveOUT){
          Serial.print("Out: ");
          Serial.println(dsSaveOUT);
          tft.setTextColor(PINK);
          tft.fillRect(4,55,100,12,BACKGROUND);
          tft.setTextSize(1);
          tft.setCursor(5, 56);
          tft.print("Temp.Out: ");
          tft.setTextColor(getColorForTemperature(dsSaveOUT));
          tft.print(dsSaveOUT);
          tft.setTextColor(PINK);
          tft.print(" C");
          

          old_dsOUT = dsSaveOUT;
        }
      }

  }


}







void BTNS(){
  if(SETbtn.released()){// -----------------------------------------------------------------SETEbtn------------------------------------------------------------------------------
    tone(buzzer,1500,80);
    do_alarm_clock=false;
    if(page == 1){



      if(line_page_1 == 0){//WiFi Clock
        if(WifiClock){
        WifiClock = false;
        }else{
        WifiClock = true;
        }
        
        tft.fillRect(23,18,55,10,BLACK);
        tft.setCursor(25, 20);
        tft.setTextSize(1);
        if(WifiClock){
        tft.setTextColor(GREEN);
        }else{
        tft.setTextColor(RED);
        }
        tft.print("Wifi Clock");

        File file = SD.open("/clock_setting.json", FILE_WRITE);
        if (file) {
          // Записуємо стандартні налаштування у файл
          StaticJsonDocument<256> doc;
          doc["alarm_clock"] = alarm_clock;
          doc["wificlock"] = WifiClock;
         
          doc["AlarmHour"] = AlarmHour;
          doc["AlarmMinut"] = AlarmMinut;

          // Серіалізуємо дані у JSON-формат і записуємо у файл
          if (serializeJson(doc, file) == 0) {
            Serial.println(F("Помилка запису JSON у файл!"));
          }
          file.close();
        }
      }else if(line_page_1 == 1){//Alarm Clock

        if(alarm_clock){
        alarm_clock = false;
        }else{
        alarm_clock = true;
        }
        tft.fillRect(24,33,65,10,BLACK);
        tft.setCursor(25, 35);
        tft.setTextSize(1);

        if(alarm_clock){
        tft.setTextColor(GREEN);
        }else{
        tft.setTextColor(RED);
        }
        tft.print("Alarm Clock");

        File file = SD.open("/clock_setting.json", FILE_WRITE);
        if (file) {
          // Записуємо стандартні налаштування у файл
          StaticJsonDocument<256> doc;
          doc["alarm_clock"] = alarm_clock;
          doc["wificlock"] = WifiClock;
         
          doc["AlarmHour"] = AlarmHour;
          doc["AlarmMinut"] = AlarmMinut;

          // Серіалізуємо дані у JSON-формат і записуємо у файл
          if (serializeJson(doc, file) == 0) {
            Serial.println(F("Помилка запису JSON у файл!"));
          }
          file.close();
        }




      }else if(line_page_1 == 2){//Налаштування годинника
        if(str_time_page_1 == 0){
          setClock = !setClock;
          Serial.print("setClock: ");
          Serial.println(setClock);
          str_time_page_1++;
          Serial.println("Hours");

          tft.fillRect(23, 47, 35, 13,BACKGROUND);
          tft.setCursor(25, 50);
          tft.setTextColor(BLUE);
          tft.print(hour);
          tft.setTextColor(YELLOW);
          tft.print(":");
          tft.print(minut);

        }else if (str_time_page_1 == 1){
          str_time_page_1++;
          Serial.println("Minutes");

          tft.fillRect(23, 47, 35, 13,BACKGROUND);
          tft.setCursor(25, 50);
          tft.setTextColor(YELLOW);
          tft.print(hour);
          tft.print(":");
          tft.setTextColor(BLUE);
          tft.print(minut);

        }else if (str_time_page_1 == 2){
          str_time_page_1 = 0;    
          Serial.println("Cursor");
          setClock = !setClock;
          
          tft.fillRect(23, 47, 35, 13,BACKGROUND);
          tft.setCursor(25, 50);
          tft.setTextColor(YELLOW);
          tft.print(hour);
          tft.print(":");
          tft.print(minut);

          Serial.print("Minut: ");
          Serial.println(minut);  



        }
        Serial.print("Lines: ");
        Serial.println(str_time_page_1);

      }else if(line_page_1 == 3){//Налаштування будильника
        if(str_time_page_1 == 0){
          setClock = !setClock;
          Serial.print("setClock: ");
          Serial.println(setClock);
          str_time_page_1++;
          Serial.println("HoursAlarm");

          tft.fillRect(23, 62, 35, 13,BACKGROUND);
          tft.setCursor(25, 65);
          tft.setTextColor(BLUE);
          tft.print(AlarmHour);
          tft.setTextColor(YELLOW);
          tft.print(":");
          tft.print(AlarmMinut);

          

        }else if (str_time_page_1 == 1){
          str_time_page_1++;
          Serial.println("Minutes");

          tft.fillRect(23, 62, 35, 13,BACKGROUND);
          tft.setCursor(25, 65);
          tft.setTextColor(YELLOW);
          tft.print(AlarmHour);
          tft.print(":");
          tft.setTextColor(BLUE);
          tft.print(AlarmMinut);

        }else if (str_time_page_1 == 2){
          str_time_page_1 = 0;    
          Serial.println("Cursor");
          setClock = !setClock;
          
          tft.fillRect(23, 62, 35, 13,BACKGROUND);
          tft.setCursor(25, 65);
          tft.setTextColor(YELLOW);
          tft.print(AlarmHour);
          tft.print(":");
          tft.print(AlarmMinut);

          Serial.print("Minut: ");
          Serial.println(AlarmMinut);  



        }
        Serial.print("Lines: ");
        Serial.println(str_time_page_1);
      }


    }
    

  }
  if(PLUSbtn.released()){// -----------------------------------------------------------------PLUSbtn------------------------------------------------------------------------------
    tone(buzzer,1500,80);

        

      




      if(page == 0){
        //Serial.print("In: ");
        //Serial.println(dsSaveIN);
        tft.setTextColor(PINK);
        tft.fillRect(4,44,100,12,BACKGROUND);
        tft.setTextSize(1);
        tft.setCursor(5, 45);
        tft.print("Temp.In: ");
        tft.setTextColor(getColorForTemperature(dsSaveIN));
        tft.print(dsSaveIN);
        tft.setTextColor(PINK);
        tft.print(" C");
        old_dsIN = dsSaveIN;
        Serial.print("Out: ");
        Serial.println(dsSaveOUT);
        tft.setTextColor(PINK);
        tft.fillRect(4,55,100,12,BACKGROUND);
        tft.setTextSize(1);
        tft.setCursor(5, 56);
        tft.print("Temp.Out: ");
        tft.setTextColor(getColorForTemperature(dsSaveOUT));
        tft.print(dsSaveOUT);
        tft.setTextColor(PINK);
        tft.print(" C");
        
        old_dsOUT = dsSaveOUT;
      getWeatherData();
      if(WifiClock){
        displayTimeWiFi();
      }else{
        timeString = String(hour) + ":" + String(minut);
        tft.fillRect(3, 70, 170, 30, BLACK);
        tft.setCursor(30, 70);
        tft.setTextSize(4);
        tft.setTextColor(0x07DF);
        tft.print(timeString);
      }


      }else if(page == 1){
        if(!setClock){
          if(line_page_1 == 0){//Положення номер 1 к 2
            line_page_1 = 1;
            tft.fillRect(1, 15, 15, 105, BLACK);
            tft.setCursor(7, 35);
            tft.setTextColor(WHITE);
            tft.print(point);
          }else if(line_page_1 == 1){//Положення номер 2 к 3
            line_page_1 = 2;
            tft.fillRect(1, 15, 15, 105, BLACK);
            tft.setCursor(7, 50);
            tft.setTextColor(WHITE);
            tft.print(point);
          }else if(line_page_1 == 2){//Положення номер 3 к 4
            line_page_1 = 3;
            tft.fillRect(1, 15, 15, 105, BLACK);
            tft.setCursor(7, 65);
            tft.setTextColor(WHITE);
            tft.print(point);
          }else if(line_page_1 == 3){//Положення номер 4 к 1
            line_page_1 = 0;
            tft.fillRect(1, 15, 15, 105, BLACK);
            tft.setCursor(7, 20);
            tft.setTextColor(WHITE);
            tft.print(point);
          }
          Serial.print("Page 1 line:");
          Serial.println(line_page_1);
      
        }else{
          if (line_page_1 == 2){//------------------------------------------------------------ГОДИННИК---------------------------------------------------------------------
            if(str_time_page_1 == 1){
              if(hour == 23){
                hour = 0;

                tft.fillRect(23, 47, 35, 13,BACKGROUND);
                tft.setCursor(25, 50);
                tft.setTextColor(BLUE);
                tft.print(hour);
                tft.setTextColor(YELLOW);
                tft.print(":");
                tft.print(minut);

                Serial.print("Hour: ");
                Serial.println(hour);  
                
              }else{
                hour++;

                tft.fillRect(23, 47, 35, 13,BACKGROUND);
                tft.setCursor(25, 50);
                tft.setTextColor(BLUE);
                tft.print(hour);
                tft.setTextColor(YELLOW);
                tft.print(":");
                tft.print(minut);

                Serial.print("Hour: ");
                Serial.println(hour);  
              }        
            }else if(str_time_page_1 == 2){
              if(minut == 59){
                minut = 0;

                tft.fillRect(23, 47, 35, 13,BACKGROUND);
                tft.setCursor(25, 50);
                tft.setTextColor(YELLOW);
                tft.print(hour);
                tft.print(":");
                tft.setTextColor(BLUE);
                tft.print(minut);

                Serial.print("Minut: ");
                Serial.println(minut);  
              }else{
                minut++;

                tft.fillRect(23, 47, 35, 13,BACKGROUND);
                tft.setCursor(25, 50);
                tft.setTextColor(YELLOW);
                tft.print(hour);
                tft.print(":");
                tft.setTextColor(BLUE);
                tft.print(minut);

                Serial.print("Minut: ");
                Serial.println(minut);  
              }        
            }
          }else if(line_page_1 == 3){//-------------------------------------------------------------БУДИЛЬНИК-------------------------------------------------------------------
            
            if(str_time_page_1 == 1){
              if(AlarmHour == 23){
                AlarmHour = 0;

                tft.fillRect(23, 63, 35, 13,BACKGROUND);
                tft.setCursor(25, 65);
                tft.setTextColor(BLUE);
                tft.print(AlarmHour);
                tft.setTextColor(YELLOW);
                tft.print(":");
                tft.print(AlarmMinut);

                Serial.print("Hour: ");
                Serial.println(AlarmHour);  
                
               File file = SD.open("/clock_setting.json", FILE_WRITE);
               if (file) {
                 // Записуємо стандартні налаштування у файл
                 StaticJsonDocument<256> doc;
                 doc["alarm_clock"] = alarm_clock;
                 doc["wificlock"] = WifiClock;
         
                 doc["AlarmHour"] = AlarmHour;
                 doc["AlarmMinut"] = AlarmMinut;
                 // Серіалізуємо дані у JSON-формат і записуємо у файл
                 if (serializeJson(doc, file) == 0) {
                   Serial.println(F("Помилка запису JSON у файл!"));
                 }
                 file.close();
               }



              }else{
                AlarmHour++;

                tft.fillRect(23, 63, 35, 13,BACKGROUND);
                tft.setCursor(25, 65);
                tft.setTextColor(BLUE);
                tft.print(AlarmHour);
                tft.setTextColor(YELLOW);
                tft.print(":");
                tft.print(AlarmMinut);

                Serial.print("Hour: ");
                Serial.println(AlarmHour);  
                
               File file = SD.open("/clock_setting.json", FILE_WRITE);
               if (file) {
                 // Записуємо стандартні налаштування у файл
                 StaticJsonDocument<256> doc;
                 doc["alarm_clock"] = alarm_clock;
                 doc["wificlock"] = WifiClock;
         
                 doc["AlarmHour"] = AlarmHour;
                 doc["AlarmMinut"] = AlarmMinut;
                 // Серіалізуємо дані у JSON-формат і записуємо у файл
                 if (serializeJson(doc, file) == 0) {
                   Serial.println(F("Помилка запису JSON у файл!"));
                 }
                 file.close();
               }


             


              }        
            }else if(str_time_page_1 == 2){
              if(AlarmMinut == 59){
                AlarmMinut = 0;

                tft.fillRect(23, 63, 35, 13,BACKGROUND);
                tft.setCursor(25, 65);
                tft.setTextColor(YELLOW);
                tft.print(AlarmHour);
                tft.print(":");
                tft.setTextColor(BLUE);
                tft.print(AlarmMinut);

                Serial.print("Minut: ");
                
               File file = SD.open("/clock_setting.json", FILE_WRITE);
               if (file) {
                 // Записуємо стандартні налаштування у файл
                 StaticJsonDocument<256> doc;
                 doc["alarm_clock"] = alarm_clock;
                 doc["wificlock"] = WifiClock;
         
                 doc["AlarmHour"] = AlarmHour;
                 doc["AlarmMinut"] = AlarmMinut;
                 // Серіалізуємо дані у JSON-формат і записуємо у файл
                 if (serializeJson(doc, file) == 0) {
                   Serial.println(F("Помилка запису JSON у файл!"));
                 }
                 file.close();
               }
               


                
              }else{
                AlarmMinut++;

                tft.fillRect(23, 63, 35, 13,BACKGROUND);
                tft.setCursor(25, 65);
                tft.setTextColor(YELLOW);
                tft.print(AlarmHour);
                tft.print(":");
                tft.setTextColor(BLUE);
                tft.print(AlarmMinut);

                Serial.print("Minut: ");
                
               File file = SD.open("/clock_setting.json", FILE_WRITE);
               if (file) {
                 // Записуємо стандартні налаштування у файл
                 StaticJsonDocument<256> doc;
                 doc["alarm_clock"] = alarm_clock;
                 doc["wificlock"] = WifiClock;
         
                 doc["AlarmHour"] = AlarmHour;
                 doc["AlarmMinut"] = AlarmMinut;
                 // Серіалізуємо дані у JSON-формат і записуємо у файл
                 if (serializeJson(doc, file) == 0) {
                   Serial.println(F("Помилка запису JSON у файл!"));
                 }
                 file.close();
               }
               
                
                

                
              }        
            }

          }
        }

        
      }
    

  }
  if(MINUSbtn.released()){// -----------------------------------------------------------------MINUSbtn------------------------------------------------------------------------------
    tone(buzzer,1500,80);

    if(page == 0){
      if(WiFi.status() != WL_CONNECTED){
        tone(buzzer,1000,250);
        tft.fillRect(0, 20, 128, 15, BLACK);
        tft.setCursor(0, 20);
        tft.setTextColor(PINK);
        tft.setTextSize(2);
        tft.print("Reconnect");
        readStoredCredentials();
      }
    }else if(page == 1 ){

      if(!setClock){//якщо не виставляється час 

        if(line_page_1 == 3){
            line_page_1 = 2;
            tft.fillRect(1, 15, 15, 105, BLACK);
            tft.setCursor(7, 50);
            tft.setTextColor(WHITE);
            tft.print(point);

          }else if(line_page_1 == 2){
            line_page_1 = 1;
            tft.fillRect(1, 15, 15, 105, BLACK);
            tft.setCursor(7, 35);
            tft.setTextColor(WHITE);
            tft.print(point);

          }else if(line_page_1 == 1){
            line_page_1 = 0;
            tft.fillRect(1, 15, 15, 105, BLACK);
            tft.setCursor(7, 20);
            tft.setTextColor(WHITE);
            tft.print(point);

          }else if(line_page_1 == 0){
            line_page_1 = 3;
            tft.fillRect(1, 15, 15, 105, BLACK);
            tft.setCursor(7, 65);
            tft.setTextColor(WHITE);
            tft.print(point);

          }
          Serial.print("Page 1 line:");
          Serial.println(line_page_1);
        }else{
          if(line_page_1 == 2){//------------------------------------------------------------ГОДИННИК---------------------------------------------------------------------
            if(str_time_page_1 == 1){
              if(hour == 0){
                hour = 23;

                tft.fillRect(23, 47, 35, 13,BACKGROUND);
                tft.setCursor(25, 50);
                tft.setTextColor(BLUE);
                tft.print(hour);
                tft.setTextColor(YELLOW);
                tft.print(":");
                tft.print(minut);

                Serial.print("Hour: ");
                Serial.println(hour);  
              }else{
                hour = hour - 1;

                tft.fillRect(23, 47, 35, 13,BACKGROUND);
                tft.setCursor(25, 50);
                tft.setTextColor(BLUE);
                tft.print(hour);
                tft.setTextColor(YELLOW);
                tft.print(":");
                tft.print(minut);

                Serial.print("Hour: ");
                Serial.println(hour);  
              }        
            }else if(str_time_page_1 == 2){
              if(minut == 0){
                minut = 59;

                tft.fillRect(23, 47, 35, 13,BACKGROUND);
                tft.setCursor(25, 50);
                tft.setTextColor(YELLOW);
                tft.print(hour);
                tft.print(":");
                tft.setTextColor(BLUE);
                tft.print(minut);

                Serial.print("Minut: ");
                Serial.println(minut);  
              }else{
                minut--;

                tft.fillRect(23, 47, 35, 13,BACKGROUND);
                tft.setCursor(25, 50);
                tft.setTextColor(YELLOW);
                tft.print(hour);
                tft.print(":");
                tft.setTextColor(BLUE);
                tft.print(minut);

                Serial.print("Minut: ");
                Serial.println(minut);  
              }        
            }
          }else if(line_page_1 == 3){//-------------------------------------------------------------БУДИЛЬНИК-------------------------------------------------------------------

            if(str_time_page_1 == 1){
              if(AlarmHour == 0){
                AlarmHour = 23;

                tft.fillRect(23, 62, 35, 13,BACKGROUND);
                tft.setCursor(25, 65);
                tft.setTextColor(BLUE);
                tft.print(AlarmHour);
                tft.setTextColor(YELLOW);
                tft.print(":");
                tft.print(AlarmMinut);

                Serial.print("Hour: ");
                Serial.println(hour);  
                
               File file = SD.open("/clock_setting.json", FILE_WRITE);
               if (file) {
                 // Записуємо стандартні налаштування у файл
                 StaticJsonDocument<256> doc;
                 doc["alarm_clock"] = alarm_clock;
                 doc["wificlock"] = WifiClock;
         
                 doc["AlarmHour"] = AlarmHour;
                 doc["AlarmMinut"] = AlarmMinut;
                 // Серіалізуємо дані у JSON-формат і записуємо у файл
                 if (serializeJson(doc, file) == 0) {
                   Serial.println(F("Помилка запису JSON у файл!"));
                 }
                 file.close();
               }
                
                
           

              }else{
                AlarmHour = AlarmHour - 1;

                tft.fillRect(23, 62, 35, 13,BACKGROUND);
                tft.setCursor(25, 65);
                tft.setTextColor(BLUE);
                tft.print(AlarmHour);
                tft.setTextColor(YELLOW);
                tft.print(":");
                tft.print(AlarmMinut);

                Serial.print("Hour: ");
                Serial.println(AlarmHour);  
                
               File file = SD.open("/clock_setting.json", FILE_WRITE);
               if (file) {
                 // Записуємо стандартні налаштування у файл
                 StaticJsonDocument<256> doc;
                 doc["alarm_clock"] = alarm_clock;
                 doc["wificlock"] = WifiClock;
         
                 doc["AlarmHour"] = AlarmHour;
                 doc["AlarmMinut"] = AlarmMinut;
                 // Серіалізуємо дані у JSON-формат і записуємо у файл
                 if (serializeJson(doc, file) == 0) {
                   Serial.println(F("Помилка запису JSON у файл!"));
                 }
                 file.close();
               }
              



              }        
            }else if(str_time_page_1 == 2){
              if(AlarmMinut == 0){
                AlarmMinut = 59;

                tft.fillRect(23, 62, 35, 13,BACKGROUND);
                tft.setCursor(25, 65);
                tft.setTextColor(YELLOW);
                tft.print(AlarmHour);
                tft.print(":");
                tft.setTextColor(BLUE);
                tft.print(AlarmMinut);

                Serial.print("Minut: ");
                Serial.println(AlarmMinut);  
                
               File file = SD.open("/clock_setting.json", FILE_WRITE);
               if (file) {
                 // Записуємо стандартні налаштування у файл
                 StaticJsonDocument<256> doc;
                 doc["alarm_clock"] = alarm_clock;
                 doc["wificlock"] = WifiClock;
         
                 doc["AlarmHour"] = AlarmHour;
                 doc["AlarmMinut"] = AlarmMinut;
                 // Серіалізуємо дані у JSON-формат і записуємо у файл
                 if (serializeJson(doc, file) == 0) {
                   Serial.println(F("Помилка запису JSON у файл!"));
                 }
                 file.close();
               }


                
              }else{
                AlarmMinut--;
                tft.fillRect(23, 62, 35, 13,BACKGROUND);
                tft.setCursor(25, 65);
                tft.setTextColor(YELLOW);
                tft.print(AlarmHour);
                tft.print(":");
                tft.setTextColor(BLUE);
                tft.print(AlarmMinut);

                Serial.print("Minut: ");
                Serial.println(AlarmMinut);  
                
               File file = SD.open("/clock_setting.json", FILE_WRITE);
               if (file) {
                 // Записуємо стандартні налаштування у файл
                 StaticJsonDocument<256> doc;
                 doc["alarm_clock"] = alarm_clock;
                 doc["wificlock"] = WifiClock;
         
                 doc["AlarmHour"] = AlarmHour;
                 doc["AlarmMinut"] = AlarmMinut;
                 // Серіалізуємо дані у JSON-формат і записуємо у файл
                 if (serializeJson(doc, file) == 0) {
                   Serial.println(F("Помилка запису JSON у файл!"));
                 }
                 file.close();
               }
               
                
                
                
              }        
            }

          }
        }
      
    }else if(page == 2){

    }
  }

  if(PAGEbtn.released()){// -----------------------------------------------------------------PAGEbtn------------------------------------------------------------------------------
      tone(buzzer, 1500, 80);
      if(page == 0){
        tft.fillScreen(BACKGROUND);
        tft.setCursor(33, 5);
        tft.setTextColor(YELLOW);
        tft.setTextSize(1);
        tft.print("Setting clock");
        tft.fillRect(1, 15, 15, 105, BLACK);
        tft.setCursor(7, 20);
        tft.setTextColor(WHITE);
        tft.print(point);
        //WiFi Clock
        tft.setCursor(25, 20);
        tft.setTextSize(1);
        if(WifiClock){
        tft.setTextColor(GREEN);
        }else{
        tft.setTextColor(RED);
        }
        tft.print("Wifi Clock");

        //Alarm Clock
        tft.setCursor(25, 35);
        tft.setTextSize(1);
        if(alarm_clock){
        tft.setTextColor(GREEN);
        }else{
        tft.setTextColor(RED);
        }
        tft.print("Alarm Clock");
        //налаштування годинника
        tft.setCursor(80, 50);
        tft.setTextColor(ORANGE);
        tft.print("Clock");
        tft.setCursor(25, 65);
        tft.setCursor(25, 50);
        tft.setTextColor(YELLOW);
        tft.print(hour);
        tft.print(":");
        tft.print(minut);
        //налаштування будильника
        tft.setCursor(80, 65);
        tft.setTextColor(ORANGE);
        tft.print("Alarm Clock");
        tft.setCursor(25, 65);
        tft.setTextColor(YELLOW);
        tft.print(AlarmHour);
        tft.print(":");
        tft.print(AlarmMinut);
        


        str_time_page_1 = 0;
        line_page_1 = 0;
        page = 1;
        setClock = false;
      }else if(page == 1){//------------------------------------------------------WiFi INFO-----------------------------------------
        tft.fillScreen(BACKGROUND);
        page++;
        tft.setTextColor(YELLOW);
        tft.setCursor(40, 5);
        tft.print("WiFi Info");
        tft.setTextColor(GREEN);
        tft.setCursor(5, 20);
        tft.print("SSID: ");
        tft.setTextColor(PINK);
        tft.print(ssid_);
        tft.setTextColor(GREEN);
        tft.setCursor(5, 35);
        tft.print("PASS: ");
        tft.setTextColor(PINK);
        tft.print(password_);
        tft.setTextColor(GREEN);
        tft.setCursor(5, 50);
        tft.print("Local IP: ");
        tft.setTextColor(PINK);
        tft.print(WiFi.softAPIP());
        tft.setTextColor(GREEN);
        tft.setCursor(5, 65);
        tft.print("IP: ");
        tft.setTextColor(PINK);
        if(WiFi.status() == WL_CONNECTED){
          tft.print(WiFi.localIP().toString());
        }else{
        tft.setTextColor(RED);
        tft.print("UNCONNECT");
        }

      }else if(page == 2){
        tft.fillScreen(BACKGROUND);
        if (WiFi.status() != WL_CONNECTED) {
          tft.setCursor(25, 5);
          tft.setTextColor(RED);
          tft.setTextSize(1);
          tft.print(WiFi.softAPIP());
        }else{
          tft.fillRect(30, 5, 85, 15, BLACK);
          tft.setCursor(33,5);
          tft.setTextColor(GREEN);
          tft.setTextSize(1);
          tft.print(WiFi.localIP());

          getWeatherData();

          if(WifiClock){
            displayTimeWiFi();
            displayTimeWiFi();
            tft.setCursor(17, 70);
            tft.setTextSize(4);
            tft.setTextColor(0x07DF);
            tft.print(timeString);
          }else{
            timeString = String(hour) + ":" + String(minut);
            tft.fillRect(3, 70, 170, 30, BLACK);
            tft.setCursor(30, 70);
            tft.setTextSize(4);
            tft.setTextColor(0x07DF);
            tft.print(timeString);
          }
          getWeatherData(); // Виводимо погоду
          getWeatherData(); // Виводимо погоду

          if(old_dsIN != dsSaveIN){
            //Serial.print("In: ");
            //Serial.println(dsSaveIN);
            tft.setTextColor(PINK);
            tft.fillRect(4,44,100,12,BACKGROUND);
            tft.setTextSize(1);
            tft.setCursor(5, 45);
            tft.print("Temp.In: ");
            tft.setTextColor(getColorForTemperature(dsSaveIN));
            tft.print(dsSaveIN);
            tft.setTextColor(PINK);
            tft.print(" C");
            old_dsIN = dsSaveIN;
          }
          if(old_dsOUT != dsSaveOUT){
            Serial.print("Out: ");
            Serial.println(dsSaveOUT);
            tft.setTextColor(PINK);
            tft.fillRect(4,55,100,12,BACKGROUND);
            tft.setTextSize(1);
            tft.setCursor(5, 56);
            tft.print("Temp.Out: ");
            tft.setTextColor(getColorForTemperature(dsSaveOUT));
            tft.print(dsSaveOUT);
            tft.setTextColor(PINK);
            tft.print(" C");

            old_dsOUT = dsSaveOUT;
          }
        }
        page = 0;
      }
      Serial.print("Menu Page: ");
      Serial.println((int)page);
  }
 

}






void displayTimeWiFi() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi Disconnected");
    return;
  }

  HTTPClient http;
  String url = "http://worldtimeapi.org/api/timezone/Europe/Berlin";
  http.begin(url);

  int httpResponseCode = http.GET();
  if (httpResponseCode > 0) {
    String payload = http.getString();
    Serial.println(payload);

    DynamicJsonDocument doc(2048);
    DeserializationError error = deserializeJson(doc, payload);

    if (!error) {
      const char* datetime = doc["datetime"];
      //timeString = String(datetime).substring(11, 16);
      //Serial.print("Time: ");
      //Serial.println(timeString);
      // Витягуємо години та хвилини безпосередньо з рядка datetime
      String hourString = String(datetime).substring(11, 13);  // Витягуємо години (символи з позицій 11-12)
      String minutString = String(datetime).substring(14, 16); // Витягуємо хвилини (символи з позицій 14-15)
      String secString = String(datetime).substring(17, 19);  

      hour = hourString.toInt();   // Перетворюємо рядок з годинами в число
      minut = minutString.toInt(); // Перетворюємо рядок з хвилинами в число
      sec = secString.toInt();      // Перетворюємо рядок з секундами в число

      timeString = String(hour) + ":" + String(minut);

      tft.fillRect(3, 70, 170, 30, BLACK);
      tft.setCursor(17, 70);
      tft.setTextSize(4);
      tft.setTextColor(0x07DF);
      tft.print(timeString);
    } else {
      Serial.print("Помилка парсингу JSON: ");
      Serial.println(error.c_str());
    }
  } else {
    Serial.println("Error on HTTP request.TIME");
  }
  http.end();
}

void getWeatherData() {
  if(page == 0){
    if (WiFi.status() == WL_CONNECTED) {
      HTTPClient http;
      String url = "http://api.openweathermap.org/data/2.5/weather?q=" + String(city) + "&appid=" + String(apiKey) + "&units=" + String(units);
      http.begin(url);

      int httpResponseCode = http.GET();
      if (httpResponseCode > 0) {
        String payload = http.getString();
        Serial.println(payload);

        DynamicJsonDocument doc(1024);
        DeserializationError error = deserializeJson(doc, payload);

        if (!error) {
          float temp = doc["main"]["temp"];
          const char* weather_ = doc["weather"][0]["main"];

          tft.fillRect(1, 18, 157, 25, BACKGROUND);
          tft.setCursor(5, 20);
          tft.setTextSize(1);
          tft.setTextColor(YELLOW);
          tft.print("Temperature:");
          tft.setTextColor(getColorForTemperature(temp));
          tft.print(temp);
          tft.setTextColor(YELLOW);
          tft.println(" C");

          tft.setTextColor(YELLOW);
          tft.setCursor(5, 29);
          tft.print("Weather:");
          tft.setTextColor(getColorForWeather(weather_));
          tft.println(weather_);
        } else {
          Serial.print("Помилка парсингу JSON: ");
          Serial.println(error.c_str());
        }
      } else {
        Serial.println("Error on HTTP request WEATHER");
      }
      http.end();
    } else {
      Serial.println("WiFi Disconnected");
      tft.fillRect(0, 20, 128, 128, BLACK);
      tft.setCursor(0, 20);
      tft.setTextColor(RED);
      tft.setTextSize(2);
      tft.print("Unconnect");
    }
  }
}

uint16_t getColorForTemperature(float temp) {
  if (temp <= 0) return BLUE;
  if (temp > 0 && temp <= 15) return tft.color565(0, 0, 255);
  if (temp > 15 && temp <= 25) return tft.color565(0, 255, 0);
  if (temp > 25 && temp <= 35) return tft.color565(255, 255, 0);
  return tft.color565(255, 165, 0);
}


uint16_t getColorForWeather(const char* weather_) {
  if (strcmp(weather_, "clear sky") == 0) return CYAN;
  if (strcmp(weather_, "few clouds") == 0) return 0xF7D6;
  if (strcmp(weather_, "Clouds") == 0 || strcmp(weather_, "drizzle") == 0) return 0x96D6;
  if (strcmp(weather_, "rain") == 0) return 0x528A;
  if (strcmp(weather_, "shower rain") == 0) return LIGHTGREY;
  if (strcmp(weather_, "thunderstorm") == 0) return MAGENTA;
  if (strcmp(weather_, "snow") == 0) return WHITE;
  if (strcmp(weather_, "mist") == 0) return DARKGREY;
  return GREEN;
}



void handleRoot() {
  String html = "<!DOCTYPE html>"
   "<html>"
   "<head>"
   "  <meta charset='utf-8'>"
   "  <meta name='viewport' content='width=device-width, initial-scale=1'>"
   "  <title>More</title>"
   "  <style>"
   "    body {"
   "      font-family: 'Arial', sans-serif;"
   "      background-color: #f2f2f2;"
   "      text-align: center;"
   "      background-color:grey;"
   "    }"
   "    .container {"
   "      max-width: 400px;"
   "      margin: 0 auto;"
   "      padding: 20px;"
   "      background-color: #9c9a9a;"
   "      border-radius: 5px;"
   "      box-shadow: 0 0 10px rgba(0, 0, 0, 0.1);"
   "    }"
   "    h1 {"
   "      color: #333;"
   "    }"
   "    label {"
   "      display: block;"
   "      margin-bottom: 10px;"
   "      font-weight: bold;"
   "    }"
   "    .input-container {"
   "      display: flex;"
   "      flex-direction: column;"
   "      align-items: center;"
   "      margin-bottom: 20px;" // Відступ між контейнером інпутів і іншими елементами
   "    }"
   "    input[type='text'], input[type='password'] {"
   "      width: 100%;"
   "      padding: 10px;"
   "      margin-bottom: 10px;" // Відступ між інпутами
   "      border: 1px solid #ccc;"
   "      border-radius: 3px;"
   "    }"
   "    .connect-button {"
   "      background-color: #007BFF;"
   "      color: #fff;"
   "      border: none;"
   "      padding: 10px 20px;"
   "      border-radius: 3px;"
   "      cursor: pointer;"
   "    }"
   "    .connect-button:hover {"
   "      background-color: #0056b3;"
   "    }"
   "    .back-button {"
   "      background-color: #ccc;"
   "      color: #333;"
   "      border: none;"
   "      padding: 10px 20px;"
   "      border-radius: 3px;"
   "      cursor: pointer;"
   "      margin-top: 10px;" // Відступ між кнопкою і іншими елементами
   "    }"
   "    .back-button:hover {"
   "      background-color: #999;"
   "    }"
   "  </style>"
   "</head>"
   "<body>";

  if (WiFi.status() != WL_CONNECTED) {
    html += "<div class='container' id='setting'>";
    html += "  <h1>Wi-Fi налаштування</h1>";
    html += "  <div class='input-container'>"; // Один контейнер для обох інпутів
    html += "    <label for='ssid'>Назва Wi-Fi:</label>";
    html += "    <input type='text' id='ssid' name='ssid'>";
    html += "    <label for='password'>Пароль Wi-Fi:</label>";
    html += "    <input type='password' id='password' name='password'>";
    html += "  </div>";
    html += "  <div>";
    html += "    <button class='connect-button' onclick='connectWiFi();'>";
    html += "      <b>З'єднатись</b>";
    html += "    </button>";
    html += "  </div>";
    html += "</div>";
  } else {
    html += "<h1 class='white-text' style='text-align:center;'>" + WiFi.localIP().toString() + "</h1>";
  }



  html += "<script>"
   "  function connectWiFi() {"
   "    var ssid = document.getElementById('ssid').value;"
   "    var password = document.getElementById('password').value;"
   "    var xhttp = new XMLHttpRequest();"
   "    xhttp.open('POST', '/connect', true);"
   "    xhttp.setRequestHeader('Content-type', 'application/x-www-form-urlencoded');"
   "    xhttp.send('ssid=' + ssid + '&password=' + password);"
   "    alert('Перезавантажте сторінку');"
   "  }"
   "  function back() {"
   "    window.location.href = '/';"
   "  }"
   "</script>"
   "</body>"
   "</html>";

  webServer.send(200, "text/html", html);
}

void handleConnect() {
  String ssidInput = webServer.arg("ssid");
  String passwordInput = webServer.arg("password");

  Serial.println("Дані Wi-Fi:");
  Serial.println("SSID: " + ssidInput);
  Serial.println("Пароль: " + passwordInput);

  WiFi.begin(ssidInput.c_str(), passwordInput.c_str());

  if (WiFi.waitForConnectResult() == WL_CONNECTED) {
    //display
    tft.fillRect(30, 5, 85, 15, BLACK);
    tft.setCursor(33,5);
    tft.setTextColor(GREEN);
    tft.setTextSize(1);
    tft.print(WiFi.localIP());  

    File file = SD.open("/wifi.json", FILE_WRITE);
    if (file) {
      DynamicJsonDocument doc(1024);
      doc["ssid"] = ssidInput;
      doc["password"] = passwordInput;

      if (serializeJson(doc, file)) {
        Serial.println("Wi-Fi дані збережено на SD-карті.");
      } else {
        Serial.println("Помилка запису JSON.");
      }

      file.close();
    } else {
      Serial.println("Не вдалося відкрити файл для запису.");
    }
      Serial.println("Підключено до Wi-Fi!");
      Serial.print("IP адреса: ");
      Serial.println(WiFi.localIP());
    } else {
      Serial.println("Не вдалося підключитися до Wi-Fi.");
    }
}

void readStoredCredentials() {
  File file = SD.open("/wifi.json");
  if (file) {
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, file);

    if (!error) {
      ssid_ = doc["ssid"].as<String>();
      password_ = doc["password"].as<String>();

      Serial.println("Завантажено дані з SD:");
      Serial.println("SSID: " + ssid_);
      Serial.println("Password: " + password_);

      WiFi.begin(ssid_.c_str(), password_.c_str());

      if (WiFi.waitForConnectResult() == WL_CONNECTED) {
        Serial.println("Підключено до Wi-Fi.");
        //display
        tft.fillRect(30, 5, 85, 15, BLACK);
        tft.setCursor(33,5);
        tft.setTextColor(GREEN);
        tft.setTextSize(1);
        tft.print(WiFi.localIP());  
        WiFi.softAPdisconnect(true); 
        WiFi.softAP(ssid + " " + WiFi.localIP().toString(), password);
      } else {
        Serial.println("Не вдалося підключитися до Wi-Fi.");
        tft.fillRect(0, 20, 128, 15, BLACK);
        tft.setCursor(0, 20);
        tft.setTextColor(RED);
        tft.setTextSize(2);
        tft.print("Unconnect");
      }
    } else {
      Serial.println("Помилка читання JSON з SD.");
    }

    file.close();
  } else {
    Serial.println("Файл Wi-Fi даних не знайдено.");
  }
}