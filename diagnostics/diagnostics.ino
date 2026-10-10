#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Servo.h>

#include <SPI.h>
#include <MFRC522.h>


// =====================================================
// MAPA DE PINES
// =====================================================

const byte PIN_BUZZER      = 2;
const byte PIN_DIAGNOSTICO = 3;
const byte PIN_SERVO       = 4;

// D5 queda libre

// RFID RC522
const byte RFID_RST = 6;
const byte RFID_SS  = 10;

// 74HC595
const byte DATA  = 7;
const byte CLOCK = 8;
const byte LATCH = 9;

// Keypad
const byte columnas[4] = {A0, A1, A2, A3};


// =====================================================
// LCD
// =====================================================

LiquidCrystal_I2C lcd(0x27, 16, 2);


// =====================================================
// SERVO
// =====================================================

Servo puerta;


// =====================================================
// RFID RC522
// =====================================================

MFRC522 rfid(RFID_SS, RFID_RST);


// =====================================================
// 74HC595
// =====================================================

/*
   REGISTRO #1
   ------------------------
   Q0 -> Keypad R1
   Q1 -> Keypad R2
   Q2 -> Keypad R3
   Q3 -> Keypad R4

   Q4 -> LED verde
   Q5 -> LED rojo
   Q6 -> LED amarillo
   Q7 -> LED azul


   REGISTRO #2
   ------------------------
   Actualmente libre.
*/

byte registro1 = B00001111;
byte registro2 = B00000000;


// =====================================================
// KEYPAD
// =====================================================

char teclas[4][4] = {

  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}

};


// =====================================================
// ACTUALIZAR LOS DOS 74HC595
// =====================================================

void actualizarRegistros()
{
  digitalWrite(LATCH, LOW);

  // Segundo 74HC595
  shiftOut(DATA, CLOCK, MSBFIRST, registro2);

  // Primer 74HC595
  shiftOut(DATA, CLOCK, MSBFIRST, registro1);

  digitalWrite(LATCH, HIGH);
}


// =====================================================
// SELECCIONAR FILA DEL KEYPAD
// =====================================================

void seleccionarFila(byte fila)
{
  // Q0-Q3 en HIGH
  registro1 |= B00001111;

  // Fila seleccionada en LOW
  bitClear(registro1, fila);

  actualizarRegistros();
}


// =====================================================
// LEER KEYPAD
// =====================================================

char leerTeclado()
{
  for (byte fila = 0; fila < 4; fila++)
  {
    seleccionarFila(fila);

    delayMicroseconds(10);

    for (byte columna = 0; columna < 4; columna++)
    {
      if (digitalRead(columnas[columna]) == LOW)
      {
        // Anti-rebote
        delay(30);

        if (digitalRead(columnas[columna]) == LOW)
        {
          char tecla = teclas[fila][columna];

          // Esperar hasta que se suelte
          while (digitalRead(columnas[columna]) == LOW)
          {
            delay(5);
          }

          return tecla;
        }
      }
    }
  }

  return 0;
}


// =====================================================
// CONTROL DE LEDs
// =====================================================

void apagarLEDs()
{
  /*
     Q4-Q7 -> LEDs
     Q0-Q3 -> Keypad

     Conservamos los cuatro bits inferiores.
  */

  registro1 &= B00001111;

  actualizarRegistros();
}


void encenderLED(byte numero)
{
  apagarLEDs();

  /*
     0 -> Q4 -> Verde
     1 -> Q5 -> Rojo
     2 -> Q6 -> Amarillo
     3 -> Q7 -> Azul
  */

  bitSet(registro1, numero + 4);

  actualizarRegistros();
}


// =====================================================
// BUZZER
// =====================================================

void sonidoAmigable()
{
  // SOL4
  tone(PIN_BUZZER, 392);
  delay(120);

  noTone(PIN_BUZZER);
  delay(50);

  // DO5
  tone(PIN_BUZZER, 523);
  delay(180);

  noTone(PIN_BUZZER);
}


// =====================================================
// CONVERTIR UID RFID A TEXTO
// =====================================================

String obtenerUID()
{
  String uid = "";

  for (byte i = 0; i < rfid.uid.size; i++)
  {
    // Agregar cero delante si el byte es menor a 0x10
    if (rfid.uid.uidByte[i] < 0x10)
    {
      uid += "0";
    }

    uid += String(rfid.uid.uidByte[i], HEX);

    // Separar bytes con :
    if (i < rfid.uid.size - 1)
    {
      uid += ":";
    }
  }

  uid.toUpperCase();

  return uid;
}


