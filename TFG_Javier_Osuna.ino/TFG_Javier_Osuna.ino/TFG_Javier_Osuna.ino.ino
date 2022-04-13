#include <Wire.h> //permite comuincarse por los pines I2C
#include <Adafruit_INA219.h> //librería para el uso del sensor

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h> //ambas para el uso de la pantalla

#include <SPI.h> //permite comunicarse por los pines SPI
#include <SD.h> //ya incluida en arduino, permite usar la tarjeta microSD

// Definir constantes
#define ANCHO_PANTALLA 128 // ancho pantalla OLED
#define ALTO_PANTALLA 64 // alto pantalla OLED

#define CSpin D8    // Slave Select en pin digital 8

File memoria;      // objeto donde se realiza el almacenamiento de datos del tipo File

// Objeto de la clase Adafruit_SSD1306 con el ancho, el alto, el puntero y el reset
Adafruit_SSD1306 display(ANCHO_PANTALLA, ALTO_PANTALLA, &Wire, -1);

Adafruit_INA219 ina219;
void setup(void)
{
  Serial.begin(115200);
  uint32_t currentFrequency;
  delay(500);

  // Iniciar el INA219
  ina219.begin();  //por defecto, inicia a 32V y 2A
  // Opcionalmente, cambiar la sensibilidad del sensor
  //ina219.setCalibration_32V_1A();
  ina219.setCalibration_16V_400mA();
  Serial.println("INA219 iniciado...");
  delay(500);

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
  delay(500);

  Serial.println("Inicializando tarjeta ...");  // texto en ventana de monitor
  if (!SD.begin(CSpin)) {     // inicializacion de tarjeta SD
    Serial.println("fallo en inicializacion !");// si falla se muestra texto correspondiente y
    return;        // impide que el programa avance
  }
  Serial.println("inicializacion correcta");  // texto de inicializacion correcta
  delay(500);
}
void loop(void)
{
  float corriente_mA = 0;

  // Obtener mediciones
  corriente_mA = ina219.getCurrent_mA();

  // Limpiar buffer
  display.clearDisplay();
  display.setCursor(0, 0);
  // Escribir texto
  display.println("SENSOR 1");
  display.setCursor(0, 15);
  display.print(corriente_mA);
  display.println(" mA");

  // Enviar a pantalla
  display.display();

  memoria = SD.open("datos.txt", FILE_WRITE); // apertura para lectura/escritura de archivo datos.txt
  if (memoria) {
    Serial.print("Guardando datos en memoria microSD ");
    memoria.print(" SENSOR 1: ");
    memoria.print(corriente_mA);
    memoria.println(" mA");// Mostrar mediciones
    Serial.print("SENSOR 1: ");
    Serial.print(corriente_mA); Serial.println(" mA");
    memoria.close(); //cierra el archivo y garantiza que se escriban los datos correctamente
  } else {
    Serial.println("error en apertura de datos.txt");  // texto de falla en apertura de archivo
  }

  delay(2000);
}
