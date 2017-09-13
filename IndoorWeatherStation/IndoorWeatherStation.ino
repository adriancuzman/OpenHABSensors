#include <Homie.h>
#include <DHT.h>
#include <SDS011.h>

#define FW_NAME "indoor-weather-station"
#define FW_VERSION "1.0.2"

#define DHTPIN            D3         // Pin which is connected to the DHT sensor.

#define DHTTYPE           DHT22     // DHT 22 (AM2302)

DHT dht(DHTPIN, DHTTYPE);

/* Magic sequence for Autodetectable Binary Upload */
const char *__FLAGGED_FW_NAME = "\xbf\x84\xe4\x13\x54" FW_NAME "\x93\x44\x6b\xa7\x75";
const char *__FLAGGED_FW_VERSION = "\x6a\x3f\x3e\x0e\xe1" FW_VERSION "\xb0\x30\x48\xd4\x1a";
/* End of magic sequence for Autodetectable Binary Upload */

// in seconds
const int DHT22_READ_INTERVAL = 60;
unsigned long lastDHT22Reading = 0;

// in seconds
const int SDS_sleepTime = 600;
const int SDS_wormupTime = 15;

// connect D1 to SDS011 tx pin
const int SDS011_tx_pin = D1;

// connect D2 to SDS011 rx pin
const int SDS011_rx_pin = D2;

unsigned long timeNow = 0;

unsigned long timeLast_SDS = 0;

// sds sleep status
int sds_sleeping = 1;

int nrOfSamplesToRead = 15;
int maxReedAttempts = 100;

float p10, p25;
int error;

SDS011 my_sds;

HomieNode dht22Node("dht22", "temperature+humidity");
HomieNode sds011Node("sds011", "pm10+pm2.5");

void setupHandler() {
}

void loopHandler() {
  loopDHT22();
  loopSDS011();
}

void loopDHT22() {
  if (millis() - lastDHT22Reading >= DHT22_READ_INTERVAL * 1000UL || lastDHT22Reading == 0) {
    // Reading temperature or humidity takes about 250 milliseconds!
    // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
    float humidity = dht.readHumidity();
    // Read temperature as Celsius (the default)
    float temperature = dht.readTemperature();
    if (isnan(humidity) || isnan(temperature)) {
      Serial.println("Failed to read from DHT sensor!");
    }
    else {
      Serial.print("Temperature: ");
      Serial.print(temperature);
      Serial.println(" °C");
      if (!Homie.setNodeProperty(dht22Node, "temperature", String(temperature), true)) {
        Serial.println("Temperature sending failed");
      }

      Serial.print("Humidity: ");
      Serial.print(humidity);
      Serial.println("%");
      if (!Homie.setNodeProperty(dht22Node, "humidity", String(humidity), true)) {
        Serial.println("Humidity sending failed");
      }

      // Compute heat index in Celsius (isFahreheit = false)
      float hic = dht.computeHeatIndex(temperature, humidity, false);

      Serial.print("HeatIndex: ");
      Serial.print(hic);
      Serial.println(" °C");
      if (!Homie.setNodeProperty(dht22Node, "heatIndex", String(hic), true)) {
        Serial.println("HeatIndex sending failed");
      }
    }
    lastDHT22Reading = millis();
  }
}


void loopSDS011() {
  timeNow = millis() / 1000; // the number of milliseconds that have passed since boot

  unsigned long seconds = timeNow - timeLast_SDS;

  if (seconds > SDS_sleepTime & sds_sleeping == 1) {
    Serial.println("SDS Wakeup & worming up..");
    my_sds.wakeup();
    sds_sleeping = 0;
  }

  if (seconds > (SDS_sleepTime + SDS_wormupTime)) {
    readSDSValues();
    Serial.println("SDS Sleep");
    my_sds.sleep();

    timeLast_SDS = timeNow;
    sds_sleeping = 1;
  }
}

void readSDSValues() {
  Serial.println("---- reading data started --- ");
  int readCount = 0;
  int readAttempts = 0;
  float p25Agregator = 0;
  float p10Agregator = 0;
  float p25Temp = 0;
  float p10Temp = 0;
  do {
    error = my_sds.read(&p25Temp, &p10Temp);
    readAttempts++;
    if (! error) {
      p25Agregator += p25Temp;
      p10Agregator += p10Temp;
      readCount++;
      Serial.println("---- Reading count: " + String(readCount) + "-----");
      Serial.println("P2.5: " + String(p25Temp));
      Serial.println("P10:  " + String(p10Temp));
    }
    delay(100);
  }
  while (readCount < nrOfSamplesToRead && readAttempts < maxReedAttempts);
  p25 = p25Agregator / readCount;
  p10 = p10Agregator / readCount;

  Serial.println("---- Average readings:  -----");
  Serial.println("P2.5: " + String(p25));
  Serial.println("P10:  " + String(p10));
  if (!Homie.setNodeProperty(sds011Node, "pm10", String(p10), true)) {
    Serial.println("sds011 pm10 sending failed");
  }
  if (!Homie.setNodeProperty(sds011Node, "pm2.5", String(p25), true)) {
    Serial.println("sds011 pm10 sending failed");
  }
}

void setup() {
  Homie.setFirmware(FW_NAME, FW_VERSION);
  Homie.registerNode(dht22Node);
  Homie.registerNode(sds011Node);
  Homie.setSetupFunction(setupHandler);
  Homie.setLoopFunction(loopHandler);
  Homie.setup();

  setup_DHT22();
  setup_SDS();
}


void setup_SDS() {
  Serial.println("---- sds setup --- ");
  my_sds.begin(SDS011_tx_pin, SDS011_rx_pin);
  my_sds.sleep();
  sds_sleeping = 1;
  timeLast_SDS =  millis() / 1000;
}

void setup_DHT22() {
  dht.begin();
}

void loop() {
  Homie.loop();
}
