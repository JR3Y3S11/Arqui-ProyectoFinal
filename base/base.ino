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

 D2  -> Buzzer pasivo
 D3  -> Botón de diagnóstico
 D4  -> Servo

 D5  -> Libre

 D6  -> RFID RST

 D7  -> 74HC595 DATA
 D8  -> 74HC595 CLOCK
 D9  -> 74HC595 LATCH

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
// 3. REGISTRO DE DESPLAZAMIENTO 74HC595
// ==============================================================

/*

                  74HC595

             Q0 ─────► Keypad R1
             Q1 ─────► Keypad R2
             Q2 ─────► Keypad R3
             Q3 ─────► Keypad R4

             Q4 ─────► LED verde
             Q5 ─────► LED rojo
             Q6 ─────► LED amarillo
             Q7 ─────► LED azul


 Bits:

       7   6   5   4   3   2   1   0
     ┌───┬───┬───┬───┬───┬───┬───┬───┐
     │AZ │AM │RO │VE │R4 │R3 │R2 │R1 │
     └───┴───┴───┴───┴───┴───┴───┴───┘

*/

// Inicialmente las cuatro filas están HIGH.

byte registro = B00001111;


// ==============================================================
// 4. MAPA DEL KEYPAD
// ==============================================================

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

// Usuario cuya tarjeta fue reconocida (-1 = ninguno); lo usará validarPIN()
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
// 7. ACTUALIZAR 74HC595
// ==============================================================

void actualizarRegistro()
{
  digitalWrite(LATCH, LOW);

  shiftOut(DATA, CLOCK, MSBFIRST, registro);

  digitalWrite(LATCH, HIGH);
}


// ==============================================================
// 8. CONTROL DE LEDs
// ==============================================================

void apagarLEDs()
{
  /*
   TODO - REGISTROS / OPERACIONES BINARIAS

   Los LEDs se encuentran conectados a Q4-Q7.

   Deben apagarse SIN modificar Q0-Q3,
   ya que esos bits controlan las filas
   del teclado.

   Modificar la variable:

       registro

   y posteriormente actualizar el 74HC595.
  */
}


void encenderLED(byte numero)
{
  /*
   TODO - REGISTROS / OPERACIONES BINARIAS

   Encender solamente uno de los LEDs.

       numero = 0 -> Q4 -> Verde
       numero = 1 -> Q5 -> Rojo
       numero = 2 -> Q6 -> Amarillo
       numero = 3 -> Q7 -> Azul

   IMPORTANTE:

   No modificar Q0-Q3.
  */
}


// ==============================================================
// 9. SELECCIÓN DE FILA DEL KEYPAD
// ==============================================================

void seleccionarFila(byte fila)
{
  /*
   TODO - LABORATORIO KEYPAD

   Para detectar una tecla:

   1. Colocar Q0-Q3 en HIGH.

   2. Colocar únicamente la fila que se desea
      consultar en LOW.

   3. Enviar el nuevo valor al 74HC595.

   Pista:

       bitClear()
  */
}


// ==============================================================
// 10. LECTURA DEL KEYPAD
// ==============================================================

char leerTeclado()
{
  /*
   TODO - LABORATORIO KEYPAD

   Implementar el barrido de la matriz 4x4.

   El algoritmo deberá:

   1. Seleccionar una fila.
   2. Leer las cuatro columnas.
   3. Determinar qué tecla fue presionada.
   4. Manejar el rebote de la tecla.
   5. Retornar el carácter correspondiente.

   Si ninguna tecla fue presionada:

       return 0;
  */

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

bool leerPIN()
{
  /*
   TODO - LABORATORIO KEYPAD / INTEGRACIÓN

   Utilizar:

       leerTeclado()

   para capturar un PIN de cuatro números.

   Cada número deberá almacenarse en:

       pinIngresado

   Cuando existan cuatro números:

       return true;

   Mientras el usuario continúe escribiendo:

       return false;


   SUGERENCIA:

   En el LCD no mostrar los números.

   Mostrar:

       *
       **
       ***
       ****
  */

  return false;
}


// ==============================================================
// 14. VALIDACIÓN DEL PIN
// ==============================================================

bool validarPIN()
{
  /*
   TODO - LABORATORIO EEPROM

   El PIN autorizado se encuentra almacenado
   en la memoria EEPROM.

   El algoritmo deberá:

   1. Leer el PIN almacenado.
   2. Compararlo con pinIngresado.
   3. Determinar si son iguales.

   Retornar:

       true  -> PIN correcto
       false -> PIN incorrecto
  */

  return false;
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

   Utilizar el buzzer pasivo para generar
   un sonido que indique acceso autorizado.
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

   Diseñar el comportamiento de la alarma.

   Puede utilizar:

       - LED rojo
       - Buzzer
       - LCD

   La implementación queda a criterio
   de cada equipo.
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
  lcd.print("SISTEMA URL");

  lcd.setCursor(0, 1);
  lcd.print("ACERQUE TARJETA");


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
  lcd.print("SISTEMA URL");

  lcd.setCursor(0, 1);
  lcd.print("INICIANDO...");

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
        if (tarjetaAutorizada(UIDLeido))
        {
          lcd.clear();

          lcd.setCursor(0, 0);
          lcd.print("TARJETA VALIDA");

          lcd.setCursor(0, 1);
          lcd.print("INGRESE PIN");

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

      if (validarPIN())
      {
        estado = ACCESO_AUTORIZADO;
      }
      else
      {
        intentosFallidos++;

        estado = ACCESO_DENEGADO;
      }

      break;


    // ========================================================
    // ESTADO 3 - ACCESO AUTORIZADO
    // ========================================================

    case ACCESO_AUTORIZADO:

      lcd.clear();

      lcd.setCursor(0, 0);
      lcd.print("ACCESO");

      lcd.setCursor(0, 1);
      lcd.print("AUTORIZADO");


      encenderLED(0);

      sonidoCorrecto();

      abrirPuerta();


      estado = PUERTA_ABIERTA;

      break;


    // ========================================================
    // ESTADO 4 - ACCESO DENEGADO
    // ========================================================

    case ACCESO_DENEGADO:

      lcd.clear();

      lcd.setCursor(0, 0);
      lcd.print("ACCESO");

      lcd.setCursor(0, 1);
      lcd.print("DENEGADO");


      encenderLED(1);

      sonidoError();


      delay(1500);


      if (intentosFallidos >= 3)
      {
        estado = ALARMA;
      }
      else
      {
        apagarLEDs();

        reiniciarSistema();
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

      lcd.clear();

      lcd.setCursor(0, 0);
      lcd.print("!!! ALARMA !!!");


      activarAlarma();

      break;
  }
}