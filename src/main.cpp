#include <Arduino.h>
#include <EEPROM.h>
#include "DHT.h"
#include "BluetoothSerial.h"
#include "ArduinoJson.h"
#include <time.h>
#include <sys/time.h>

#define DHTPIN 4
#define DHTTYPE DHT22
#define RELE_LAMPADA 13
#define RELE_VALV_SOLENOIDE 32

#define RELE_COOLER_EXAUSTAO 18
#define RELE_COOLER_VENTO 19

#define SENSORSOLO A0
//const int moisturePin = A0;

#define EEPROM_SIZE 24

#define SEC_PER_DAY 86400
#define SEC_PER_HOUR 3600
#define SEC_PER_MIN 60

//function prototypes
bool sincronizaApp(unsigned long refreshTime, unsigned long refreshTimeUmidade);
bool atualizaSensorTempHum();
bool atualizaSensorSolo();
String recebeRequisicao(String received_data);
bool leRequisicao(String received_data);
bool controlaRequisicao(String received_data);
void atualizaHora();
void consultaHora();
void sincronizaHoraApp();
void enviaDadosSensores(int celsius, int humi, int humSolo);
bool toogleLed();
void respostaRequisicao(String tipo, String received_data);
void delayIO();
bool verificaCondicaoAcionamentoLed(String dados);
bool gravaProgramacaoLeds();
bool leProgramacaoLeds();
void verificaRega();
bool toogleExaustor();
bool toogleVento();

// end function prototypes

//Variáveis globais

int horaAgora = 0;
//int minutoAgora = 0;
String horasLuz = "";

//char diasDaSemana[7][12] = {"Domingo", "Segunda", "Terca", "Quarta", "Quinta", "Sexta", "Sabado"}; //Dias da semana
bool primeiraExecucao = true;

float umidadeAr;
float celsius;
float fah;
int umidadeSolo;
int umidadeSoloLimite = 80;

BluetoothSerial SerialBT;
StaticJsonDocument<128> jsonRequisicaoApp;
DHT dht(DHTPIN, DHTTYPE);

void setup()
{

  /*
      Inicializa componentes
  */
  EEPROM.begin(EEPROM_SIZE);
  Serial.begin(115200);
  SerialBT.begin("ESP32test"); //Bluetooth device name
  Serial.println("The device started, now you can pair it with bluetooth!");
  dht.begin();

  pinMode(RELE_LAMPADA, OUTPUT);
  digitalWrite(RELE_LAMPADA, HIGH);

  pinMode(RELE_VALV_SOLENOIDE, OUTPUT);
  digitalWrite(RELE_VALV_SOLENOIDE, HIGH);

  pinMode(RELE_COOLER_EXAUSTAO, OUTPUT);
  digitalWrite(RELE_COOLER_EXAUSTAO, LOW);

  pinMode(RELE_COOLER_VENTO, OUTPUT);
  digitalWrite(RELE_COOLER_VENTO, LOW);

  gravaProgramacaoLeds();
  leProgramacaoLeds();
  // atualizaHora();
}

void loop()
{

  //TODO estruturas separadas para rodar coisas com tempos diferentes.
  //Ex: atualiazacao de sensores, atualizacao da hora

  static unsigned long refreshTime = 2000; //tempo para dev (9600) / na vera (2000)
  static unsigned long refreshTimeUmidade = 30000;

  //maestro(); //controla as estruturas de tempo
  //tronco(); controla o fluxo de requisicoes 

  sincronizaApp(refreshTime, refreshTimeUmidade);
}

bool sincronizaApp(unsigned long refreshTime, unsigned long refreshTimeUmidade)
{
  String received_data = "";
  static unsigned long lastRefreshTime = 0;

  if (millis() - lastRefreshTime >= refreshTime)
  {
    lastRefreshTime += refreshTime;
    atualizaSensorTempHum();

    received_data = recebeRequisicao(received_data);
    leRequisicao(received_data);
    controlaRequisicao(received_data);
    enviaDadosSensores(celsius, umidadeAr, umidadeSolo);
    consultaHora();
  }

  if (millis() - lastRefreshTime >= refreshTimeUmidade)
  {
    lastRefreshTime += refreshTimeUmidade;
    verificaRega();
  }

  return true;
}

