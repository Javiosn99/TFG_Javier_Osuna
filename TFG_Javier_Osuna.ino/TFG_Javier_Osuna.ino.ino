#include <Wire.h> //permite comuincarse por los pines I2C
#include <Adafruit_INA219.h> //librería para el uso del sensor

Adafruit_INA219 ina219;

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
}
void loop(void) 
{
  float current_mA = 0;

  // Obtener mediciones
  current_mA = ina219.getCurrent_mA();

  // Mostrar mediciones
  Serial.print("Corriente:       "); Serial.print(current_mA); Serial.println(" mA");

  delay(2000);
}
