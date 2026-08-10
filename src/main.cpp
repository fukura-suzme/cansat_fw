#include <Arduino.h>
#include <Adafruit_BMP280.h>
#include <Seeed_FS.h>
#include <SD/Seeed_SD.h>
#include <TFT_eSPI.h>

// When development phase, it should be enabled 
//#define DEV_MODE

#ifdef DEV_MODE 
    #define DEBUG_PRINT(x) Serial.print(x)
    #define DEBUG_PRINTLN(x) Serial.println(x)
#else
    #define DEBUG_PRINT(x) 
    #define DEBUG_PRINTLN(x)
#endif 


// Threshold
#define DESCENT_THRESHOLD_HPA 0.3f
#define PAUSE_COUNT 5
#define LAND_COUNT 60
#define MOVING_AVG_SIZE 10

                             
enum State {
    IDLE,
    ARMED,
    DESCENDING,
    PAUSED,
    LANDED
};

// Global variables
Adafruit_BMP280 bmp;  // I2C
                      
File dataFile;
const char* filename = "data.csv";

State state = IDLE;
int seq = 0;
int run_id = 0;
int stableCount = 0;

float pressureHistory[MOVING_AVG_SIZE] = {0};
int historyIndex = 0;
bool historyFull = false;

float baseline_fixed = 0;

bool lastButtonState = HIGH; // negative logic
                             
TFT_eSPI tft = TFT_eSPI();   // LCD
const char* prevStateName = "IDLE"; // for LCD
float prevPressure = 0;             // for LCD
int prevRunId = 0;                  // for LCD
int prevSeq = 0;                    // for LCD
int prevStableCount = 0;            // for LCD
                         

// Calculates a moving average
float calcMovingAvg() {
    int count = historyFull ? MOVING_AVG_SIZE : historyIndex;
    if (count == 0) return 0;

    float sum = 0;
    for( int i=0; i<count; i++) {
        sum += pressureHistory[i];
    }
    return sum / count;
}

// Accepts a State value and returns the corresponding string
const char* stateName(State s) {
    switch(s) {
        case IDLE:          return "IDLE";
        case ARMED:         return "ARMED";
        case DESCENDING:    return "DESCENDING"; 
        case PAUSED:        return "PAUSED"; 
        case LANDED:        return "LANDED"; 
        default:            return "UNKNOWN";
    }
}

// Check if the data is number
bool isNumber(String str) {
    if (str.length() == 0) return false;
    for (unsigned int i = 0; i< str.length(); i++) {
        if (!isDigit(str[i])) {
            return false;
        }
    }
    return true;
}

// Get next ID from SD card
int getNextID() {
    DEBUG_PRINTLN("getNextID called");

    dataFile = SD.open(filename, FILE_READ);
    
    // If the file does not exist, the ID is 1
    if (!dataFile) {
        DEBUG_PRINTLN("file not found, return 1"); 
        return 1;
    }
    // If the file size is 0, the ID is 1
    unsigned long fileSize = dataFile.size();
    DEBUG_PRINT("fileSize: ");
    DEBUG_PRINTLN(fileSize);
    if (fileSize == 0) {
        dataFile.close();
        DEBUG_PRINTLN("fileSize 0, return 1");
        return 1;
    }

    // Search the start of the line of the last row
    long pos = fileSize - 2;
    while (pos >= 0) {
        dataFile.seek(pos);
        if (dataFile.read() == '\n') {
            break;
        }
        pos--;
    }

    // Move to the start of the line of the last row
    dataFile.seek(pos + 1);

    // Read character until the first ',' and restore the last ID
    String idString = "";
    while (dataFile.available()) {
        char c = dataFile.read();
        if (c == ',' || c == '\r' || c == '\n') {
            break;
        }
        idString += c;
    }
    dataFile.close();

    idString.trim();

    // Check if the idString is number
    if (isNumber(idString)) {
        int lastId = idString.toInt();
        return lastId + 1;
    } else {  // When the csv file have only title row
        return 1;
    }
}



/* *********************************************
 *  Setup
 * ********************************************* */

void setup() {

#ifdef DEV_MODE 
    Serial.begin(115200);
    while (!Serial) delay(100);  // wait for native usb
#endif

    DEBUG_PRINTLN("BMP280 + SD + LCD test start");
    pinMode(WIO_KEY_C, INPUT_PULLUP);   // ButtonA: INPUT Mode 

    // setup LCD
    tft.begin();
    tft.setRotation(3); // landscape
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(2);
    tft.drawString("Welcome", 10, 10);

    // Try to find BMP280(Barometer Sensor) 
    if (!bmp.begin()) {
        DEBUG_PRINTLN("BMP280 not found. Check wiring!");
        while (1) delay(10);
    }
    DEBUG_PRINTLN("BMP280 found!");

    // Try to find SD
    if (!SD.begin(SDCARD_SS_PIN, SDCARD_SPI)) {
        DEBUG_PRINTLN("SD init failed!");
        while (1) delay(10);
    }
    DEBUG_PRINTLN("SD found!");

    if (!SD.exists(filename)) {
        dataFile = SD.open(filename, FILE_APPEND);
        if (dataFile) {
            dataFile.println("run_id,seq,timestamp_ms,pressure_hpa,baseline_hpa,state");
            dataFile.close();

            DEBUG_PRINTLN("Header written.");
        } else {
            DEBUG_PRINTLN("Failed to open file!");
        }
    } else {

        DEBUG_PRINTLN("Appending to existing file.");
    }

    run_id = getNextID();

    tft.setTextColor(TFT_BLACK);
    tft.setTextSize(2);
    tft.drawString("Welcome", 10, 10);
}


