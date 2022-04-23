#include <Wire.h> //permite comuincarse por los pines I2C
#include <Adafruit_INA219.h> //librería para el uso del sensor

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h> //ambas para el uso de la pantalla

#include <SPI.h> //permite comunicarse por los pines SPI
#include <SD.h> //ya incluida en arduino, permite usar la tarjeta microSD

#include <NTPClient.h> //importamos la librería del cliente NTP
#include <ESP8266WiFi.h> //librería necesaria para la conexión wifi
#include <WiFiUdp.h> // importamos librería UDP para comunicar con NTP
#include <Time.h>
#include <TimeLib.h>
#include <Timezone.h>

// Definir constantes
#define ANCHO_PANTALLA 128 // ancho pantalla OLED
#define ALTO_PANTALLA 64 // alto pantalla OLED

#define CSpin D8    // Slave Select en pin digital 8 para tarjeta microSD

// NTP propiedades
#define NTP_OFFSET   3600      // Zona horaria, en España +1, es decir, 1h en segundos
#define NTP_INTERVALO 2 * 1000    // Tiempo en milisegundos que se vuelve a solicitar la información
#define NTP_ADDRESS  "hora.roa.es"  // pagina ntp donde obtener los datos

Adafruit_INA219 ina219;         //Por defecto dirección I2C 0x40
Adafruit_INA219 ina219_b(0x41); //Puente en R7
Adafruit_INA219 ina219_c(0x44); //Puente en R6
Adafruit_INA219 ina219_d(0x45); //Puente en R6 y R7

// Objeto de la clase Adafruit_SSD1306 con el ancho, el alto, el puntero y el reset
Adafruit_SSD1306 display(ANCHO_PANTALLA, ALTO_PANTALLA, &Wire, -1);

File memoria;      // objeto donde se realiza el almacenamiento de datos del tipo File

//iniciamos el cliente udp para su uso con el servidor NTP
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, NTP_ADDRESS, NTP_OFFSET, NTP_INTERVALO);

// datos para conectarnos a nuestra red wifi
const char *ssid     = "AndroidAP";
const char *password = "cdwu2208";
String date;
String t;

// Convertir las unidades de tiempo UTC en local
TimeChangeRule CEST = {"CEST", Last, Sun, Mar, 2, 60};  //Horario de verano en europa central, "Eurepan Central Summer Time"
TimeChangeRule CET = {"CET", Last, Sun, Oct, 3, 0};   //Horario de invierno
Timezone CE(CEST, CET);

void setup(void)
{
  Serial.begin(115200);
  uint32_t currentFrequency;
  delay(200);

  // Iniciar el INA219
  ina219.begin();  //por defecto, inicia a 32V y 2A
  ina219_b.begin();
  ina219_c.begin();
  ina219_d.begin();
  // Opcionalmente, cambiar la sensibilidad del sensor
  //ina219.setCalibration_32V_1A();
  ina219.setCalibration_16V_400mA();
  Serial.println("Inicidando Sensores de Corriente ");
  delay(200);

  Serial.println("Iniciando pantalla OLED");
  // Iniciar pantalla OLED en la dirección 0x3C, SSD1306_SWITCHCAPVCC indica voltaje 3,3V interno de pantalla
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C))
  {
    Serial.println("No se encuentra la pantalla OLED");
    while (true);
  }

  // Limpiar buffer pantalla
  display.clearDisplay();
  // Tamaño del texto
  display.setTextSize(1);
  // Color del texto
  display.setTextColor(SSD1306_WHITE);
  // Posición del texto
  delay(200);

  Serial.println("Inicializando tarjeta ...");  // texto en ventana de monitor
  if (!SD.begin(CSpin)) {     // inicializacion de tarjeta SD
    Serial.println("fallo en inicializacion !");// si falla se muestra texto correspondiente y
    return;        // impide que el programa avance
  }
  Serial.println("inicializacion correcta");  // texto de inicializacion correcta
  delay(200);

  WiFi.begin(ssid, password); // nos conectamos al wifi
  // Esperamos hasta que se establezca la conexión wifi
  while ( WiFi.status() != WL_CONNECTED ) {
    Serial.print ( "." );
    delay ( 500 );
  }

  timeClient.begin();

  delay(200);
}

