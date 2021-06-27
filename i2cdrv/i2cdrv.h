#ifndef _I2CDRV_H_
#define _I2CDRV_H_

#include <linux/ioctl.h>

// IOCTL用コマンドのタイプ
#define I2CDRV_IOC_TYPE 'I'

// axiAddrで指定された領域への書き込みシーケンス制御
#define I2CDRV_WRITE_AXI \
    _IOW(I2CDRV_IOC_TYPE, 1, I2CDrvParam)

// axiAddrで指定された領域からの読み込みシーケンス制御
//   事前の書き込みを含めて一括で制御
#define I2CDRV_READ_COMPOUND_AXI \
    _IOR(I2CDRV_IOC_TYPE, 2, I2CDrvParam)

// IOCTRLパラメータ
typedef struct {
    int masterId;       // I2C masterのID (0-2)
    int slaveAddr;      // I2C Slaveのアドレス
    unsigned axiAddr;   // 対象のAXIアドレス
    unsigned *buffer;   // R/Wバッファー(User空間)
    int num;            // データ数(バイト数ではない)
} I2CDrvParam;

#endif // _I2CDRV_H_
