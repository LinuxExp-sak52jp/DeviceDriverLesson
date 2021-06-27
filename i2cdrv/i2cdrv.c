#include <linux/init.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/sched.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/printk.h>
#include <asm/current.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include "i2cdrv.h"

// デバイスに関する情報
MODULE_LICENSE("Dual BSD/GPL");

// /proc/devices等で表示されるデバイス名
#define DRIVER_NAME "i2cdrv"

// マイナー番号の開始番号とデバイス数 */
static const unsigned int MINOR_BASE = 0;
static const unsigned int MINOR_NUM  = 1;

// メジャー番号(動的に決める)
static unsigned int i2cdrv_major;

// キャラクタデバイスのオブジェクト
static struct cdev i2cdrv_cdev;

// デバイスドライバのクラスオブジェクト
static struct class *i2cdrv_class = NULL;

// ioctl用に値を保持する変数
static I2CDrvParam i2cdrvParam;

/**
 * @brief パラメータを初期化する
 */
static void i2cdrv_initparam(I2CDrvParam *param)
{
    param->masterId = -1;
    param->slaveAddr = -1;
    param->axiAddr = 0;
    param->buffer = NULL;
    param->num = 0;
}

/**
 * @brief パラメータをクリアする
 */
static void i2cdrv_clearparam(I2CDrvParam *param)
{
    if (param->buffer != NULL)
        kfree(param->buffer);

    i2cdrv_initparam(param);
}

/**
 * @brief user空間から取得したパラメータを内部変数にコピー
 */
static int i2cdrv_copyparam_from(I2CDrvParam *dst, I2CDrvParam *userSrc)
{
    if (userSrc->buffer != NULL) {
        printk(KERN_ERR "dstが初期化されていません\n");
        return -1;
    }
        
    return 0;
}

/**
 * @brief 結果をuser空間へコピー
 */
static int i2cdrv_copyparam_to(I2CDrvParam *src, I2CDrvParam *userDst)
{

    return 0;
}

/**
 * @brief 指定されたAXIアドレスに書き込む
 */
static int i2cdrv_write_axi(void)
{
    return 0;
}

/**
 * @brief 指定されたAXIアドレスから読み込む
 */
static int i2cdrv_readcompound_axi(void)
{
    return 0;
}

/**
 * @brief ioctlハンドラ
 */
static long i2cdrv_ioctl(
    struct file *filp,
    unsigned int cmd,
    unsigned long arg)
{
    printk("i2cdrv_ioctl\n");
    
    switch (cmd) {
        case I2CDRV_WRITE_AXI:
            printk("I2CDRV_WRITE_AXI\n");
            if (i2cdrv_copyparam_from(&i2cdrvParam, (I2CDrvParam *)arg)) {
                return -EFAULT;
            }
            if (i2cdrv_write_axi()) {
                i2cdrv_clearparam(&i2cdrvParam);
                return -EFAULT;
            }
            i2cdrv_clearparam(&i2cdrvParam);
            break;
        case I2CDRV_READ_COMPOUND_AXI:
            printk("I2CDRV_READ_COMPOUND_AXI\n");
            if (i2cdrv_copyparam_from(&i2cdrvParam, (I2CDrvParam *)arg)) {
                return -EFAULT;
            }
            if (i2cdrv_readcompound_axi()) {
                i2cdrv_clearparam(&i2cdrvParam);
                return -EFAULT;
            }
            if (i2cdrv_copyparam_to(&i2cdrvParam, (I2CDrvParam *)arg)) {
                return -EFAULT;
            }
            i2cdrv_clearparam(&i2cdrvParam);
            break;
        default:
            printk(KERN_WARNING "unsupported command %d\n", cmd);
            return -EFAULT;
    }
    return 0;
}


/* open時に呼ばれる関数 */
static int i2cdrv_open(struct inode *inode, struct file *file)
{
    printk("i2cdrv_open\n");
    return 0;
}

