/*
 ==============================================================
             ARQUITECTURA DEL COMPUTADOR
        PROYECTO: SISTEMA DE CONTROL DE ACCESO (VARIANTE A)
 ==============================================================
 Josué Reyes
 José De la Roca

 VERSIÓN CON BUZZER PASIVO (se maneja con tone() / noTone())

 Distribución del teclado (serigrafía de la placa):
   S1  S2  S3  S4   ->  1 2 3 A
   S5  S6  S7  S8   ->  4 5 6 B
   S9  S10 S11 S12  ->  7 8 9 C
   S13 S14 S15 S16  ->  0 * # D

 Teclas especiales:
   0-9      -> dígitos del PIN
   S14 (*)  -> borrar último dígito / cancelar
   S15 (#)  -> confirmar PIN / confirmar borrado
   S12 (C)  -> cerrar la puerta
   S16 (D)  -> apagar la alarma
   S4  (A)  -> borrar TODOS los usuarios (en la pantalla de
               espera); pide confirmación: # = sí, * = no

 Sonidos:
   Tarjeta válida   -> 1 pip agudo
   Tarjeta inválida -> 2 pips graves
   PIN correcto     -> 1 pip agudo
   PIN incorrecto   -> 2 pips graves
   Alarma           -> sirena continua hasta presionar D

 Usuarios (EEPROM):
   - Si la EEPROM está vacía, el sistema pide registrar los
     usuarios al arrancar (tarjeta + PIN).
   - Para borrar todos los usuarios: en la pantalla
     "PRESENTE TARJETA" presionar A (S4) y confirmar con #.
     Luego el sistema pide registrar usuarios nuevos.
 ==============================================================
*/

#define AUTOTEST 1   // 1 = al arrancar prueba LEDs, buzzer y servo

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Servo.h>
#include <SPI.h>
#include <MFRC522.h>
#include <EEPROM.h>


// ==============================================================
// 1. MAPA DE PINES
// ==============================================================
/*
 D0  -> Serial RX (no se usa)
 D1  -> Serial TX (no se usa)
 D2  -> Buzzer PASIVO (+)
 D3  -> Libre
 D4  -> Servo
 D5  -> Libre
 D6  -> RFID RST        (con divisor)
 D7  -> 74HC595 DATA    (al chip 1, teclado)
 D8  -> 74HC595 CLOCK   (chip 1 y chip 2)
 D9  -> 74HC595 LATCH   (chip 1 y chip 2)
 D10 -> RFID SDA / SS   (con divisor)
 D11 -> RFID MOSI       (con divisor)
 D12 -> RFID MISO       (directo)
 D13 -> RFID SCK        (con divisor)
 A0..A3 -> Keypad C1..C4
 A4  -> LCD SDA
 A5  -> LCD SCL
*/

const byte PIN_BUZZER = 2;
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

const byte ANGULO_CERRADA = 50;
const byte ANGULO_ABIERTA = 130;

const char TECLA_CERRAR_PUERTA   = 'C';   // S12
const char TECLA_APAGAR_ALARMA   = 'D';   // S16
const char TECLA_BORRAR_USUARIOS = 'A';   // S4

// Frecuencias del buzzer pasivo (Hz)
const unsigned int FREQ_CORRECTO     = 2000;   // pip agudo
const unsigned int FREQ_ERROR        = 500;    // pips graves
const unsigned int FREQ_ALARMA_ALTA  = 1800;   // sirena: tono alto
const unsigned int FREQ_ALARMA_BAJA  = 1200;   // sirena: tono bajo


// ==============================================================
// 3. REGISTROS DE DESPLAZAMIENTO 74HC595 (ENCADENADOS)
// ==============================================================
/*
 Arduino -> Chip 1 (teclado) -> Q7' -> Chip 2 (LEDs)

 El primer byte que se envía termina en el chip 2 (LEDs)
 y el segundo byte en el chip 1 (teclado).

 registro1 (chip 1): bits 3..0 = filas R4..R1 del teclado
 registro2 (chip 2): bits 7..0 = LED8..LED1
*/

byte registro1 = B00001111;   // filas en HIGH
byte registro2 = B00000000;   // LEDs apagados

const byte LED_INICIO         = B00000001;   // LED 1
const byte LED_ESPERANDO_RFID = B00000010;   // LED 2
const byte LED_VALIDANDO_RFID = B00000100;   // LED 3
const byte LED_ESPERANDO_PIN  = B00001000;   // LED 4
const byte LED_VALIDANDO_PIN  = B00010000;   // LED 5
const byte LED_AUTORIZADO     = B00100000;   // LED 6
const byte LED_DENEGADO       = B01000000;   // LED 7
const byte LED_ALARMA         = B10000000;   // LED 8


