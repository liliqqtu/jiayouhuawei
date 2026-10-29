#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "iot_i2c.h"
#include "iot_i2c_ex.h"

#include "nfc.h"
#include "nfcForum.h"
#include "NT3H.h"

/**
 * @brief Defines I2C data transmission attributes.
 */
typedef struct {
    /** Pointer to the buffer storing data to send */
    unsigned char *sendBuf;
    /** Length of data to send */
    unsigned int  sendLen;
    /** Pointer to the buffer for storing data to receive */
    unsigned char *receiveBuf;
    /** Length of data received */
    unsigned int  receiveLen;
} WifiIotI2cData;

uint8_t     nfcPageBuffer[NFC_PAGE_SIZE];
NT3HerrNo   errNo;
// due to the nature of the NT3H a timeout is required to
// protectd 2 consecutive I2C access

inline const uint8_t *get_last_ncf_page(void)
{
    return nfcPageBuffer;
}

static bool writeTimeout(uint8_t *data, uint8_t dataSend)
{
    uint32_t status = 0;

    status = IoTI2cWrite(1, (NT3H1X_ADDRESS << 1) | 0x00, data, dataSend);
    if (status != 0) {
        printf("===== Error: I2C write status = 0x%x! =====\r\n", status);
        return 0;
    }
    usleep(NT3H1X_WRITE_TIMEOUT_US);
    return 1;
}

static bool readTimeout(uint8_t address, uint8_t *block_data)
{
    uint32_t status = 0;
    IotI2cData nt3h1101_i2c_data = { 0 };
    uint8_t  buffer[1] = { address };
    nt3h1101_i2c_data.sendBuf = buffer;
    nt3h1101_i2c_data.sendLen = 1;
    nt3h1101_i2c_data.receiveBuf = block_data;
    nt3h1101_i2c_data.receiveLen = NFC_PAGE_SIZE;
    status = IoTI2cWriteread(1, (NT3H1X_ADDRESS << 1) | 0x00, &nt3h1101_i2c_data);
    if (status != 0) {
        printf("===== Error: I2C write status = 0x%x! =====\r\n", status);
        return 0;
    }
    return 1;
}


bool NT3HReadHeaderNfc(uint8_t *endRecordsPtr, uint8_t *ndefHeader)
{
    *endRecordsPtr = 0;
    bool ret = NT3HReadUserData(0);
    // read the first page to see where is the end of the Records.
    if (ret == true) {
        // if the first byte is equals to NDEF_START_BYTE there are some records
        // store theend of that
        if ((NDEF_START_BYTE == nfcPageBuffer[0]) && (NTAG_ERASED != nfcPageBuffer[NDEFHeader])) {
            *endRecordsPtr = nfcPageBuffer[EndRecordsPtr];
            *ndefHeader = nfcPageBuffer[NDEFHeader];
        }
        return true;
    } else {
        errNo = NT3HERROR_READ_HEADER;
    }

    return ret;
}


bool NT3HWriteHeaderNfc(uint8_t endRecordsPtr, uint8_t ndefHeader)
{
// read the first page to see where is the end of the Records.
    bool ret = NT3HReadUserData(0);
    if (ret == true) {
        nfcPageBuffer[EndRecordsPtr] = endRecordsPtr;
        nfcPageBuffer[NDEFHeader] = ndefHeader;
        ret = NT3HWriteUserData(0, nfcPageBuffer);
        if (ret == false) {
            errNo = NT3HERROR_WRITE_HEADER;
        }
    } else {
        errNo = NT3HERROR_READ_HEADER;
    }

    return ret;
}

bool NT3HEraseAllTag(void)
{
    bool ret = true;
    uint8_t erase[NFC_PAGE_SIZE + 1] = { USER_START_REG, 0x03, 0x03, 0xD0, 0x00, 0x00, 0xFE };
    ret = writeTimeout(erase, sizeof(erase));
    if (ret == false) {
        errNo = NT3HERROR_ERASE_USER_MEMORY_PAGE;
    }
    return ret;
}

bool NT3HReaddManufactoringData(uint8_t *manuf)
{
    return readTimeout(MANUFACTORING_DATA_REG, manuf);
}

bool NT3HReadConfiguration(uint8_t *configuration)
{
    return readTimeout(CONFIG_REG, configuration);
}

bool getSessionReg(void)
{
    return readTimeout(SESSION_REG, nfcPageBuffer);
}

bool NT3HReadUserData(uint8_t page)
{
    uint8_t reg = USER_START_REG + page;
    // if the requested page is out of the register exit with error
    if (reg > USER_END_REG) {
        errNo = NT3HERROR_INVALID_USER_MEMORY_PAGE;
        return false;
    }
    bool ret = readTimeout(reg, nfcPageBuffer);
    if (ret == false) {
        errNo = NT3HERROR_READ_USER_MEMORY_PAGE;
    }

    return ret;
}

