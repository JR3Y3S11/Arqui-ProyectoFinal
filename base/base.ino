/*
 ==============================================================
        SISTEMA DE CONTROL DE ACCESO - VERSIÓN DE PRUEBA
 ==============================================================
 Hardware que se usa en esta versión:
   LCD I2C, un 74HC595 (LEDs), RC522, servo, buzzer activo, botón

 Sin teclado matricial: las teclas se escriben en el Monitor
 Serial (9600). Ejemplo: "1234#" escribe el PIN y lo confirma.

   0-9 -> dígitos del PIN
   *   -> borrar último dígito
   #   -> confirmar PIN
   C   -> cerrar la puerta
   D   -> apagar la alarma

 Sonidos:
   Tarjeta válida   -> 1 pip
   Tarjeta inválida -> 2 pips
   PIN correcto     -> 1 pip
   PIN incorrecto   -> 2 pips

 Cuando estén armados el teclado y el segundo 595:
   USAR_TECLADO 1  y  DOS_595 1
 ==============================================================
*/

#define USAR_TECLADO 0   // 0 = teclas por Monitor Serial, 1 = teclado matricial
#define DOS_595      0   // 0 = un solo 595 (LEDs), 1 = dos 595 en cadena
#define AUTOTEST     1   // 1 = al arrancar prueba LEDs, buzzer y servo

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Servo.h>
#include <SPI.h>
#include <MFRC522.h>
#include <EEPROM.h>


// ==============================================================
// 1. PINES
// ==============================================================
/*
 D2  -> Buzzer ACTIVO
 D3  -> Botón (a GND, INPUT_PULLUP): mantener al encender = registrar usuarios
 D4  -> Servo
 D6  -> RFID RST (divisor)
 D7  -> 74HC595 DATA
 D8  -> 74HC595 CLOCK
 D9  -> 74HC595 LATCH
 D10 -> RFID SS  (divisor)
 D11 -> RFID MOSI (divisor)
 D12 -> RFID MISO (directo)
 D13 -> RFID SCK (divisor)
 A0..A3 -> Keypad C1..C4 (solo si USAR_TECLADO 1)
 A4/A5 -> LCD SDA/SCL
*/

const byte PIN_BUZZER = 2;
const byte PIN_DIAG   = 3;
const byte PIN_SERVO  = 4;

const byte RFID_RST = 6;

const byte DATA  = 7;
const byte CLOCK = 8;
const byte LATCH = 9;

const byte RFID_SS = 10;

const byte columnas[4] = { A0, A1, A2, A3 };


// ==============================================================
// 2. DISPOSITIVOS Y CONSTANTES
// ==============================================================

LiquidCrystal_I2C lcd(0x27, 16, 2);   // cambiar a 0x3F si no se ve nada

Servo puerta;

MFRC522 rfid(RFID_SS, RFID_RST);

const byte ANGULO_CERRADA = 0;
const byte ANGULO_ABIERTA = 90;

const char TECLA_CERRAR_PUERTA = 'C';
const char TECLA_APAGAR_ALARMA = 'D';


// ==============================================================
// 3. REGISTROS 74HC595
// ==============================================================

byte registro1 = B00001111;   // filas del teclado (solo con DOS_595 1)
byte registro2 = B00000000;   // LEDs de estado

const byte LED_INICIO         = B00000001;   // LED 1
const byte LED_ESPERANDO_RFID = B00000010;   // LED 2
const byte LED_VALIDANDO_RFID = B00000100;   // LED 3
const byte LED_ESPERANDO_PIN  = B00001000;   // LED 4
const byte LED_VALIDANDO_PIN  = B00010000;   // LED 5
const byte LED_AUTORIZADO     = B00100000;   // LED 6
const byte LED_DENEGADO       = B01000000;   // LED 7
const byte LED_ALARMA         = B10000000;   // LED 8


// ==============================================================
// 4. VARIABLES DEL SISTEMA
// ==============================================================

#if USAR_TECLADO
char teclas[4][4] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};
#endif

byte uidLeido[4];
byte uidTam = 0;

char pinIngresado[5] = "";
byte posicionPIN = 0;

byte intentosFallidos = 0;

// EEPROM: [MAGIC][UID 4 bytes][PIN 4 caracteres] x NUM_USUARIOS
const byte NUM_USUARIOS = 2;
const byte DIR_MAGIC    = 0;
const byte VALOR_MAGIC  = 0x45;
const byte DIR_USUARIOS = 1;
const byte TAM_REGISTRO = 8;

int usuarioActual = -1;