// ==============================================================
// 4. MAPA DEL KEYPAD
// ==============================================================

char teclas[4][4] = {
  {'A', 'B', 'C', 'D'},   // S4,  S8,  S12, S16
  {'3', '6', '9', '#'},   // S3,  S7,  S11, S15
  {'2', '5', '8', '*'},   // S2,  S6,  S10, S14
  {'1', '4', '7', '0'}    // S1,  S5,  S9,  S13
};


// ==============================================================
// 5. VARIABLES DEL SISTEMA
// ==============================================================

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

int usuarioActual = -1;   // -1 = tarjeta no reconocida


// ==============================================================
// 6. ESTADOS DEL SISTEMA
// ==============================================================

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
// 7. ACTUALIZAR LOS DOS 74HC595
// ==============================================================

void actualizarRegistro()
{
  digitalWrite(LATCH, LOW);

  shiftOut(DATA, CLOCK, MSBFIRST, registro2);   // termina en chip 2 (LEDs)
  shiftOut(DATA, CLOCK, MSBFIRST, registro1);   // termina en chip 1 (teclado)

  digitalWrite(LATCH, HIGH);
}


// ==============================================================
// 8. LCD Y LEDs DE ESTADO
// ==============================================================

void mostrar(const __FlashStringHelper* linea1, const __FlashStringHelper* linea2)
{
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(linea1);
  lcd.setCursor(0, 1);
  lcd.print(linea2);
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
// 9. KEYPAD
// ==============================================================

void seleccionarFila(byte fila)
{
  registro1 |= B00001111;       // Q0-Q3 en HIGH
  bitClear(registro1, fila);    // solo la fila elegida en LOW
  actualizarRegistro();
}

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
        delay(30);   // anti-rebote

        if (digitalRead(columnas[columna]) == LOW)
        {
          char tecla = teclas[fila][columna];

          while (digitalRead(columnas[columna]) == LOW)
          {
            delay(5);   // esperar a que se suelte
          }

          return tecla;
        }
      }
    }
  }

  return 0;
}


// ==============================================================
// 10. BUZZER PASIVO (tone / noTone)
// ==============================================================

// Un pip de 'ms' milisegundos a la frecuencia indicada
void pitar(unsigned int frecuencia, unsigned int ms)
{
  tone(PIN_BUZZER, frecuencia);
  delay(ms);
  noTone(PIN_BUZZER);
}

// Correcto: un solo pip agudo
void sonidoCorrecto()
{
  pitar(FREQ_CORRECTO, 150);
}

// Error: dos pips graves
void sonidoError()
{
  pitar(FREQ_ERROR, 150);
  delay(150);
  pitar(FREQ_ERROR, 150);
}


// ==============================================================
// 11. RFID
// ==============================================================

// Devuelve true si se leyó una tarjeta nueva (UID en uidLeido)
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
// 12. EEPROM: USUARIOS
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

// Borra todos los usuarios: invalida la marca y limpia los registros
void borrarUsuarios()
{
  EEPROM.update(DIR_MAGIC, 0xFF);   // primero la marca, por si se corta la energía

  for (int i = DIR_USUARIOS; i < DIR_USUARIOS + NUM_USUARIOS * TAM_REGISTRO; i++)
  {
    EEPROM.update(i, 0xFF);
  }
}


// ==============================================================
// 13. CAPTURA DEL PIN
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

// true cuando hay 4 dígitos y se presionó '#'
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
  else if (tecla == '*')            // borrar el último dígito
  {
    if (posicionPIN > 0)
    {
      posicionPIN--;
      pinIngresado[posicionPIN] = '\0';
      dibujarPIN();
    }
  }
  else if (tecla == '#')            // confirmar
  {
    if (posicionPIN == 4) return true;
  }

  return false;
}


// ==============================================================
// 14. REGISTRO Y BORRADO DE USUARIOS
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

      for (byte s = 0; s < u; s++)          // evitar tarjetas repetidas
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

    // ---- 3. Guardar en EEPROM ----
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

// Pide confirmación, borra todos los usuarios y pasa al registro
void confirmarBorrado()
{
  mostrar(F("BORRAR USUARIOS?"), F("#=SI    *=NO"));

  while (true)
  {
    char tecla = leerTeclado();

    if (tecla == '#')                  // confirmar
    {
      borrarUsuarios();

      mostrar(F("USUARIOS"), F("BORRADOS"));
      sonidoCorrecto();
      delay(1500);

      registrarUsuarios();             // sin usuarios no hay acceso
      reiniciarSistema();
      return;
    }

    if (tecla == '*')                  // cancelar
    {
      mostrar(F("BORRADO"), F("CANCELADO"));
      delay(1200);

      reiniciarSistema();
      return;
    }
  }
}


