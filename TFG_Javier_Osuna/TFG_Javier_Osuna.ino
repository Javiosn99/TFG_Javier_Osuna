#include <Wire.h> //permite comuincarse por los pines I2C
#include <Adafruit_INA219.h> //librería para el uso del sensor

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h> //ambas para el uso de la pantalla
#include <Button2.h>

#include <SD.h> //ya incluida en arduino, permite usar la tarjeta microSD
#include <SPI.h> //permite comunicarse por los pines SPI

#include <NTPClient.h> //importamos la librería del cliente NTP
#include <ESP8266WiFi.h> //librería necesaria para la conexión wifi
#include <WiFiUdp.h> // importamos librería UDP para comunicar con NTP
#include <Time.h> //Se utiliza para sincronizar la hora con NTP
#include <TimeLib.h> //Se incluye con la librería anterior
#include <Timezone.h> //Permite pasar UTC a tiempo local, y usar el cambio de verano e invierno

#include <PubSubClient.h>
#include <ArduinoJson.h>

#include "ESP8266FtpServer.h"
//#include "ESP8266_Utils.hpp"

// Definir constantes
#define ANCHO_PANTALLA 128 // ancho pantalla OLED
#define ALTO_PANTALLA 64 // alto pantalla OLED

#define BUTTON_PIN  0
#define CSpin D8    // Slave Select en pin digital 8 para tarjeta microSD

// NTP propiedades
#define NTP_OFFSET   3600      // Zona horaria, en España +1, es decir, 1h en segundos
#define NTP_INTERVALO 2 * 1000    // Tiempo en milisegundos que se vuelve a solicitar la información
#define NTP_ADDRESS  "hora.roa.es"  // pagina ntp donde obtener los datos

// Define MQTT iot.ac.uma.es
#define mqtt_server "iot.ac.uma.es"
#define mqtt_user "infind"
#define mqtt_password "zancudo"

// Topics a emplear
#define consumo_topic "TFG/Consumos"
#define envio_datos_topic "TFG/Envio_datos"
#define envio_tiempo_topic "TFG/Envio_tiempo"
#define almacenamiento_datos_topic "TFG/Almacenamiento_datos"
#define almacenamiento_tiempo_topic "TFG/Almacenamiento_tiempo"

unsigned long TiempoPantalla = 0;
unsigned long PeriodoPantalla = 3000;

unsigned long TiempoAlmacenamiento = 0;
unsigned long PeriodoAlmacenamiento = 2000;

unsigned long TiempoEnvio = 0;
unsigned long PeriodoEnvio = 500;

unsigned cont = 1;
unsigned modo = 1;

struct conjunto_corrientes
{
  float corriente_mA;
  float corriente_b_mA;
  float corriente_c_mA;
  float corriente_d_mA;
};

struct conjunto_voltajes
{
  float voltaje_V;
  float voltaje_b_V;
  float voltaje_c_V;
  float voltaje_d_V;
};

struct conjunto_potencias
{
  float potencia_mW;
  float potencia_b_mW;
  float potencia_c_mW;
  float potencia_d_mW;
};

// datos para conectarnos a nuestra red wifi
const char *ssid     = "AndroidAP";
const char *password = "cdwu2208";
String date;
String t;

// Se declaran las variables donde se almacenan los payload recibidos por mqtt
String payload_envio;
String payload_almacenamiento;
String topic_recibido;

Adafruit_INA219 ina219;
Adafruit_INA219 ina219_b(0x41);
Adafruit_INA219 ina219_c(0x44);
Adafruit_INA219 ina219_d(0x45);

// Objeto de la clase Adafruit_SSD1306 con el ancho, el alto, el puntero y el reset
Adafruit_SSD1306 display(ANCHO_PANTALLA, ALTO_PANTALLA, &Wire, -1);

// Declaración del boton "button2.h"
Button2 button = Button2(BUTTON_PIN);

File memoria;      // objeto donde se realiza el almacenamiento de datos del tipo File

//iniciamos el cliente udp para su uso con el servidor NTP
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, NTP_ADDRESS, NTP_OFFSET, NTP_INTERVALO);

// Convertir las unidades de tiempo UTC en local
TimeChangeRule CEST = {"CEST", Last, Sun, Mar, 2, 60};  //Horario de verano en europa central, "Eurepan Central Summer Time"
TimeChangeRule CET = {"CET", Last, Sun, Oct, 3, 0};   //Horario de invierno
Timezone CE(CEST, CET);

// ESP8266 WiFi
WiFiClient espClient;
PubSubClient client(espClient);