bool NT3HWriteUserData(uint8_t page, const uint8_t *data)
{
    bool ret = true;
    uint8_t dataSend[NFC_PAGE_SIZE + 1]; // data plus register
    uint8_t reg = USER_START_REG + page;

    // if the requested page is out of the register exit with error
    if (reg > USER_END_REG) {
        errNo = NT3HERROR_INVALID_USER_MEMORY_PAGE;
        ret = false;
        return ret;
    }

    dataSend[0] = reg; // store the register
    memcpy_s(&dataSend[1], NFC_PAGE_SIZE, data, NFC_PAGE_SIZE);
    ret = writeTimeout(dataSend, sizeof(dataSend));
    if (ret == false) {
        errNo = NT3HERROR_WRITE_USER_MEMORY_PAGE;
        return ret;
    }
    return ret;
}

bool NT3HReadSram(void)
{
    bool ret = false;
    for (int i = SRAM_START_REG, j = 0; i <= SRAM_END_REG; i++, j++) {
        ret = readTimeout(i, nfcPageBuffer);
        if (ret == false) {
            return ret;
        }
    }
    return ret;
}

void NT3HGetNxpSerialNumber(char *buffer)
{
    uint8_t manuf[16];

    if (NT3HReaddManufactoringData(manuf)) {
        for (int i = 0; i < SERIAL_NUM_LEN; i++) {
            buffer[i] = manuf[i];
        }
    }
}
typedef uint8_t(*composeRtdPtr)(const NDEFDataStr *ndef, NDEFRecordStr *ndefRecord, uint8_t *I2CMsg);
static composeRtdPtr composeRtd[] = { composeRtdText, composeRtdUri };

int16_t firstRecord(UncompletePageStr *page, const NDEFDataStr *data, RecordPosEnu rtdPosition)
{
    NDEFRecordStr record;
    NDEFHeaderStr header;
    uint8_t typeFunct = 0;
    switch (data->rtdType) {
        case RTD_TEXT:
            typeFunct = 0;
            break;

        case RTD_URI:
            typeFunct = 1;
            break;

        default:
            return -1;
            break;
    }

    // clear all buffers
    memset_s(&record, sizeof(NDEFRecordStr), 0, sizeof(NDEFRecordStr));
    memset_s(nfcPageBuffer, NFC_PAGE_SIZE, 0, NFC_PAGE_SIZE);

    // this is the first record
    header.startByte = NDEF_START_BYTE;
    composeNDEFMBME(true, true, &record);

    // prepare the NDEF Header and payload
    uint8_t recordLength = composeRtd[typeFunct](data, &record, &nfcPageBuffer[sizeof(NDEFHeaderStr)]);
    header.payloadLength = data->rtdPayloadlength + recordLength;

    // write first record
    memcpy_s(nfcPageBuffer, sizeof(NDEFHeaderStr), &header, sizeof(NDEFHeaderStr));

    return sizeof(NDEFHeaderStr) + recordLength;
}


int16_t addRecord(UncompletePageStr *pageToUse, const NDEFDataStr *data, RecordPosEnu rtdPosition)
{
    NDEFRecordStr record;
    NDEFHeaderStr header = { 0 };
    uint8_t       newRecordPtr, mbMe;
    bool          ret = true;
    uint8_t       tmpBuffer[NFC_PAGE_SIZE];

    uint8_t typeFunct = 0;

    switch (data->rtdType) {
        case RTD_TEXT:
            typeFunct = 0;
            break;

        case RTD_URI:
            typeFunct = 1;
            break;

        default:
            return -1;
            break;
    }

    // first Change the Header of the first Record
    NT3HReadHeaderNfc(&newRecordPtr, &mbMe);
    record.header = mbMe;
    composeNDEFMBME(true, false, &record); // this is the first record
    mbMe = record.header;

    memset_s(&record, sizeof(NDEFRecordStr), 0, sizeof(NDEFRecordStr));
    memset_s(tmpBuffer, NFC_PAGE_SIZE, 0, NFC_PAGE_SIZE);

    // prepare second record
    uint8_t recordLength = composeRtd[typeFunct](data, &record, tmpBuffer);

    if (rtdPosition == NDEFMiddlePos) {
        // this is a record in the middle adjust it on the buffet
        composeNDEFMBME(false, false, &record);
    } else if (rtdPosition == NDEFLastPos) {
        // this is the last record adjust it on the buffet
        composeNDEFMBME(false, true, &record);
    }

    tmpBuffer[0] = record.header;

    header.payloadLength += data->rtdPayloadlength + recordLength;

    // save the new value of length on the first page
    NT3HWriteHeaderNfc((newRecordPtr + header.payloadLength), mbMe);

    // use the last valid page and start to add the new record
    NT3HReadUserData(pageToUse->page);
    if (pageToUse->usedBytes + recordLength < NFC_PAGE_SIZE) {
        memcpy_s(&nfcPageBuffer[pageToUse->usedBytes], recordLength, tmpBuffer, recordLength);
        return recordLength + pageToUse->usedBytes;
    } else {
        uint8_t byteToCopy = NFC_PAGE_SIZE - pageToUse->usedBytes;
        memcpy_s(&nfcPageBuffer[pageToUse->usedBytes], byteToCopy, tmpBuffer, byteToCopy);
        NT3HWriteUserData(pageToUse->page, nfcPageBuffer);
        // update the info with the new page
        pageToUse->page++;
        pageToUse->usedBytes = recordLength - byteToCopy;
        // copy the remain part in the pageBuffer because this is what the caller expect
        memcpy_s(nfcPageBuffer, pageToUse->usedBytes, &tmpBuffer[byteToCopy], pageToUse->usedBytes);
        return pageToUse->usedBytes;
    }
}

