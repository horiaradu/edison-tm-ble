
#include "mbed.h"
#include "ble/BLE.h"
#include "ble/services/HeartRateService.h"
#include "ble/services/BatteryService.h"
#include "ble/services/DeviceInformationService.h"
#include "LocatorService.h"
#include "UARTService.h"

#define NEED_CONSOLE_OUTPUT 1 /* Set this if you need debug messages on the console;
                               * it will have an impact on code-size and power consumption. */
#if NEED_CONSOLE_OUTPUT
#define DEBUG(STR) { if (uart) uart->write(STR, strlen(STR)); }
#else
#define DEBUG(...) /* nothing */
#endif /* #if NEED_CONSOLE_OUTPUT */
UARTService *uart;

#define SERVICE_UUID 0x1821

DigitalOut led1(LED1);

const static char     DEVICE_NAME[]        = "BAT3.1";
static const uint16_t uuid16_list[]        = {GattService::UUID_HEART_RATE_SERVICE,
                                              SERVICE_UUID,
                                              GattService::UUID_DEVICE_INFORMATION_SERVICE};
static volatile bool  triggerSensorPolling = false;

uint8_t hrmCounter = 100; // init HRM to 100bps

HeartRateService         *hrService;
DeviceInformationService *deviceInfo;
LocatorService           *locatorService;

void disconnectionCallback(const Gap::DisconnectionCallbackParams_t *params)
{
    BLE::Instance(BLE::DEFAULT_INSTANCE).gap().startAdvertising(); // restart advertising
}

void periodicCallback(void)
{
    led1 = !led1; /* Do blinky on LED1 while we're waiting for BLE events */

    /* Note that the periodicCallback() executes in interrupt context, so it is safer to do
     * heavy-weight sensor polling from the main thread. */
    triggerSensorPolling = true;
}

void scanCallback(const Gap::AdvertisementCallbackParams_t *params) {
    char info[20];
    for(unsigned i=0;i<20;i++)
       info[i]='\0';
    unsigned rs=-params->rssi;
    sprintf(info, "%02x%02x%02x%02x%02x%02x;%d.", 
        params->peerAddr[5], params->peerAddr[4], params->peerAddr[3], params->peerAddr[2], params->peerAddr[1], params->peerAddr[0],
        rs);

   // printf("%s sResp:%u,aType:%u \r\n", info, params->isScanResponse, params->type);    
/*
//02 01 06 .. 09 get name
     for (unsigned index = 0; index < params->advertisingDataLen-7; index++) {
       if((params->advertisingData[index]==2)&&(params->advertisingData[index+1]==1)&&(params->advertisingData[index+2]==6)
          &&(params->advertisingData[index+4]==9)) {
       for (unsigned index2 = index+5; index2 < params->advertisingData[index+3]+index+5; index2++) //name is here
    ;//     printf("%d ", params->advertisingData[index2]);
          }
    }
    
  //  printf("\r\n");     
  */
    DEBUG(info);
//    locatorService->updateData(info);
//      char a[10];
//        a[0]=1;
//        a[1]=2;
//        a[2]=3;
//        a[3]=4;
//     locatorService->updateData(a);
}

void bleInitComplete(BLE::InitializationCompleteCallbackContext *params)
{
    BLE &ble          = params->ble;
    ble_error_t error = params->error;

    if (error != BLE_ERROR_NONE) {
        return;
    }

    ble.gap().onDisconnection(disconnectionCallback);
    /* Setup auxiliary service. */
    deviceInfo = new DeviceInformationService(ble, "ARM", "Model1", "SN1", "hw-rev1", "fw-rev1", "soft-rev1");

    /* Setup primary service. */
    hrService = new HeartRateService(ble, hrmCounter, HeartRateService::LOCATION_FINGER);
    locatorService = new LocatorService(ble);
    uart = new UARTService(ble);

    /* setup advertising */
    ble.gap().accumulateAdvertisingPayload(GapAdvertisingData::BREDR_NOT_SUPPORTED | GapAdvertisingData::LE_GENERAL_DISCOVERABLE);
    ble.gap().accumulateAdvertisingPayload(GapAdvertisingData::COMPLETE_LIST_16BIT_SERVICE_IDS, (uint8_t *)uuid16_list, sizeof(uuid16_list));
    ble.gap().accumulateAdvertisingPayload(GapAdvertisingData::GENERIC_HEART_RATE_SENSOR);
    ble.gap().accumulateAdvertisingPayload(GapAdvertisingData::COMPLETE_LIST_128BIT_SERVICE_IDS,
                                     (const uint8_t *)UARTServiceUUID_reversed, sizeof(UARTServiceUUID_reversed));
    ble.gap().accumulateAdvertisingPayload(GapAdvertisingData::COMPLETE_LOCAL_NAME, (uint8_t *)DEVICE_NAME, sizeof(DEVICE_NAME));
    ble.gap().setAdvertisingType(GapAdvertisingParams::ADV_CONNECTABLE_UNDIRECTED);
    ble.gap().setAdvertisingInterval(200); /* 1000ms */
    ble.gap().startAdvertising();
}

int main(void)
{
    led1 = 1;
    Ticker ticker;
    ticker.attach(periodicCallback, 1); // blink  every second

    BLE& ble = BLE::Instance(BLE::DEFAULT_INSTANCE);
    ble.init(bleInitComplete);

    /* SpinWait for initialization to complete. This is necessary because the
     * BLE object is used in the main loop below. */
    while (ble.hasInitialized()  == false) { /* spin loop */ }
    
    ble.gap().setScanParams(500 /* scan interval */, 300 /* scan window */);
    ble.gap().startScan(scanCallback);

    // infinite loop
    while (1) {
        // check for trigger from periodicCallback()
        if (triggerSensorPolling && ble.getGapState().connected) {
            triggerSensorPolling = false;

            // Do blocking calls or whatever is necessary for sensor polling.
            // In our case, we simply update the HRM measurement.
            hrmCounter++;
            if (hrmCounter == 175) { //  100 <= HRM bps <=175
                hrmCounter = 100;
            }

            hrService->updateHeartRate(hrmCounter);
        } else {
            ble.waitForEvent(); // low power wait for event
        }
    }
}