void loop(void)
{
  float corriente_mA = 0;
  float corriente_b_mA = 0;
  float corriente_c_mA = 0;
  float corriente_d_mA = 0;

  // Obtener mediciones
  corriente_mA = ina219.getCurrent_mA();
  corriente_b_mA = ina219_b.getCurrent_mA();
  corriente_c_mA = ina219_c.getCurrent_mA();
  corriente_d_mA = ina219_d.getCurrent_mA();

  // Limpiar buffer
  display.clearDisplay();
  //Sensor 1
  display.setCursor(0, 0); display.println("SENSOR 1");
  display.setCursor(0, 16); display.print(corriente_mA); display.println(" mA");
  //Sensor 2
  display.setCursor(64, 0); display.println("SENSOR 2");
  display.setCursor(64, 16); display.print(corriente_b_mA); display.println(" mA");
  //Sensor 3
  display.setCursor(0, 32); display.println("SENSOR 3");
  display.setCursor(0, 48); display.print(corriente_c_mA); display.println(" mA");
  //Sensor 4
  display.setCursor(64, 32); display.println("SENSOR 4");
  display.setCursor(64, 48); display.print(corriente_d_mA); display.println(" mA");
  // Enviar a pantalla
  display.display();

  memoria = SD.open("datos.txt", FILE_WRITE); // apertura para lectura/escritura de archivo datos.txt
  if (memoria) {
    Serial.println("Guardando datos en memoria microSD ");
    if (WiFi.status() == WL_CONNECTED) //Check WiFi connection status
    {
      timeClient.update(); //sincronizamos con el server NTP
      unsigned long epochTime =  timeClient.getEpochTime();

      // convert received time stamp to time_t object
      time_t local, utc;
      utc = epochTime;
      local = CE.toLocal(utc);

      Obtener_Fecha(local);
      Obtener_Hora(local);

      // Mostrar en pantalla fecha y hora
      Serial.print(date);
      Serial.print(" ");
      Serial.print(t);
      // Mostrar en tarjeta de almacenamiento fecha y hora
      memoria.print(date);
      memoria.print(" ");
      memoria.print(t);
    }
    else
    {
      Serial.print("FalloConexiónWifi ");
      memoria.print("FalloConexiónWifi ");
    }

    memoria.print("   SENSOR 1: "); memoria.print(corriente_mA);   memoria.print(" mA   ");
    memoria.print("SENSOR 2: "); memoria.print(corriente_b_mA); memoria.print(" mA   ");
    memoria.print("SENSOR 3: "); memoria.print(corriente_c_mA); memoria.print(" mA   ");
    memoria.print("SENSOR 4: "); memoria.print(corriente_d_mA); memoria.println(" mA   ");

    // Mostrar mediciones
    Serial.print("Sensor 1:  "); Serial.print(corriente_mA);   Serial.print(" mA   ");
    Serial.print("Sensor 2:  "); Serial.print(corriente_b_mA); Serial.print(" mA   ");
    Serial.print("Sensor 3:  "); Serial.print(corriente_c_mA); Serial.print(" mA   ");
    Serial.print("Sensor 4:  "); Serial.print(corriente_d_mA); Serial.println(" mA");

    memoria.close(); //cierra el archivo y garantiza que se escriban los datos correctamente
  }
  else
  {
    Serial.println("error en apertura de datos.txt");  // texto de falla en apertura de archivo
  }

  delay(2000);
}

String Obtener_Fecha(time_t local)
{
  // now format the Time variables into strings with proper names for month, day etc
  date = "";
  date += day(local);
  date += "/";
  date += month(local);
  date += "/";
  date += year(local);
  return date;
}

String Obtener_Hora(time_t local)
{
  t = "";
  // format the time
  if (hour(local) < 10)
    t += "0";
  t += hour(local);
  t += ":";
  if (minute(local) < 10) // add a zero if minute is under 10
    t += "0";
  t += minute(local);
  t += ":";
  if (second(local) < 10)
    t += "0";
  t += second(local);
  return t;
}
