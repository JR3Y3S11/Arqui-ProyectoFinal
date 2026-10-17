/*
 ==============================================================
             ARQUITECTURA DEL COMPUTADOR
        PROYECTO: SISTEMA DE CONTROL DE ACCESO

                 PROGRAMA BASE
 ==============================================================

 Este programa contiene la arquitectura general del proyecto.

 El código está INCOMPLETO intencionalmente.

 Durante los siguientes laboratorios se implementarán
 progresivamente las funciones marcadas como:

                         TODO

 Al finalizar los laboratorios, este programa se habrá
 convertido en el sistema completo de control de acceso.
 ==============================================================
*/


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
 ARDUINO UNO
 --------------------------------------------------------------

 D0  -> Serial RX
 D1  -> Serial TX

 D2  -> Buzzer
 D3  -> Botón de diagnóstico
 D4  -> Servo

 D5  -> Libre

 D6  -> RFID RST

 D7  -> 74HC595 DATA   (compartido por los dos 74HC595)
 D8  -> 74HC595 CLOCK  (compartido por los dos 74HC595)
 D9  -> 74HC595 LATCH  (compartido por los dos 74HC595)

 D10 -> RFID SDA / SS
 D11 -> RFID MOSI
 D12 -> RFID MISO
 D13 -> RFID SCK

 A0  -> Keypad C1
 A1  -> Keypad C2
 A2  -> Keypad C3
 A3  -> Keypad C4

 A4  -> LCD SDA
 A5  -> LCD SCL
*/


const byte PIN_BUZZER = 2;
const byte PIN_DIAG   = 3;
const byte PIN_SERVO  = 4;

const byte RFID_RST = 6;

const byte DATA  = 7;
const byte CLOCK = 8;
const byte LATCH = 9;

const byte RFID_SS = 10;

const byte columnas[4] = {
  A0, A1, A2, A3
};


// ==============================================================
// 2. DISPOSITIVOS
// ==============================================================

LiquidCrystal_I2C lcd(0x27, 16, 2);

Servo puerta;

MFRC522 rfid(RFID_SS, RFID_RST);


// ==============================================================
// 3. REGISTROS DE DESPLAZAMIENTO 74HC595 (ENCADENADOS)
// ==============================================================

/*
 Dos 74HC595 en cadena: el pin Q7' (pin 9) del primero se
 conecta al pin DS (pin 14) del segundo. Ambos comparten
 CLOCK y LATCH.

 REGISTRO 1 (primer 74HC595) - Teclado
 ------------------------------------------------------
       7   6   5   4   3   2   1   0
     ┌───┬───┬───┬───┬───┬───┬───┬───┐
     │ - │ - │ - │ - │R4 │R3 │R2 │R1 │
     └───┴───┴───┴───┴───┴───┴───┴───┘

 REGISTRO 2 (segundo 74HC595) - LEDs de estado
 ------------------------------------------------------
       7   6   5   4   3   2   1   0
     ┌───┬───┬───┬───┬───┬───┬───┬───┐
     │L8 │L7 │L6 │L5 │L4 │L3 │L2 │L1 │
     └───┴───┴───┴───┴───┴───┴───┴───┘
*/

// Inicialmente las cuatro filas del teclado están HIGH.
byte registro1 = B00001111;

// Inicialmente todos los LEDs apagados.
byte registro2 = B00000000;


// ==============================================================
// 3.1 VALORES BINARIOS DE LOS ESTADOS (LEDs)
// ==============================================================

/*
 Cada estado del sistema se representa encendiendo
 un único LED.
*/

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

/*
 Teclas con función especial:

   0-9 -> dígitos del PIN
   *   -> borrar el último dígito
   #   -> confirmar el PIN
*/

char teclas[4][4] = {

  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}

};


// ==============================================================
// 5. VARIABLES DEL SISTEMA
// ==============================================================

// UID obtenido del RC522

String UIDLeido = "";


// PIN introducido mediante el keypad

char pinIngresado[5] = "";

byte posicionPIN = 0;


// Número de intentos incorrectos

byte intentosFallidos = 0;


// Usuarios en EEPROM: [UID 4 bytes][PIN 4 caracteres] x NUM_USUARIOS