bool atualizaSensorTempHum()
{
  umidadeAr = dht.readHumidity();
  celsius = dht.readTemperature();
  fah = dht.readTemperature(true);

  if (isnan(umidadeAr) || isnan(celsius) || isnan(fah))
  {
    Serial.println("Failed to read from DHT sensor!");
    return false;
  }
  return true;
}

bool atualizaSensorSolo()
{
  int valorSensorSolo = analogRead(SENSORSOLO);
  umidadeSolo = constrain(map(valorSensorSolo, 1000, 4095, 100, 0), 0, 100);

  Serial.print("Umidade do solo = ");
  Serial.print(umidadeSolo);
  Serial.println("%");
}

String recebeRequisicao(String received_data)
{
  while (SerialBT.available() > 0)
  {
    char c = SerialBT.read();
    received_data += c;
    delay(5);
  }

  received_data.trim();
  delayIO();
  return received_data;
}

bool leRequisicao(String received_data)
{
  jsonRequisicaoApp.clear();
  DeserializationError error = deserializeJson(jsonRequisicaoApp, received_data);
  if (error)
  {
    // Test if parsing succeeds. - Nao apagar
    // Nao apagar !
    // SerialBT.print(F("deSerialBTizeJson() failed: "));
    // SerialBT.println(error.c_str());
    //delay(1000);
    return false;
  }
  //delay(1000);

  return true;
}

bool controlaRequisicao(String received_data)
{
  if (received_data != "")
  {
    Serial.print("Requisicao lida - ");
    Serial.println(received_data);
    delayIO();
  }

  // ver alteracoes para case
  if (jsonRequisicaoApp["tp"] == "dt")
  {
    sincronizaHoraApp();
  };
  if (jsonRequisicaoApp["tp"] == "hl")
  {
    leProgramacaoLeds();
  };
  if (jsonRequisicaoApp["tp"] == "ghl")
  {
    gravaProgramacaoLeds();
  };
  if (jsonRequisicaoApp["tp"] == "xst")
  {
    toogleExaustor();
  };
  if (jsonRequisicaoApp["tp"] == "vnt")
  {
    toogleVento();
  };
  //delay(2000);
}

void consultaHora()
{
  struct timeval tv;
  struct timezone tz;
  struct tm *today;
  int zone;

  gettimeofday(&tv, &tz);

  /* get time details */
  today = localtime(&tv.tv_sec);
  printf("It's %d:%0d:%0d.%d\n",
         today->tm_hour,
         today->tm_min,
         today->tm_sec,
         tv.tv_usec);

  /* set time zone value to hours, not minutes */
  zone = tz.tz_minuteswest / 60;
  /* calculate for zones east of GMT */

  if (zone > 12)
    zone -= 24;
  // printf("Time zone: GMT %+d\n",zone);
  // printf("Daylight Saving Time adjustment: %d\n",tz.tz_dsttime);
}

void sincronizaHoraApp()
{
  Serial.println("SincronizaHoraApp");
  delayIO();
  atualizaHora();
  toogleLed();
}

void atualizaHora()
{

  int ano = jsonRequisicaoApp["Y"];

  struct tm tm;
  //  tm.tm_year = 2017 - 1900;
  tm.tm_year = ano - 1900;
  tm.tm_mon = jsonRequisicaoApp["M"];
  tm.tm_mday = jsonRequisicaoApp["D"];
  tm.tm_hour = jsonRequisicaoApp["h"];
  tm.tm_min = jsonRequisicaoApp["m"];
  tm.tm_sec = jsonRequisicaoApp["s"];
  time_t t = mktime(&tm);
  printf("Setting time: %s", asctime(&tm));
  struct timeval now = {.tv_sec = t};
  settimeofday(&now, NULL);

  horaAgora = jsonRequisicaoApp["h"];
}

void enviaDadosSensores(int celsius, int humiAr, int humSolo)
{
  DynamicJsonDocument doc(124); // ou seria static ??
  doc["tp"] = "sensor";
  doc["tmp"] = celsius;
  doc["hmd"] = humiAr;
  doc["sl"] = humSolo;

//  serializeJson(doc, Serial);
//  Serial.println();
//  delayIO();

  String respostaBluetooth;
  serializeJson(doc, respostaBluetooth);
  SerialBT.println(respostaBluetooth);

  delayIO();
}