static bool writeUserPayload(int16_t payloadPtr, const NDEFDataStr *data, UncompletePageStr *addPage)
{
    uint8_t addedPayload;
    bool ret = false;

    uint8_t finish = payloadPtr + data->rtdPayloadlength;
    bool endRecord = false;
    uint8_t copyByte = 0;

    // if the header is less then the NFC_PAGE_SIZE, fill it with the payload
    if (NFC_PAGE_SIZE > payloadPtr) {
        if (data->rtdPayloadlength > NFC_PAGE_SIZE - payloadPtr) {
            copyByte = NFC_PAGE_SIZE - payloadPtr;
        } else {
            copyByte = data->rtdPayloadlength;
        }
    }

    // Copy the payload
    memcpy_s(&nfcPageBuffer[payloadPtr], copyByte, data->rtdPayload, copyByte);
    addedPayload = copyByte;

    // if it is sufficient one send add the NDEF_END_BYTE
    if ((addedPayload >= data->rtdPayloadlength) && ((payloadPtr + copyByte) < NFC_PAGE_SIZE)) {
        nfcPageBuffer[(payloadPtr + copyByte)] = NDEF_END_BYTE;
        endRecord = true;
    }

    ret = NT3HWriteUserData(addPage->page, nfcPageBuffer);

    while (!endRecord) {
        addPage->page++; // move to a new register

        memset_s(nfcPageBuffer, NFC_PAGE_SIZE, 0, NFC_PAGE_SIZE);
        // special case just the NDEF_END_BYTE remain out
        if (addedPayload == data->rtdPayloadlength) {
            nfcPageBuffer[0] = NDEF_END_BYTE;
            endRecord = true;
            if ((ret = NT3HWriteUserData(addPage->page, nfcPageBuffer)) == false) {
                errNo = NT3HERROR_WRITE_NDEF_TEXT;
            }
            return ret;
        }

        if (addedPayload < data->rtdPayloadlength) {
            // add the NDEF_END_BYTE if there is enough space
            if ((data->rtdPayloadlength - addedPayload) < NFC_PAGE_SIZE) {
                memcpy_s(nfcPageBuffer, (data->rtdPayloadlength - addedPayload),
                    &data->rtdPayload[addedPayload], (data->rtdPayloadlength - addedPayload));
                nfcPageBuffer[(data->rtdPayloadlength - addedPayload)] = NDEF_END_BYTE;
            } else {
                memcpy_s(nfcPageBuffer, NFC_PAGE_SIZE, &data->rtdPayload[addedPayload], NFC_PAGE_SIZE);
            }
            addedPayload += NFC_PAGE_SIZE;
            if ((ret = NT3HWriteUserData(addPage->page, nfcPageBuffer)) == false) {
                errNo = NT3HERROR_WRITE_NDEF_TEXT;
                return ret;
            }
        } else {
            endRecord = true;
        }
    }

    return ret;
}

typedef int16_t(*addFunct_T) (UncompletePageStr *page, const NDEFDataStr *data, RecordPosEnu rtdPosition);
static addFunct_T addFunct[] = { firstRecord, addRecord, addRecord };

bool NT3HwriteRecord(const NDEFDataStr *data)
{
    uint8_t recordLength = 0, mbMe;
    UncompletePageStr addPage;
    addPage.page = 0;

    // calculate the last used page
    if (data->ndefPosition != NDEFFirstPos) {
        NT3HReadHeaderNfc(&recordLength, &mbMe);
        addPage.page = (recordLength + sizeof(NDEFHeaderStr) + 1) / NFC_PAGE_SIZE;

        // remove the NDEF_END_BYTE byte because it will overwrite by the new Record
        addPage.usedBytes = (recordLength + sizeof(NDEFHeaderStr) + 1) % NFC_PAGE_SIZE - 1;
    }

    // call the appropriate function and consider the pointer
    // within the NFC_PAGE_SIZE that need to be used
    int16_t payloadPtr = addFunct[data->ndefPosition](&addPage, data, data->ndefPosition);
    if (payloadPtr == -1) {
        errNo = NT3HERROR_TYPE_NOT_SUPPORTED;
        return false;
    }

    return  writeUserPayload(payloadPtr, data, &addPage);
}