/* *********************************************
 *  Loop 
 * ********************************************* */

void loop() {
    //float temp = bmp.readTemperature();
    float pressure = bmp.readPressure() / 100.0F;
    unsigned long ts = millis();

    // update baseline
    pressureHistory[historyIndex] = pressure;
    historyIndex++;
    if (historyIndex >= MOVING_AVG_SIZE) {
        historyIndex = 0;
        historyFull = true;
    }
    float baseline = calcMovingAvg();  

    // Skip the 0th iteration right after observation
    if (historyIndex == 0 && !historyFull) {
        return;
    }
    
    // Button operation
    bool currentButtonState = digitalRead(WIO_KEY_C);
    if (lastButtonState == HIGH && currentButtonState == LOW) {
        switch(state) {
            case IDLE:
                state = ARMED;
                run_id = getNextID();
                seq = 0;
                stableCount = 0;
                baseline_fixed = pressure; 
                break;
            case ARMED:
                state = IDLE;
                break;
            case DESCENDING:
                state = IDLE;
                break;
            case PAUSED:
                state = IDLE;
                break;
            case LANDED:
                break;
        }
        delay(20); // Avoid chattering
    }                

    lastButtonState = currentButtonState;
                
    
    float diff = pressure - baseline;

    switch(state) {
        case ARMED:
            // to account for cases where the change is gradual,
            // the determination is based on a fixed value(only at the start)
            if (pressure - baseline_fixed > DESCENT_THRESHOLD_HPA) {
                state = DESCENDING;
                stableCount = 0;
            }
            break;

        case DESCENDING:
            if (abs(diff) < 0.05f) { 
                stableCount++;
                if (stableCount == PAUSE_COUNT) {
                    state = PAUSED;
                    stableCount = 0;
                }
            } 
            else {
                stableCount = 0;
            }
            break;

        case PAUSED:
            if (diff > DESCENT_THRESHOLD_HPA) {
                state = DESCENDING;
                stableCount = 0;
            }
            else {
                stableCount++;
                if (stableCount >= LAND_COUNT){
                    state = LANDED;
                    //stableCount = 0; 
                }
            }
            break;
        case IDLE:
            break;
        case LANDED:
            tft.fillScreen(TFT_BLACK);
            tft.setTextColor(TFT_WHITE);
            tft.drawString("*** LANDED ***", 10, 100);
            delay(5000);  // wait for 5sec
            state = IDLE;              
            break;
    }


    // Serial output (Dev_MODE)                
    DEBUG_PRINT(run_id);
    DEBUG_PRINT(", ");
    DEBUG_PRINT(seq);
    DEBUG_PRINT(", ");
    DEBUG_PRINT(ts);
    DEBUG_PRINT(", ");
    DEBUG_PRINT(pressure);
    DEBUG_PRINT(" hPa, ");
    DEBUG_PRINT(baseline);
    DEBUG_PRINT(" hPa, ");
    DEBUG_PRINTLN(stateName(state));

    // SD card
    if (state == ARMED || state == DESCENDING || state == PAUSED || state == LANDED) {
        dataFile = SD.open(filename, FILE_APPEND);
        if (dataFile) {
            dataFile.print(run_id);
            dataFile.print(",");
            dataFile.print(seq);
            dataFile.print(",");
            dataFile.print(ts);
            dataFile.print(",");
            dataFile.print(pressure);
            dataFile.print(",");
            dataFile.print(baseline);
            dataFile.print(",");
            dataFile.println(stateName(state));

            dataFile.close();

            seq++;
            
        } else {

            DEBUG_PRINTLN("Failed to open file!");
        }
    }

    // LCD Update
    tft.setTextColor(TFT_BLACK); 
    tft.drawString("State: " + String(prevStateName), 10, 10);
    tft.drawString("Pres: " + String(prevPressure), 10, 40);
    tft.drawString("run_id:" + String(prevRunId) + " seq:" + String(prevSeq), 10, 70);
    tft.drawString("stable:" + String(prevStableCount), 10, 100);

    // Display on LCD
    tft.setTextColor(TFT_WHITE);
    tft.drawString("State: " + String(stateName(state)), 10, 10);
    tft.drawString("Pres: " + String(pressure), 10, 40);
    tft.drawString("run_id:" + String(run_id) + " seq:" + String(seq), 10, 70);
    tft.drawString("stable:" + String(stableCount), 10, 100);

    // update the LCD previous data 
    prevStateName = stateName(state);
    prevPressure = pressure;
    prevRunId = run_id;
    prevSeq = seq;
    prevStableCount = stableCount;
      
    delay(1000);   // wait for 1000ms = 1s

}