bool toogleLed()
{
  bool houveAlteracao = false;
  bool deveLigar = verificaCondicaoAcionamentoLed(horasLuz);

  Serial.println("toogleLed");
  delayIO();

  if (deveLigar)
  {
    Serial.println("Led deve ligar");
   // delay(1000);
    // se leds já estiverem ligados, nao disparar comando novamente. Idem pro else
    if (digitalRead(RELE_LAMPADA) == HIGH)
    {
      digitalWrite(RELE_LAMPADA, LOW);
      houveAlteracao = true;
      respostaRequisicao("TL", "LED-ON"); // tl = toogleLed
      //delayIO();
    }
  }
  else
  {
    Serial.println("Led deve desligar");
   // delay(1000);
    if (digitalRead(RELE_LAMPADA) == LOW)
    {
      digitalWrite(RELE_LAMPADA, HIGH);
      houveAlteracao = true;
      respostaRequisicao("TL", "LED-OFF"); // tl = toogleLed
    //  delayIO();
    }
  }
  delayIO();
  return houveAlteracao;
}

void respostaRequisicao(String tipo, String received_data)
{
  DynamicJsonDocument doc(124);
  doc["tp"] = tipo;
  doc["data"] = received_data;

  serializeJson(doc, Serial);
  Serial.println();

  String respostaBluetooth;
  serializeJson(doc, respostaBluetooth);
  SerialBT.println(respostaBluetooth);

  delayIO();
}

void delayIO()
{
  static int waitingTime = 1000;
  delay(waitingTime);
}

bool verificaCondicaoAcionamentoLed(String dados)
{
  char estadoHoraAtual = dados.charAt(horaAgora);
  if (estadoHoraAtual == '0')
    return false;
  if (estadoHoraAtual == '1')
    return true;
}

bool gravaProgramacaoLeds()
{
  // Também é possível gravar objetos. Ver exemplo site arduino
  /*
 * ClearRELE_VALV_SOLENOIDE
  }
*/

  String requisicao = jsonRequisicaoApp["data"];

  if (primeiraExecucao)
  {
    requisicao = "011100001111111100000000";
    primeiraExecucao = false;
  }

  int eeAddress = 0; //Location we want the data to be put.
  EEPROM.writeString(eeAddress, requisicao);
  EEPROM.commit();

  Serial.println("Gravou na memoria!");
  delay(1000);

  respostaRequisicao("ghl", requisicao);
  leProgramacaoLeds();
}

bool leProgramacaoLeds()
{
  String memoriaLeds;
  int eeAddress = 0; //EEPROM address to start reading from

  memoriaLeds = EEPROM.readString(eeAddress);
 // delay(1000);

  horasLuz = memoriaLeds;
  toogleLed();
  respostaRequisicao("ml", memoriaLeds);

  return true;
}

void verificaRega()
{
  atualizaSensorSolo();

  // para fins de teste
  umidadeSolo = 81;

  int tempoRega = 30;

  if (umidadeSolo < umidadeSoloLimite)
  {
    digitalWrite(RELE_VALV_SOLENOIDE, LOW);
    Serial.println("Rele rega on");

    int contadorTempoRega = 0;

    while (contadorTempoRega < tempoRega)
    {
      Serial.println(contadorTempoRega);
      // Rega por 30 segundos
      contadorTempoRega++;
      delay(1000);
    }

    digitalWrite(RELE_VALV_SOLENOIDE, HIGH);

    Serial.println("Rele rega off");
  }
  else
  {
    Serial.println("Não precisa de rega");
    delayIO();
  }
}

bool toogleExaustor()
{
  if (digitalRead(RELE_COOLER_EXAUSTAO) == HIGH)
  {
    digitalWrite(RELE_COOLER_EXAUSTAO, LOW);
    respostaRequisicao("tc", "XST-OFF"); // tc = toogleCooler
    delayIO();
  }
  else
  {
    digitalWrite(RELE_COOLER_EXAUSTAO, HIGH);
    respostaRequisicao("tc", "XST-ON"); // tc = toogleCooler
    delayIO();
  }
}

bool toogleVento()
{
  if (digitalRead(RELE_COOLER_VENTO) == HIGH)
  {
    digitalWrite(RELE_COOLER_VENTO, LOW);
    respostaRequisicao("TC", "VNT-OFF"); // tc = toogleCooler
    delayIO();
  }
  else
  {
    digitalWrite(RELE_COOLER_VENTO, HIGH);
    respostaRequisicao("TC", "VNT-ON"); // tc = toogleCooler
    delayIO();
  }
}