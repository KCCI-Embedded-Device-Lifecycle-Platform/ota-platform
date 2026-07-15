#include "object_bms.h"
//#include <bits/stdc++.h>
#include <iostream>
using namespace std;

int main()
{
    lwm2m_object_t *bmsObjectP = get_bms_object();

    if(bmsObjectP == nullptr){
        cerr << "Failed to create BMS Object\n";
        return 1;
    }
    int numData = 0;
    lwm2m_data_t *dataArrayP = nullptr;

    uint8_t readResult = bmsObjectP->readFunc(nullptr, 0, &numData, &dataArrayP, bmsObjectP);

    double voltage = 0.0;
    bool readSucceeded = readResult == COAP_205_CONTENT &&
                         numData == 1 &&
                        dataArrayP != nullptr &&
                        lwm2m_data_decode_float(&dataArrayP[0], &voltage) != 0;
    
    if(readSucceeded)
    {
        cout << "BMS object ID: "<< bmsObjectP->objID << '\n';
        cout << "BMS voltage: " << voltage << '\n';
    }
    else
    {
        cerr << "Failed to read BMS voltage\n";
    }

    if (dataArrayP != nullptr)
        lwm2m_data_free(numData, dataArrayP);

    free_bms_object(bmsObjectP);
    
    return readSucceeded ? 0 : 1;
}