// =====================================================
// REVISAR SI HAY UNA TARJETA RFID
// =====================================================

void revisarRFID()
{
  // ¿Hay una nueva tarjeta?
  if (!rfid.PICC_IsNewCardPresent())
  {
    return;
  }

  // ¿Se puede leer?
  if (!rfid.PICC_ReadCardSerial())
  {
    return;
  }


  // ===================================================
  // OBTENER UID
  // ===================================================

  String uid = obtenerUID();


  // ===================================================
  // MOSTRAR POR SERIAL
  // ===================================================

  Serial.println();  
  Serial.println("------------------------------");
  Serial.println(" TARJETA RFID DETECTADA");
  Serial.println("------------------------------");

  Serial.print("UID: ");
  Serial.println(uid);


  // ===================================================
  // MOSTRAR EN LCD
  // ===================================================

  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("RFID DETECTADO");

  lcd.setCursor(0, 1);

  /*
     El LCD tiene solamente 16 caracteres.

     Un UID normal de 4 bytes ocupa:
     XX:XX:XX:XX

     Por lo que cabe perfectamente.
  */

  if (uid.length() <= 16)
  {
    lcd.print(uid);
  }
  else
  {
    // Si encontramos una tarjeta con UID más largo,
    // mostramos solamente los primeros 16 caracteres.
    lcd.print(uid.substring(0, 16));
  }


  // ===================================================
  // FEEDBACK
  // ===================================================

  encenderLED(0);

  sonidoAmigable();

  delay(1500);

  apagarLEDs();


  // ===================================================
  // TERMINAR COMUNICACION CON TARJETA
  // ===================================================

  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();


  // ===================================================
  // REGRESAR A PANTALLA PRINCIPAL
  // ===================================================

  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("ACERQUE TARJETA");

  lcd.setCursor(0, 1);
  lcd.print("O USE KEYPAD");
}

// =====================================================
// PRUEBA LCD
// =====================================================

void probarLCD()
{
  Serial.println("1. Probando LCD...");

  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("PRUEBA LCD");

  lcd.setCursor(0, 1);
  lcd.print("FUNCIONANDO");

  delay(1500);

  Serial.println("   LCD OK");
}


// =====================================================
// PRUEBA LEDs
// =====================================================

void probarLEDs()
{
  Serial.println("2. Probando LEDs...");

  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("PRUEBA LEDs");


  // VERDE

  lcd.setCursor(0, 1);
  lcd.print("VERDE           ");

  encenderLED(0);

  delay(600);


  // ROJO

  lcd.setCursor(0, 1);
  lcd.print("ROJO            ");

  encenderLED(1);

  delay(600);


  // AMARILLO

  lcd.setCursor(0, 1);
  lcd.print("AMARILLO        ");

  encenderLED(2);

  delay(600);


  // AZUL

  lcd.setCursor(0, 1);
  lcd.print("AZUL            ");

  encenderLED(3);

  delay(600);


  apagarLEDs();

  Serial.println("   LEDs OK");
}


// =====================================================
// PRUEBA BUZZER
// =====================================================

void probarBuzzer()
{
  Serial.println("3. Probando buzzer...");

  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("PRUEBA BUZZER");

  lcd.setCursor(0, 1);
  lcd.print("SONIDO");

  sonidoAmigable();

  delay(1000);

  Serial.println("   Buzzer OK");
}


// =====================================================
// PRUEBA SERVO
// =====================================================

void probarServo()
{
  Serial.println("4. Probando servo...");

  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("PRUEBA SERVO");


  // CERRADO

  lcd.setCursor(0, 1);
  lcd.print("CERRADO         ");

  puerta.write(20);

  delay(1200);


  // ABIERTO

  lcd.setCursor(0, 1);
  lcd.print("ABIERTO         ");

  puerta.write(90);

  delay(1500);


  // CERRADO

  lcd.setCursor(0, 1);
  lcd.print("CERRADO         ");

  puerta.write(20);

  delay(1200);

  Serial.println("   Servo OK");
}


// =====================================================
// DIAGNOSTICO COMPLETO
// =====================================================

