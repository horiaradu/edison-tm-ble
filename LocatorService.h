#include "ble/BLE.h"

class LocatorService {
public:
    typedef uint8_t* dist_t;
    ReadOnlyGattCharacteristic<dist_t> distanceCharacteristic;
    
    LocatorService(BLE& _ble) :
        ble(_ble),
        distanceCharacteristic(0x2A59, &dist) 
    {            
        GattCharacteristic *charTable[] = { &distanceCharacteristic };
        GattService distService(0x1821, charTable, sizeof(charTable) / sizeof(GattCharacteristic *));
        ble.gattServer().addService(distService);
    }
    
    /**
     * @brief   Update temperature characteristic.
     * @param   newTemperatureVal New temperature measurement.
     */
    void updateData(char* data)
    {
        printf("%s\n\r", data);
        ble.gattServer().write(distanceCharacteristic.getValueHandle(), (uint8_t *) data, 4);
    }
private:
    BLE& ble;
    
    dist_t dist;
};