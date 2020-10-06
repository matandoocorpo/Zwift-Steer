/*
    Zwift Steering Simulator
    Takes ADC reading from pin 32 and maps it to an angle from -40 to 40
    Then transmits it to Zwift via BLE
    
    Inspired in samples by Kolban (ESP32 and BLE Arduino) & Peter Everett
    Thanks to Keith Wakeham's for his protocol explanation https://www.youtube.com/watch?v=BPVFjz5zD4g
    Thanks to fiveohhh for the demo code https://github.com/fiveohhh/zwift-steerer/
    
    Written on Arduino IDE 1.8.13
    
    Tested using Zwift on iPad, iPhone, MacBook and iMac, should work on other platforms
    
    v0.1 Oct 2020 matandoocorpo / EA1NK
    
    Licensed under GNU GPL-3

*/

//Libraries

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

//BLE Definitions
#define STEERING_DEVICE_UUID "347b0001-7635-408b-8918-8ff3949ce592"
#define STEERING_ANGLE_CHAR_UUID "347b0030-7635-408b-8918-8ff3949ce592" //notify
#define STEERING_RX_CHAR_UUID "347b0031-7635-408b-8918-8ff3949ce592"    //write
#define STEERING_TX_CHAR_UUID "347b0032-7635-408b-8918-8ff3949ce592"    //indicate

#define POT 32 // Joystick Xaxis to GPIO32

bool deviceConnected = false;
bool oldDeviceConnected = false;
bool auth = false;

float angle = 20;
float angle_deviation = 0; //Joystick Calibration
//Sterzo stuff
int FF = 0xFF;
uint8_t authChallenge[4] = {0x03, 0x10, 0xff, 0xff};
uint8_t authSuccess[3] = {0x03, 0x11, 0xff};

BLEServer *pServer = NULL;
BLECharacteristic *pAngle = NULL;
BLECharacteristic *pRx = NULL;
BLECharacteristic *pTx = NULL;
BLEAdvertising *pAdvertising;

//Server Callbacks
class MyServerCallbacks : public BLEServerCallbacks
{
    void onConnect(BLEServer *pServer)
    {
        deviceConnected = true;
        BLEDevice::startAdvertising();
        
    };

    void onDisconnect(BLEServer *pServer)
    {
        deviceConnected = false;
    }
};

//Characteristic Callbacks
class MyCharacteristicCallbacks : public BLECharacteristicCallbacks
{

    void onRead(BLECharacteristic *pRx)
    {

    }

    void onWrite(BLECharacteristic *pRx){

        std::string rxValue = pRx->getValue();
        
        if(rxValue.length() == 4){
          delay(250);
          pTx->setValue(authSuccess,3);
          pTx->indicate();
          auth = true;
          Serial.println("Auth Success!");
        }
    }
};

//Joystick read angle from axis
float readAngle()
{
    int potVal = analogRead(POT);
    Serial.println(potVal);
    angle = map(potVal,0,4095,-40,40);
    return angle;
}

//Arduino setup
void setup()
{
    //Serial Debug
    Serial.begin(115200);
    
    //Joystick Configuration
    pinMode(18, OUTPUT);
    pinMode(17, OUTPUT);
    pinMode(POT, INPUT);    // GPIO32 will be => Xaxis on Joystick
    digitalWrite(18, HIGH); // GPIO18 will be => +5v on Joystick
    digitalWrite(17, LOW);  // GPIO17 will be => GND on Joystick
    
    angle_deviation = readAngle();
   
    //Setup BLE
    Serial.println("Creating BLE server...");
    BLEDevice::init("STEERING");

    // Create the BLE Server
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());

    // Create the BLE Service
    Serial.println("Define service...");
    BLEService *pService = pServer->createService(STEERING_DEVICE_UUID);

    // Create BLE Characteristics
    Serial.println("Define characteristics");

    pTx = pService->createCharacteristic(STEERING_TX_CHAR_UUID, BLECharacteristic::PROPERTY_INDICATE | BLECharacteristic::PROPERTY_READ);
    pTx->addDescriptor(new BLE2902());
    
    pRx = pService->createCharacteristic(STEERING_RX_CHAR_UUID, BLECharacteristic::PROPERTY_WRITE);
    pRx->addDescriptor(new BLE2902());
    pRx->setCallbacks(new MyCharacteristicCallbacks());

    pAngle = pService->createCharacteristic(STEERING_ANGLE_CHAR_UUID, BLECharacteristic::PROPERTY_NOTIFY);
    pAngle->addDescriptor(new BLE2902());

    // Start the service
    Serial.println("Staring BLE service...");
    pService->start();

    // Start advertising
    Serial.println("Define the advertiser...");
    pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->setScanResponse(true);
    pAdvertising->addServiceUUID(STEERING_DEVICE_UUID);
    pAdvertising->setMinPreferred(0x06); // set value to 0x00 to not advertise this parameter
    Serial.println("Starting advertiser...");
    BLEDevice::startAdvertising();
    Serial.println("Waiting a client connection to notify...");
}

//Arduino loop
void loop()
{

    if (deviceConnected)
    {
        
        if(auth){
          angle = readAngle() - angle_deviation;
          pAngle->setValue(angle);
          pAngle->notify();
          Serial.print("TX Angle: ");
          Serial.println(angle);
          delay(250);
        } else {
          Serial.println("Auth Challenging");
          pTx->setValue(authChallenge, 4);
          pTx->indicate();
          delay(250);
        
        }
    }

    //Advertising
    if (!deviceConnected && oldDeviceConnected)
    {
        delay(300);                  // give the bluetooth stack the chance to get things ready
        pServer->startAdvertising(); // restart advertising
        Serial.println("Nothing connected, start advertising");
        oldDeviceConnected = deviceConnected;
    }
   
    //Connecting
    if (deviceConnected && !oldDeviceConnected)
    {
        oldDeviceConnected = deviceConnected;
        Serial.println("Connecting...");
    }

    if (!deviceConnected)
    {
        //Nothing
    }
}