void ejecutarDiagnostico()
{
  Serial.println();
  Serial.println("==============================");
  Serial.println(" INICIANDO DIAGNOSTICO");
  Serial.println("==============================");


  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("DIAGNOSTICO");

  lcd.setCursor(0, 1);
  lcd.print("INICIANDO...");

  delay(1000);


  // ===================================================
  // EJECUTAR PRUEBAS
  // ===================================================

  probarLCD();

  probarLEDs();

  probarBuzzer();

  probarServo();


  // ===================================================
  // ESTADO FINAL
  // ===================================================

  apagarLEDs();

  puerta.write(20);


  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("DIAGNOSTICO");

  lcd.setCursor(0, 1);
  lcd.print("FINALIZADO");


  Serial.println();
  Serial.println("==============================");
  Serial.println(" DIAGNOSTICO FINALIZADO");
  Serial.println("==============================");

  delay(1500);


  // ===================================================
  // PANTALLA PRINCIPAL
  // ===================================================

  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("ACERQUE TARJETA");

  lcd.setCursor(0, 1);
  lcd.print("O USE KEYPAD");
}


// =====================================================
// SETUP
// =====================================================

void setup()
{
  // ===================================================
  // SERIAL
  // ===================================================

  Serial.begin(9600);

  Serial.println();
  Serial.println("==============================");
  Serial.println(" PROYECTO FINAL");
  Serial.println(" DIAGNOSTICO DE HARDWARE");
  Serial.println("==============================");


  // ===================================================
  // LCD
  // ===================================================

  lcd.init();

  lcd.backlight();


  // ===================================================
  // 74HC595
  // ===================================================

  pinMode(DATA, OUTPUT);
  pinMode(CLOCK, OUTPUT);
  pinMode(LATCH, OUTPUT);

  registro1 = B00001111;
  registro2 = B00000000;

  actualizarRegistros();


  // ===================================================
  // KEYPAD
  // ===================================================

  for (byte i = 0; i < 4; i++)
  {
    pinMode(columnas[i], INPUT_PULLUP);
  }


  // ===================================================
  // BUZZER
  // ===================================================

  pinMode(PIN_BUZZER, OUTPUT);


  // ===================================================
  // BOTON DIAGNOSTICO
  // ===================================================

  pinMode(PIN_DIAGNOSTICO, INPUT_PULLUP);


  // ===================================================
  // SERVO
  // ===================================================

  puerta.attach(PIN_SERVO);

  puerta.write(20);


  // ===================================================
  // RFID RC522
  // ===================================================

  /*
     Inicializar bus SPI.

     En Arduino Uno:

     SS   = D10
     MOSI = D11
     MISO = D12
     SCK  = D13
  */

  SPI.begin();

  // Inicializar RC522
  rfid.PCD_Init();

  Serial.println("RC522 inicializado");


  // ===================================================
  // DIAGNOSTICO
  // ===================================================

  ejecutarDiagnostico();
}


// =====================================================
// LOOP
// =====================================================

void loop()
{
  // ===================================================
  // BOTON DIAGNOSTICO
  // ===================================================

  if (digitalRead(PIN_DIAGNOSTICO) == LOW)
  {
    delay(50);

    if (digitalRead(PIN_DIAGNOSTICO) == LOW)
    {
      // Esperar hasta soltar el boton

      while (digitalRead(PIN_DIAGNOSTICO) == LOW)
      {
        delay(10);
      }

      ejecutarDiagnostico();
    }
  }


  // ===================================================
  // REVISAR RFID
  // ===================================================

  revisarRFID();


  // ===================================================
  // REVISAR KEYPAD
  // ===================================================

  char tecla = leerTeclado();

  if (tecla != 0)
  {
    lcd.clear();

    lcd.setCursor(0, 0);
    lcd.print("TECLA:");

    lcd.setCursor(0, 1);
    lcd.print(tecla);


    Serial.print("Tecla presionada: ");
    Serial.println(tecla);


    // Feedback visual

    encenderLED(0);

    delay(100);

    apagarLEDs();


    delay(500);


    // Volver a pantalla principal

    lcd.clear();

    lcd.setCursor(0, 0);
    lcd.print("ACERQUE TARJETA");

    lcd.setCursor(0, 1);
    lcd.print("O USE KEYPAD");
  }
}