FtpServer ftpSrv; //Instancia de la librería para tener acceso a sus funciones

void setup(void)
{
  Serial.begin(115200);

  ina219.begin();
  ina219_b.begin();
  ina219_c.begin();
  ina219_d.begin();

  ina219.setCalibration_16V_400mA();
  ina219_b.setCalibration_16V_400mA();
  ina219_c.setCalibration_16V_400mA();
  ina219_d.setCalibration_16V_400mA();
  Serial.println("Iniciando Sensores de Corriente ");
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

  button.setClickHandler(handler);
  button.setLongClickHandler(handler);
  button.setDoubleClickHandler(handler);

  Serial.println("Inicializando tarjeta ...");
  if (!SD.begin(CSpin))
  {
    Serial.println("Fallo en inicializacion !");
    return;        // impide que el programa avance
  }
  else {
    Serial.println("Inicializacion correcta");

    ftpSrv.begin("admin", "admin");   //nombre de usuario, contraseña para ftp. establecer puerto en ESP8266FtpServer.h (predeterminado 21, 50009 para PASV)
    delay(200);
  }

  WiFi.begin(ssid, password); // nos conectamos al wifi
  // Esperamos hasta que se establezca la conexión wifi
  while ( WiFi.status() != WL_CONNECTED )
  {
    Serial.print ( "." );
    delay ( 500 );
  }

  timeClient.begin();

  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

  delay(200);
}