const byte NUM_USUARIOS = 2;
const byte DIR_MAGIC = 0;        // marca para saber si la EEPROM ya fue inicializada
const byte VALOR_MAGIC = 0x42;
const byte DIR_USUARIOS = 1;
const byte TAM_REGISTRO = 8;


// Usuario cuya tarjeta fue reconocida (-1 = ninguno)

int usuarioActual = -1;


// ==============================================================
// 6. ESTADOS DEL SISTEMA
// ==============================================================

/*
 La CPU solamente puede encontrarse en uno de estos
 estados en un momento determinado.

 El estado actual determina qué operación realizará
 el programa.
*/

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


// ==============================================================
// 7. ACTUALIZAR LOS DOS 74HC595
// ==============================================================

void actualizarRegistro()
{
  digitalWrite(LATCH, LOW);

  // El primer byte enviado recorre toda la cadena
  // y termina en el SEGUNDO 74HC595 (LEDs).
  shiftOut(DATA, CLOCK, MSBFIRST, registro2);

  // El segundo byte queda en el PRIMER 74HC595 (teclado).
  shiftOut(DATA, CLOCK, MSBFIRST, registro1);

  digitalWrite(LATCH, HIGH);
}


// ==============================================================
// 8. CONTROL DE LEDs DE ESTADO
// ==============================================================

// Envía al segundo 74HC595 el valor binario del estado actual.
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
// 9. SELECCIÓN DE FILA DEL KEYPAD
// ==============================================================

void seleccionarFila(byte fila)
{
  // Q0-Q3 en HIGH
  registro1 |= B00001111;

  // Solo la fila seleccionada en LOW
  bitClear(registro1, fila);

  actualizarRegistro();
}


// ==============================================================
// 10. LECTURA DEL KEYPAD
// ==============================================================

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

          // Esperar a que se suelte la tecla
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


// ==============================================================
// 11. LECTURA DEL RFID
// ==============================================================

void cargarUsuariosIniciales()
{
  if (EEPROM.read(DIR_MAGIC) == VALOR_MAGIC) return;   // ya estaban guardados

  // CAMBIA estos UID por los de tus tarjetas (los ves en el monitor serial)
  const byte uids[NUM_USUARIOS][4] = {
    {0xDE, 0xAD, 0xBE, 0xEF},
    {0x12, 0x34, 0x56, 0x78}
  };

  const char pins[NUM_USUARIOS][5] = { "1234", "4321" };

  for (byte u = 0; u < NUM_USUARIOS; u++)
  {
    int base = DIR_USUARIOS + u * TAM_REGISTRO;

    for (byte i = 0; i < 4; i++) EEPROM.update(base + i, uids[u][i]);
    for (byte i = 0; i < 4; i++) EEPROM.update(base + 4 + i, pins[u][i]);
  }

  EEPROM.update(DIR_MAGIC, VALOR_MAGIC);
}


// Devuelve el UID del usuario u (leído de EEPROM) en formato "A1B2C3D4"
String uidDeUsuario(byte u)
{
  String uid = "";
  int base = DIR_USUARIOS + u * TAM_REGISTRO;

  for (byte i = 0; i < 4; i++)
  {
    byte b = EEPROM.read(base + i);

    if (b < 0x10) uid += "0";

    uid += String(b, HEX);
  }

  uid.toUpperCase();

  return uid;
}


String leerTarjeta()
{
  if (!rfid.PICC_IsNewCardPresent()) return "";
  if (!rfid.PICC_ReadCardSerial())   return "";

  String uid = "";

  for (byte i = 0; i < rfid.uid.size; i++)
  {
    if (rfid.uid.uidByte[i] < 0x10) uid += "0";

    uid += String(rfid.uid.uidByte[i], HEX);
  }

  uid.toUpperCase();

  rfid.PICC_HaltA();        // detener la tarjeta para no leerla en bucle
  rfid.PCD_StopCrypto1();

  Serial.print("UID: ");
  Serial.println(uid);

  return uid;
}


// ==============================================================
// 12. VALIDACIÓN DE TARJETA
// ==============================================================

bool tarjetaAutorizada(String uid)
{
  for (byte u = 0; u < NUM_USUARIOS; u++)
  {
    if (uid == uidDeUsuario(u))
    {
      usuarioActual = u;
      return true;
    }
  }

  usuarioActual = -1;

  return false;
}


// ==============================================================
// 13. CAPTURA DEL PIN
// ==============================================================

