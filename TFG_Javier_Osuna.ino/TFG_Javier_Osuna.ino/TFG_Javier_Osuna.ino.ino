#include <Wire.h> //permite comuincarse por los pines I2C
#include <Adafruit_INA219.h> //librería para el uso del sensor
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// Definir constantes
#define ANCHO_PANTALLA 128 // ancho pantalla OLED
#define ALTO_PANTALLA 64 // alto pantalla OLED

Adafruit_INA219 ina219;

// Objeto de la clase Adafruit_SSD1306 con el ancho, el alto, el puntero y el reset
Adafruit_SSD1306 display(ANCHO_PANTALLA, ALTO_PANTALLA, &Wire, -1);

void setup(void)
{
  Serial.begin(115200);
  uint32_t currentFrequency;

  // Iniciar el INA219
  ina219.begin();  //por defecto, inicia a 32V y 2A
  // Opcionalmente, cambiar la sensibilidad del sensor
  //ina219.setCalibration_32V_1A();
  ina219.setCalibration_16V_400mA();
  Serial.println("INA219 iniciado...");

    delay(100);
  Serial.println("Iniciando pantalla OLED");
  // Iniciar pantalla OLED en la dirección 0x3C, SSD1306_SWITCHCAPVCC indica voltaje 3,3V interno de pantalla
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) 
  {
    Serial.println("No se encuentra la pantalla OLED");
    while (true);
  }

}
void loop(void) 
{
  float corriente_mA = 0;

  // Obtener mediciones
  corriente_mA = ina219.getCurrent_mA();
  
 // Limpiar buffer
  display.clearDisplay();
 
  // Tamaño del texto
  display.setTextSize(1);
  // Color del texto
  display.setTextColor(SSD1306_WHITE);
  // Posición del texto
  display.setCursor(0, 0);
  // Escribir texto
  display.println("SENSOR 1");
  display.setCursor(0, 15);
  display.print(corriente_mA);
  display.println(" mA");
 
  // Enviar a pantalla
  display.display();
  
  // Mostrar mediciones
  Serial.print("Corriente:       "); Serial.print(corriente_mA); Serial.println(" mA");

  delay(2000);
}