/* close時に呼ばれる関数 */
static int i2cdrv_close(struct inode *inode, struct file *file)
{
    printk("i2cdrv_close\n");
    return 0;
}

/* read時に呼ばれる関数 */
static ssize_t i2cdrv_read(
    struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
    printk("i2cdrv_read\n");
    return count;
}

/* write時に呼ばれる関数 */
static ssize_t i2cdrv_write(
    struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
    printk("i2cdrv_write\n");
    return count;
}

/* 各種システムコールに対応するハンドラテーブル */
static struct file_operations i2cdrv_fops = {
    .open    = i2cdrv_open,
    .release = i2cdrv_close,
    .read    = i2cdrv_read,
    .write   = i2cdrv_write,
    .unlocked_ioctl = i2cdrv_ioctl,
    .compat_ioctl   = i2cdrv_ioctl,   // for 32-bit App
};

/* ロード(insmod)時に呼ばれる関数 */
static int i2cdrv_init(void)
{
    printk("i2cdrv_init\n");

    int alloc_ret = 0;
    int cdev_err = 0;
    dev_t dev;

    // 空いているメジャー番号を確保する
    alloc_ret = alloc_chrdev_region(&dev, MINOR_BASE, MINOR_NUM, DRIVER_NAME);
    if (alloc_ret != 0) {
        printk(KERN_ERR  "alloc_chrdev_region = %d\n", alloc_ret);
        return -1;
    }

    // 取得したdev( = メジャー番号 + マイナー番号)からメジャー番号を取得して保持しておく
    i2cdrv_major = MAJOR(dev);
    dev = MKDEV(i2cdrv_major, MINOR_BASE);

    // cdev構造体の初期化とシステムコールハンドラテーブルの登録
    cdev_init(&i2cdrv_cdev, &i2cdrv_fops);
    i2cdrv_cdev.owner = THIS_MODULE;

    // このデバイスドライバ(cdev)をカーネルに登録する
    cdev_err = cdev_add(&i2cdrv_cdev, dev, MINOR_NUM);
    if (cdev_err != 0) {
        printk(KERN_ERR  "cdev_add = %d\n", alloc_ret);
        unregister_chrdev_region(dev, MINOR_NUM);
        return -1;
    }

    // このデバイスのクラス登録をする(/sys/class/i2cdrv/ を作る)
    i2cdrv_class = class_create(THIS_MODULE, DRIVER_NAME);
    if (IS_ERR(i2cdrv_class)) {
        printk(KERN_ERR  "class_create\n");
        cdev_del(&i2cdrv_cdev);
        unregister_chrdev_region(dev, MINOR_NUM);
        return -1;
    }

    // /sys/class/i2cdrv/i2cdrv* を作る
    for (int minor = MINOR_BASE; minor < MINOR_BASE + MINOR_NUM; minor++) {
        device_create(i2cdrv_class,
                      NULL,
                      MKDEV(i2cdrv_major, minor),
                      NULL,
                      "i2cdrv%d", minor);
    }
    return 0;
}

/* アンロード(rmmod)時に呼ばれる関数 */
static void i2cdrv_exit(void)
{
    printk("i2cdrv_exit\n");

    dev_t dev = MKDEV(i2cdrv_major, MINOR_BASE);
    
    // /sys/class/i2cdrv/i2cdrv* を削除する
    for (int minor = MINOR_BASE; minor < MINOR_BASE + MINOR_NUM; minor++) {
        device_destroy(i2cdrv_class, MKDEV(i2cdrv_major, minor));
    }

    // このデバイスのクラス登録を取り除く(/sys/class/i2cdrv/を削除する)
    class_destroy(i2cdrv_class);

    // このデバイスドライバ(cdev)をカーネルから取り除く
    cdev_del(&i2cdrv_cdev);

    // このデバイスドライバで使用していたメジャー番号の登録を取り除く
    unregister_chrdev_region(dev, MINOR_NUM);
}

module_init(i2cdrv_init);
module_exit(i2cdrv_exit);