// ==============================================================
// 15. SERVO
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
// 16. ALARMA
// ==============================================================

// Sirena (dos tonos alternados) hasta presionar la tecla de apagado (D, S16)
void activarAlarma()
{
  unsigned long ultimoCambio = 0;
  bool tonoAlto = false;

  while (true)
  {
    if (millis() - ultimoCambio >= 300)
    {
      ultimoCambio = millis();
      tonoAlto = !tonoAlto;

      tone(PIN_BUZZER, tonoAlto ? FREQ_ALARMA_ALTA : FREQ_ALARMA_BAJA);
    }

    if (leerTeclado() == TECLA_APAGAR_ALARMA) break;
  }

  noTone(PIN_BUZZER);

  intentosFallidos = 0;
  apagarLEDs();
  reiniciarSistema();
}


// ==============================================================
// 17. REINICIAR CICLO DE AUTENTICACIÓN
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
// 18. SETUP
// ==============================================================

void setup()
{
  Serial.begin(9600);

  lcd.init();
  lcd.backlight();

  pinMode(DATA, OUTPUT);
  pinMode(CLOCK, OUTPUT);
  pinMode(LATCH, OUTPUT);

  for (byte i = 0; i < 4; i++) pinMode(columnas[i], INPUT_PULLUP);

  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_BUZZER, LOW);

  puerta.attach(PIN_SERVO);
  puerta.write(ANGULO_CERRADA);

  SPI.begin();
  rfid.PCD_Init();

  actualizarRegistro();

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

  pitar(FREQ_CORRECTO, 150);

  puerta.write(ANGULO_ABIERTA);
  delay(700);
  puerta.write(ANGULO_CERRADA);
  delay(700);
#endif

  // Si no hay usuarios guardados, se registran
  if (!usuariosRegistrados())
  {
    registrarUsuarios();
  }

  reiniciarSistema();
}


// ==============================================================
// 19. LOOP (MÁQUINA DE ESTADOS)
// ==============================================================

void loop()
{
  switch (estado)
  {
    // ---------------- ESPERANDO RFID ----------------
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
      else if (leerTeclado() == TECLA_BORRAR_USUARIOS)
      {
        confirmarBorrado();          // tecla A: borrar todos los usuarios
      }

      break;


    // ---------------- ESPERANDO PIN ----------------
    case ESPERANDO_PIN:

      if (leerPIN())
      {
        estado = VALIDANDO_PIN;
      }

      break;


    // ---------------- VALIDANDO PIN ----------------
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
          estado = ACCESO_DENEGADO;  // tercer intento -> alarma
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


    // ---------------- ACCESO AUTORIZADO ----------------
    case ACCESO_AUTORIZADO:

      mostrar(F("ACCESO"), F("AUTORIZADO"));

      mostrarEstadoLED(LED_AUTORIZADO);

      sonidoCorrecto();              // 1 pip: PIN correcto
      abrirPuerta();

      delay(1000);

      mostrar(F("PUERTA ABIERTA"), F("C = CERRAR"));

      estado = PUERTA_ABIERTA;

      break;


    // ---------------- ACCESO DENEGADO ----------------
    case ACCESO_DENEGADO:

      mostrarEstadoLED(LED_DENEGADO);

      if (usuarioActual < 0)
      {
        // Tarjeta no registrada
        mostrar(F("TARJETA INVALIDA"), F("ACCESO DENEGADO"));
        sonidoError();               // 2 pips: tarjeta inválida
        delay(1500);

        reiniciarSistema();
      }
      else
      {
        // Tercer intento de PIN fallido
        mostrar(F("ACCESO DENEGADO"), F(""));
        sonidoError();               // 2 pips
        delay(1500);

        mostrar(F("ACCESO BLOQUEADO"), F("ALARMA ACTIVADA"));

        estado = ALARMA;
      }

      break;


    // ---------------- PUERTA ABIERTA ----------------
    case PUERTA_ABIERTA:

      if (leerTeclado() == TECLA_CERRAR_PUERTA)
      {
        cerrarPuerta();
        intentosFallidos = 0;
        reiniciarSistema();
      }

      break;


    // ---------------- ALARMA ----------------
    case ALARMA:

      mostrarEstadoLED(LED_ALARMA);

      activarAlarma();   // bloquea hasta presionar D (S16)

      break;
  }
}