void loop(void)
{
  struct conjunto_corrientes medidas_I;
  struct conjunto_voltajes medidas_V;
  struct conjunto_potencias medidas_P;

  // Obtener mediciones
  medidas_I.corriente_mA = ina219.getCurrent_mA();
  medidas_I.corriente_b_mA = ina219_b.getCurrent_mA();
  medidas_I.corriente_c_mA = ina219_c.getCurrent_mA();
  medidas_I.corriente_d_mA = ina219_d.getCurrent_mA();

  medidas_V.voltaje_V = ina219.getBusVoltage_V();
  medidas_V.voltaje_b_V = ina219_b.getBusVoltage_V();
  medidas_V.voltaje_c_V = ina219_c.getBusVoltage_V();
  medidas_V.voltaje_d_V = ina219_d.getBusVoltage_V();

  medidas_P.potencia_mW = ina219.getPower_mW();
  medidas_P.potencia_b_mW = ina219_b.getPower_mW();
  medidas_P.potencia_c_mW = ina219_c.getPower_mW();
  medidas_P.potencia_d_mW = ina219_d.getPower_mW();

  /*Serial.print("Sensor 1: "); Serial.print( medidas_I.corriente_mA); Serial.print("mA "); Serial.print(medidas_V.voltaje_V); Serial.print("V ");
    Serial.print(medidas_P.potencia_mW); Serial.println("mW ");
    Serial.print("Sensor 2: "); Serial.print( medidas_I.corriente_b_mA); Serial.print("mA "); Serial.print(medidas_V.voltaje_b_V); Serial.print("V ");
    Serial.print(medidas_P.potencia_b_mW); Serial.println("mW ");
    Serial.print("Sensor 3: "); Serial.print( medidas_I.corriente_c_mA); Serial.print("mA "); Serial.print(medidas_V.voltaje_c_V); Serial.print("V ");
    Serial.print(medidas_P.potencia_c_mW); Serial.println("mW ");
    Serial.print("Sensor 4: "); Serial.print( medidas_I.corriente_d_mA); Serial.print("mA "); Serial.print(medidas_V.voltaje_d_V); Serial.print("V ");
    Serial.print(medidas_P.potencia_d_mW); Serial.println("mW ");
  */
  if (millis() >= PeriodoPantalla + TiempoPantalla) {
    TiempoPantalla = millis();
    if (modo == 1)
    {
      if (cont == 1)
      {
        display.clearDisplay();
        display.setCursor(40, 0); display.println("SENSOR 1");
        display.setCursor(40, 16); display.print(medidas_P.potencia_mW); display.println(" mW");
        display.setCursor(40, 32); display.print(medidas_V.voltaje_V); display.println(" V");
        display.setCursor(40, 48); display.print(medidas_I.corriente_mA); display.println(" mA");
        display.display();
        cont = 2;
      }
      else if (cont == 2)
      {
        display.clearDisplay();
        display.setCursor(40, 0); display.println("SENSOR 2");
        display.setCursor(40, 16); display.print(medidas_P.potencia_b_mW); display.println(" mW");
        display.setCursor(40, 32); display.print(medidas_V.voltaje_b_V); display.println(" V");
        display.setCursor(40, 48); display.print(medidas_I.corriente_b_mA); display.println(" mA");
        display.display();
        cont = 3;
      }
      else if (cont == 3)
      {
        display.clearDisplay();
        display.setCursor(40, 0); display.println("SENSOR 3");
        display.setCursor(40, 16); display.print(medidas_P.potencia_c_mW); display.println(" mW");
        display.setCursor(40, 32); display.print(medidas_V.voltaje_c_V); display.println(" V");
        display.setCursor(40, 48); display.print(medidas_I.corriente_c_mA); display.println(" mA");
        display.display();
        cont = 4;
      }
      else
      {
        display.clearDisplay();
        display.setCursor(40, 0); display.println("SENSOR 4");
        display.setCursor(40, 16); display.print(medidas_P.potencia_d_mW); display.println(" mW");
        display.setCursor(40, 32); display.print(medidas_V.voltaje_d_V); display.println(" V");
        display.setCursor(40, 48); display.print(medidas_I.corriente_d_mA); display.println(" mA");
        display.display();
        cont = 1;
      }
    }
    else
    {
      if (cont == 1)
      {
        display.clearDisplay();
        display.setCursor(0, 0); display.println("SENSOR 1");
        display.setCursor(0, 16); display.print(medidas_P.potencia_mW); display.println(" mW");
        display.setCursor(64, 0); display.println("SENSOR 2");
        display.setCursor(64, 16); display.print(medidas_P.potencia_b_mW); display.println(" mW");
        display.setCursor(0, 32); display.println("SENSOR 3");
        display.setCursor(0, 48); display.print(medidas_P.potencia_c_mW); display.println(" mW");
        display.setCursor(64, 32); display.println("SENSOR 4");
        display.setCursor(64, 48); display.print(medidas_P.potencia_d_mW); display.println(" mW");
        display.display();
        cont = 2;
      }
      else if (cont == 2)
      {
        display.clearDisplay();
        display.setCursor(0, 0); display.println("SENSOR 1");
        display.setCursor(0, 16); display.print(medidas_V.voltaje_V); display.println(" V");
        display.setCursor(64, 0); display.println("SENSOR 2");
        display.setCursor(64, 16); display.print(medidas_V.voltaje_b_V); display.println(" V");
        display.setCursor(0, 32); display.println("SENSOR 3");
        display.setCursor(0, 48); display.print(medidas_V.voltaje_c_V); display.println(" V");
        display.setCursor(64, 32); display.println("SENSOR 4");
        display.setCursor(64, 48); display.print(medidas_V.voltaje_d_V); display.println(" V");
        display.display();
        cont = 3;
      }
      else
      {
        display.clearDisplay();
        display.setCursor(0, 0); display.println("SENSOR 1");
        display.setCursor(0, 16); display.print(medidas_I.corriente_mA); display.println(" mA");
        display.setCursor(64, 0); display.println("SENSOR 2");
        display.setCursor(64, 16); display.print(medidas_I.corriente_b_mA); display.println(" mA");
        display.setCursor(0, 32); display.println("SENSOR 3");
        display.setCursor(0, 48); display.print(medidas_I.corriente_c_mA); display.println(" mA");
        display.setCursor(64, 32); display.println("SENSOR 4");
        display.setCursor(64, 48); display.print(medidas_I.corriente_d_mA); display.println(" mA");
        display.display();
        cont = 1;
      }
    }
  }

  if (millis() >= PeriodoAlmacenamiento + TiempoAlmacenamiento) {
    TiempoAlmacenamiento = millis();

    memoria = SD.open("datos.csv", FILE_WRITE); // apertura para lectura/escritura de archivo datos.csv
    if (memoria) {

      Serial.println("Guardando datos en memoria microSD ");
      if (memoria.size() == 0)
      {
        memoria.print("FECHA ;"); memoria.print("HORA ;");   memoria.print("SENSOR 1 (mW) ;"); memoria.print("SENSOR 2 (mW) ;");
        memoria.print("SENSOR 3 (mW) ;"); memoria.println("SENSOR 4 (mW) ;");
      }
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
        memoria.print(";");
        memoria.print(t);
        memoria.print(";");
      }
      else
      {
        Serial.print("FalloConexiónWifi ");
        memoria.print("FalloConexiónWifi ");
      }
    memoria.print(medidas_P.potencia_mW); memoria.print(";");
    memoria.print(medidas_P.potencia_b_mW); memoria.print(";");
    memoria.print(medidas_P.potencia_c_mW); memoria.print(";");
    memoria.print(medidas_P.potencia_d_mW); memoria.println(";");

    memoria.close(); //cierra el archivo y garantiza que se escriban los datos correctamente
  }
  
  else
  {
    Serial.println("error en apertura de datos.txt");  // texto de falla en apertura de archivo
  }
  }
    if (!client.connected()) {
    reconnect();
    }

  if (millis() >= PeriodoEnvio + TiempoEnvio) {
    TiempoEnvio = millis();
    if (payload_envio == "Activar")
    {
      client.publish(consumo_topic, SerializeMedidas(medidas_P).c_str(), true);
    }
  }

  button.loop();
  client.loop();
  ftpSrv.handleFTP();
}