enum EstadoSistema
{
  ESPERANDO_RFID,
  ESPERANDO_PIN,
  VALIDANDO_PIN,
  ACCESO_AUTORIZADO,
  ACCESO_DENEGADO,
  PUERTA_ABIERTA,
  ALARMA
};

EstadoSistema estado = ESPERANDO_RFID;

void reiniciarSistema();   // prototipo (se usa antes de definirse)


// ==============================================================
// 5. 74HC595
// ==============================================================

void actualizarRegistro()
{
  digitalWrite(LATCH, LOW);

  shiftOut(DATA, CLOCK, MSBFIRST, registro2);          // LEDs

#if DOS_595
  shiftOut(DATA, CLOCK, MSBFIRST, registro1);          // teclado
#endif

  digitalWrite(LATCH, HIGH);
}

void mostrarEstadoLED(byte valor)
{
  registro2 = valor;
  actualizarRegistro();
}

void apagarLEDs()
{
  mostrarEstadoLED(0);
}


// ==============================================================
// 6. LCD
// ==============================================================

void mostrar(const __FlashStringHelper* linea1, const __FlashStringHelper* linea2)
{
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(linea1);
  lcd.setCursor(0, 1);
  lcd.print(linea2);
}


// ==============================================================
// 7. TECLADO (matricial o Monitor Serial)
// ==============================================================

#if USAR_TECLADO
void seleccionarFila(byte fila)
{
  registro1 |= B00001111;
  bitClear(registro1, fila);
  actualizarRegistro();
}
#endif

char leerTeclado()
{
#if USAR_TECLADO

  for (byte fila = 0; fila < 4; fila++)
  {
    seleccionarFila(fila);
    delayMicroseconds(10);

    for (byte columna = 0; columna < 4; columna++)
    {
      if (digitalRead(columnas[columna]) == LOW)
      {
        delay(30);   // anti-rebote

        if (digitalRead(columnas[columna]) == LOW)
        {
          char tecla = teclas[fila][columna];

          while (digitalRead(columnas[columna]) == LOW) delay(5);

          return tecla;
        }
      }
    }
  }

  return 0;

#else

  // Sin teclado: cada carácter escrito en el Monitor Serial es una tecla
  while (Serial.available())
  {
    char c = Serial.read();

    if (c == '\n' || c == '\r' || c == ' ') continue;

    if (c >= 'a' && c <= 'z') c = c - 32;   // minúsculas -> mayúsculas

    return c;
  }

  return 0;

#endif
}


// ==============================================================
// 8. BUZZER ACTIVO
// ==============================================================

void pitar(unsigned int ms)
{
  digitalWrite(PIN_BUZZER, HIGH);
  delay(ms);
  digitalWrite(PIN_BUZZER, LOW);
}

// Correcto: un solo pip
void sonidoCorrecto()
{
  pitar(150);
}

// Error: dos pips
void sonidoError()
{
  pitar(150);
  delay(150);
  pitar(150);
}


// ==============================================================
// 9. RFID
// ==============================================================

bool leerTarjeta()
{
  if (!rfid.PICC_IsNewCardPresent()) return false;
  if (!rfid.PICC_ReadCardSerial())   return false;

  uidTam = rfid.uid.size;

  for (byte i = 0; i < 4; i++)
  {
    uidLeido[i] = (i < uidTam) ? rfid.uid.uidByte[i] : 0;
  }

  Serial.print(F("UID: "));
  for (byte i = 0; i < uidTam; i++)
  {
    if (rfid.uid.uidByte[i] < 0x10) Serial.print('0');
    Serial.print(rfid.uid.uidByte[i], HEX);
  }
  Serial.println();

  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();

  return true;
}


// ==============================================================
// 10. EEPROM: USUARIOS
// ==============================================================

bool usuariosRegistrados()
{
  return EEPROM.read(DIR_MAGIC) == VALOR_MAGIC;
}

bool uidCoincide(byte slot)
{
  int base = DIR_USUARIOS + slot * TAM_REGISTRO;

  for (byte i = 0; i < 4; i++)
  {
    if (EEPROM.read(base + i) != uidLeido[i]) return false;
  }

  return true;
}

bool tarjetaAutorizada()
{
  usuarioActual = -1;

  if (uidTam != 4) return false;

  for (byte u = 0; u < NUM_USUARIOS; u++)
  {
    if (uidCoincide(u))
    {
      usuarioActual = u;
      return true;
    }
  }

  return false;
}

bool validarPIN()
{
  if (usuarioActual < 0) return false;

  int base = DIR_USUARIOS + usuarioActual * TAM_REGISTRO + 4;

  for (byte i = 0; i < 4; i++)
  {
    if (EEPROM.read(base + i) != pinIngresado[i]) return false;
  }

  return true;
}