// Línea 2 del LCD: asteriscos por cada dígito y guiones por los faltantes
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
  lcd.print("INGRESE SU PIN");

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
  else if (tecla == '*')            // borrar
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
// 14. VALIDACIÓN DEL PIN
// ==============================================================

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
// 15. ABRIR PUERTA
// ==============================================================

void abrirPuerta()
{
  /*
   TODO - LABORATORIO SERVO

   Mover el servo a la posición correspondiente
   a la puerta abierta.
  */
}


// ==============================================================
// 16. CERRAR PUERTA
// ==============================================================

void cerrarPuerta()
{
  /*
   TODO - LABORATORIO SERVO

   Mover el servo a la posición correspondiente
   a la puerta cerrada.
  */
}


// ==============================================================
// 17. SONIDO DE ACCESO CORRECTO
// ==============================================================

void sonidoCorrecto()
{
  /*
   TODO - LABORATORIO BUZZER

   Generar un sonido que indique acceso autorizado.
  */
}


// ==============================================================
// 18. SONIDO DE ERROR
// ==============================================================

void sonidoError()
{
  /*
   TODO - LABORATORIO BUZZER

   Generar un sonido diferente que permita
   reconocer un acceso incorrecto.
  */
}


// ==============================================================
// 19. ALARMA
// ==============================================================

void activarAlarma()
{
  /*
   TODO - PROYECTO FINAL

   Buzzer activo encendido hasta presionar la tecla
   que apaga la alarma. Después, reiniciarSistema().
  */
}


// ==============================================================
// 20. REINICIAR CICLO DE AUTENTICACIÓN
// ==============================================================

void reiniciarSistema()
{
  UIDLeido = "";

  usuarioActual = -1;

  posicionPIN = 0;

  pinIngresado[0] = '\0';


  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("CONTROL ACCESO");

  lcd.setCursor(0, 1);
  lcd.print("PRESENTE TARJETA");


  mostrarEstadoLED(LED_ESPERANDO_RFID);

  estado = ESPERANDO_RFID;
}


// ==============================================================
// 21. SETUP
// ==============================================================

void setup()
{

  // ----------------------------------------------------------
  // SERIAL
  // ----------------------------------------------------------

  Serial.begin(9600);


  // ----------------------------------------------------------
  // LCD
  // ----------------------------------------------------------

  lcd.init();

  lcd.backlight();


  // ----------------------------------------------------------
  // 74HC595
  // ----------------------------------------------------------

  pinMode(DATA, OUTPUT);
  pinMode(CLOCK, OUTPUT);
  pinMode(LATCH, OUTPUT);

  actualizarRegistro();


  // ----------------------------------------------------------
  // KEYPAD
  // ----------------------------------------------------------

  for (byte i = 0; i < 4; i++)
  {
    pinMode(columnas[i], INPUT_PULLUP);
  }


  // ----------------------------------------------------------
  // BUZZER
  // ----------------------------------------------------------

  pinMode(PIN_BUZZER, OUTPUT);


  // ----------------------------------------------------------
  // BOTÓN DIAGNÓSTICO
  // ----------------------------------------------------------

  pinMode(PIN_DIAG, INPUT_PULLUP);


  // ----------------------------------------------------------
  // SERVO
  // ----------------------------------------------------------

  puerta.attach(PIN_SERVO);


  // ----------------------------------------------------------
  // RFID
  // ----------------------------------------------------------

  SPI.begin();

  rfid.PCD_Init();


  // ----------------------------------------------------------
  // INICIO
  // ----------------------------------------------------------

  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("SISTEMA ACCESO");

  lcd.setCursor(0, 1);
  lcd.print("INICIANDO...");

  mostrarEstadoLED(LED_INICIO);

  delay(1500);

  cargarUsuariosIniciales();

  reiniciarSistema();
}


// ==============================================================
// 22. LOOP
// ==============================================================