void handler(Button2& btn) {
  switch (btn.getType()) {
    case single_click:
      if (modo == 1)
      {
        Serial.println("Activado Modo 1: Todas las Variables");
        modo = 2;
        cont = 1;
      }
      else
      {
        Serial.println("Activado Modo 2: Todos los Sensores");
        modo = 1;
        cont = 1;
      }
    case double_click:
      Serial.println("Activado Formato Rápido (2s)");
      PeriodoPantalla = 3000;
      break;
    case long_click:
      Serial.println("Activado Formato Lento (60s)");
      PeriodoPantalla = 60000;
      break;
  }
}

String Obtener_Fecha(time_t local)
{
  date = "";
  if (day(local) < 10)
    date += "0";
  date += day(local);
  date += "/";
  if (month(local) < 10)
    date += "0";
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
  if (minute(local) < 10)
    t += "0";
  t += minute(local);
  t += ":";
  if (second(local) < 10)
    t += "0";
  t += second(local);
  return t;
}

void reconnect() {
  // BUCLE DE CHECKING DE CONEXIÓN AL SERVICIO MQTT
  while (!client.connected()) {
    Serial.print("Intentando conectarse a MQTT...");

    // Generación del nombre del cliente en función de la dirección MAC y los ultimos 8 bits del contador temporal
    String clientId = "ESP8266Client-";
    clientId += String(ESP.getChipId());
    Serial.print("Conectando a ");
    Serial.print(mqtt_server);
    Serial.print(" como ");
    Serial.println(clientId);

    // Intentando conectar
    if (client.connect(clientId.c_str(), mqtt_user, mqtt_password)) {
      Serial.println("Conectado");
      client.subscribe(envio_datos_topic);
      client.subscribe(envio_tiempo_topic);
      client.subscribe(almacenamiento_datos_topic);
      client.subscribe(almacenamiento_tiempo_topic);

    } else {
      Serial.print("Conexión Fallida, rc=");
      Serial.print(client.state());
      Serial.println(" Volver a probar en 5 segundos");
      // Espera 5s antes de reintentar conexión
      delay(5000);
    }
  }
}

String SerializeMedidas(struct conjunto_potencias medidas_P )
{
  String json;
  StaticJsonDocument<512> doc;
  doc["Sensor1"] = medidas_P.potencia_mW;
  doc["Sensor2"] = medidas_P.potencia_b_mW;
  doc["Sensor3"] = medidas_P.potencia_c_mW;
  doc["Sensor4"] = medidas_P.potencia_d_mW;
  serializeJson(doc, json);
  Serial.println(json);
  return json;
}

void callback(char* topic, byte* payload, unsigned int length)
{
  String conc_payload;
  if (strcmp(topic, envio_datos_topic) == 0)
  { for (int i = 0; i < length; i++) {
      conc_payload += (char)payload[i];
    }
    payload_envio = conc_payload;
  }
  else if (strcmp(topic, almacenamiento_datos_topic) == 0)
  { for (int i = 0; i < length; i++) {
      conc_payload += (char)payload[i];
    }
    payload_almacenamiento = conc_payload;
  }

  if (payload_envio == "Activar")
  {
    if (strcmp(topic, envio_tiempo_topic) == 0)
    {
      for (int i = 0; i < length; i++) {
        conc_payload += (char)payload[i];
      }
      PeriodoEnvio = conc_payload.toInt();
      Serial.println (PeriodoEnvio);
    }
  }

  if (payload_almacenamiento == "Activar")
  {
    if (strcmp(topic, almacenamiento_tiempo_topic) == 0)
    {
      for (int i = 0; i < length; i++) {
        conc_payload += (char)payload[i];
      }
      PeriodoAlmacenamiento = conc_payload.toInt();
      Serial.println (PeriodoAlmacenamiento);
    }
  }

  Serial.print("Comando recibido de MQTT broker es : [");
  Serial.print(topic);
  Serial.println("] ");

  delay (10);
}