// ==============================================================
// 11. CAPTURA DEL PIN
// ==============================================================

void dibujarPIN()
{
  lcd.setCursor(0, 1);

  for (byte i = 0; i < 4; i++)
  {
    lcd.print(i < posicionPIN ? '*' : '_');
  }
}

void mostrarPantallaPIN()
{
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(F("INGRESE SU PIN"));
  dibujarPIN();
}

bool leerPIN()
{
  char tecla = leerTeclado();

  if (tecla == 0) return false;

  if (tecla >= '0' && tecla <= '9')
  {
    if (posicionPIN < 4)
    {
      pinIngresado[posicionPIN] = tecla;
      posicionPIN++;
      pinIngresado[posicionPIN] = '\0';
      dibujarPIN();
    }
  }
  else if (tecla == '*')
  {
    if (posicionPIN > 0)
    {
      posicionPIN--;
      pinIngresado[posicionPIN] = '\0';
      dibujarPIN();
    }
  }
  else if (tecla == '#')
  {
    if (posicionPIN == 4) return true;
  }

  return false;
}


// ==============================================================
// 12. REGISTRO DE USUARIOS EN EEPROM
// ==============================================================

void registrarUsuarios()
{
  for (byte u = 0; u < NUM_USUARIOS; u++)
  {
    // ---- 1. Tarjeta ----
    bool tarjetaOk = false;

    mostrarEstadoLED(LED_ESPERANDO_RFID);

    while (!tarjetaOk)
    {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print(F("REGISTRO USER "));
      lcd.print(u + 1);
      lcd.setCursor(0, 1);
      lcd.print(F("PASE TARJETA"));

      while (!leerTarjeta()) { }

      if (uidTam != 4)
      {
        mostrar(F("TARJETA NO"), F("COMPATIBLE"));
        sonidoError();
        delay(1200);
        continue;
      }

      tarjetaOk = true;

      for (byte s = 0; s < u; s++)
      {
        if (uidCoincide(s)) tarjetaOk = false;
      }

      if (!tarjetaOk)
      {
        mostrar(F("TARJETA YA"), F("REGISTRADA"));
        sonidoError();
        delay(1200);
      }
    }

    sonidoCorrecto();   // 1 pip: tarjeta aceptada

    // ---- 2. PIN ----
    mostrarEstadoLED(LED_ESPERANDO_PIN);

    posicionPIN = 0;
    pinIngresado[0] = '\0';

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(F("NUEVO PIN USER "));
    lcd.print(u + 1);
    dibujarPIN();

    while (!leerPIN()) { }

    // ---- 3. Guardar ----
    int base = DIR_USUARIOS + u * TAM_REGISTRO;

    for (byte i = 0; i < 4; i++) EEPROM.update(base + i, uidLeido[i]);
    for (byte i = 0; i < 4; i++) EEPROM.update(base + 4 + i, pinIngresado[i]);

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(F("USUARIO "));
    lcd.print(u + 1);
    lcd.setCursor(0, 1);
    lcd.print(F("GUARDADO"));
    sonidoCorrecto();   // 1 pip: usuario guardado
    delay(1200);
  }

  // La marca se escribe al final: si se interrumpe, se repite el registro
  EEPROM.update(DIR_MAGIC, VALOR_MAGIC);

  mostrar(F("REGISTRO"), F("COMPLETO"));
  delay(1500);
}


// ==============================================================
// 13. SERVO
// ==============================================================

void abrirPuerta()
{
  puerta.write(ANGULO_ABIERTA);
  delay(500);
}

void cerrarPuerta()
{
  puerta.write(ANGULO_CERRADA);
  delay(500);
}


// ==============================================================
// 14. ALARMA
// ==============================================================

void activarAlarma()
{
  digitalWrite(PIN_BUZZER, HIGH);

  while (leerTeclado() != TECLA_APAGAR_ALARMA) { }

  digitalWrite(PIN_BUZZER, LOW);

  intentosFallidos = 0;
  apagarLEDs();
  reiniciarSistema();
}


// ==============================================================
// 15. REINICIAR CICLO
// ==============================================================

void reiniciarSistema()
{
  uidTam = 0;
  usuarioActual = -1;
  posicionPIN = 0;
  pinIngresado[0] = '\0';

  mostrar(F("CONTROL ACCESO"), F("PRESENTE TARJETA"));

  mostrarEstadoLED(LED_ESPERANDO_RFID);

  estado = ESPERANDO_RFID;
}


// ==============================================================
// 16. SETUP
// ==============================================================