void loop()
{

  /*
   ============================================================
                       UNIDAD DE CONTROL
   ============================================================

   La variable "estado" representa el estado actual
   del sistema.

   Dependiendo de ese estado, la CPU ejecutará una
   operación diferente.
  */


  switch (estado)
  {

    // ========================================================
    // ESTADO 0 - ESPERANDO RFID
    // ========================================================

    case ESPERANDO_RFID:

      UIDLeido = leerTarjeta();

      if (UIDLeido != "")
      {
        mostrarEstadoLED(LED_VALIDANDO_RFID);

        delay(300);   // para que el LED 3 alcance a verse

        if (tarjetaAutorizada(UIDLeido))
        {
          intentosFallidos = 0;      // nuevo proceso de autenticación

          mostrarPantallaPIN();

          mostrarEstadoLED(LED_ESPERANDO_PIN);

          estado = ESPERANDO_PIN;
        }
        else
        {
          estado = ACCESO_DENEGADO;
        }
      }

      break;


    // ========================================================
    // ESTADO 1 - ESPERANDO PIN
    // ========================================================

    case ESPERANDO_PIN:

      if (leerPIN())
      {
        estado = VALIDANDO_PIN;
      }

      break;


    // ========================================================
    // ESTADO 2 - VALIDANDO PIN
    // ========================================================

    case VALIDANDO_PIN:

      mostrarEstadoLED(LED_VALIDANDO_PIN);

      delay(300);   // para que el LED 5 alcance a verse

      if (validarPIN())
      {
        estado = ACCESO_AUTORIZADO;
      }
      else
      {
        intentosFallidos++;

        if (intentosFallidos >= 3)
        {
          // Tercer intento: ACCESO DENEGADO y luego ALARMA
          estado = ACCESO_DENEGADO;
        }
        else
        {
          // Primer o segundo intento: permitir reintento
          lcd.clear();

          lcd.setCursor(0, 0);
          lcd.print("PIN INCORRECTO");

          lcd.setCursor(0, 1);
          lcd.print("INTENTO ");
          lcd.print(intentosFallidos);
          lcd.print("/3");

          mostrarEstadoLED(LED_DENEGADO);

          sonidoError();

          delay(1500);

          posicionPIN = 0;
          pinIngresado[0] = '\0';

          mostrarPantallaPIN();

          mostrarEstadoLED(LED_ESPERANDO_PIN);

          estado = ESPERANDO_PIN;
        }
      }

      break;


    // ========================================================
    // ESTADO 3 - ACCESO AUTORIZADO
    // ========================================================

    case ACCESO_AUTORIZADO:

      // "ACCESO AUTORIZADO" (17 caracteres) no cabe en una
      // línea de 16, por eso se divide en dos.

      lcd.clear();

      lcd.setCursor(0, 0);
      lcd.print("ACCESO");

      lcd.setCursor(0, 1);
      lcd.print("AUTORIZADO");


      mostrarEstadoLED(LED_AUTORIZADO);

      sonidoCorrecto();

      abrirPuerta();


      estado = PUERTA_ABIERTA;

      break;


    // ========================================================
    // ESTADO 4 - ACCESO DENEGADO
    // ========================================================

    case ACCESO_DENEGADO:

      mostrarEstadoLED(LED_DENEGADO);

      if (usuarioActual < 0)
      {
        // Tarjeta no registrada

        lcd.clear();

        lcd.setCursor(0, 0);
        lcd.print("TARJETA INVALIDA");

        lcd.setCursor(0, 1);
        lcd.print("ACCESO DENEGADO");

        sonidoError();

        delay(1500);

        apagarLEDs();

        reiniciarSistema();
      }
      else
      {
        // Tercer intento de PIN fallido

        lcd.clear();

        lcd.setCursor(0, 0);
        lcd.print("ACCESO DENEGADO");

        sonidoError();

        delay(1500);

        lcd.clear();

        lcd.setCursor(0, 0);
        lcd.print("ACCESO BLOQUEADO");

        lcd.setCursor(0, 1);
        lcd.print("ALARMA ACTIVADA");

        estado = ALARMA;
      }

      break;


    // ========================================================
    // ESTADO 5 - PUERTA ABIERTA
    // ========================================================

    case PUERTA_ABIERTA:

      lcd.clear();

      lcd.setCursor(0, 0);
      lcd.print("PUERTA ABIERTA");


      delay(3000);


      cerrarPuerta();

      apagarLEDs();

      intentosFallidos = 0;


      reiniciarSistema();

      break;


    // ========================================================
    // ESTADO 6 - ALARMA
    // ========================================================

    case ALARMA:

      mostrarEstadoLED(LED_ALARMA);

      activarAlarma();

      break;
  }
}