void setup()
{
  Serial.begin(9600);

  lcd.init();
  lcd.backlight();

  pinMode(DATA, OUTPUT);
  pinMode(CLOCK, OUTPUT);
  pinMode(LATCH, OUTPUT);

#if USAR_TECLADO
  for (byte i = 0; i < 4; i++) pinMode(columnas[i], INPUT_PULLUP);
#endif

  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_BUZZER, LOW);

  pinMode(PIN_DIAG, INPUT_PULLUP);

  puerta.attach(PIN_SERVO);
  puerta.write(ANGULO_CERRADA);

  SPI.begin();
  rfid.PCD_Init();

  mostrar(F("SISTEMA ACCESO"), F("INICIANDO..."));
  mostrarEstadoLED(LED_INICIO);
  delay(1500);

#if AUTOTEST
  // Prueba de salidas: LEDs 1..8, buzzer y servo
  for (byte i = 0; i < 8; i++)
  {
    mostrarEstadoLED(1 << i);
    delay(250);
  }
  apagarLEDs();

  pitar(150);

  puerta.write(ANGULO_ABIERTA);
  delay(700);
  puerta.write(ANGULO_CERRADA);
  delay(700);
#endif

  // Registro si la EEPROM está vacía o si se mantiene el botón D3
  if (!usuariosRegistrados() || digitalRead(PIN_DIAG) == LOW)
  {
    registrarUsuarios();
  }

  reiniciarSistema();
}


// ==============================================================
// 17. LOOP (MÁQUINA DE ESTADOS)
// ==============================================================

void loop()
{
  switch (estado)
  {
    case ESPERANDO_RFID:

      if (leerTarjeta())
      {
        mostrarEstadoLED(LED_VALIDANDO_RFID);
        delay(300);

        if (tarjetaAutorizada())
        {
          sonidoCorrecto();          // 1 pip: tarjeta válida

          intentosFallidos = 0;
          mostrarPantallaPIN();
          mostrarEstadoLED(LED_ESPERANDO_PIN);
          estado = ESPERANDO_PIN;
        }
        else
        {
          estado = ACCESO_DENEGADO;  // aquí suenan los 2 pips
        }
      }

      break;


    case ESPERANDO_PIN:

      if (leerPIN())
      {
        estado = VALIDANDO_PIN;
      }

      break;


    case VALIDANDO_PIN:

      mostrarEstadoLED(LED_VALIDANDO_PIN);
      delay(300);

      if (validarPIN())
      {
        estado = ACCESO_AUTORIZADO;  // aquí suena 1 pip
      }
      else
      {
        intentosFallidos++;

        if (intentosFallidos >= 3)
        {
          estado = ACCESO_DENEGADO;
        }
        else
        {
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print(F("PIN INCORRECTO"));
          lcd.setCursor(0, 1);
          lcd.print(F("INTENTO "));
          lcd.print(intentosFallidos);
          lcd.print(F("/3"));

          mostrarEstadoLED(LED_DENEGADO);
          sonidoError();             // 2 pips: PIN incorrecto
          delay(1500);

          posicionPIN = 0;
          pinIngresado[0] = '\0';

          mostrarPantallaPIN();
          mostrarEstadoLED(LED_ESPERANDO_PIN);
          estado = ESPERANDO_PIN;
        }
      }

      break;


    case ACCESO_AUTORIZADO:

      mostrar(F("ACCESO"), F("AUTORIZADO"));

      mostrarEstadoLED(LED_AUTORIZADO);

      sonidoCorrecto();              // 1 pip: PIN correcto
      abrirPuerta();

      delay(1000);

      mostrar(F("PUERTA ABIERTA"), F("C = CERRAR"));

      estado = PUERTA_ABIERTA;

      break;


    case ACCESO_DENEGADO:

      mostrarEstadoLED(LED_DENEGADO);

      if (usuarioActual < 0)
      {
        mostrar(F("TARJETA INVALIDA"), F("ACCESO DENEGADO"));
        sonidoError();               // 2 pips: tarjeta inválida
        delay(1500);

        reiniciarSistema();
      }
      else
      {
        mostrar(F("ACCESO DENEGADO"), F(""));
        sonidoError();               // 2 pips: tercer PIN incorrecto
        delay(1500);

        mostrar(F("ACCESO BLOQUEADO"), F("ALARMA ACTIVADA"));

        estado = ALARMA;
      }

      break;


    case PUERTA_ABIERTA:

      if (leerTeclado() == TECLA_CERRAR_PUERTA)
      {
        cerrarPuerta();
        intentosFallidos = 0;
        reiniciarSistema();
      }

      break;


    case ALARMA:

      mostrarEstadoLED(LED_ALARMA);

      activarAlarma();   // bloquea hasta recibir 'D'

      break;
  }
}