#include <linux/init.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/sched.h>
#include <linux/device.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/semaphore.h>
#include <linux/delay.h>
#include <linux/signal.h>

#include <asm/current.h>
#include <asm/uaccess.h>

#define USE_SEM

MODULE_LICENSE("Dual BSD/GPL");

/* /proc/devices等で表示されるデバイス名 */
#define DRIVER_NAME "MyDevice"

/* このデバイスドライバで使うマイナー番号の開始番号と個数(=デバイス数) */
static const unsigned int MINOR_BASE = 0;
static const unsigned int MINOR_NUM  = 2;	/* マイナー番号は 0 ~ 1 */

/* このデバイスドライバのメジャー番号(動的に決める) */
static unsigned int mydevice_major;

/* キャラクタデバイスのオブジェクト */
static struct cdev mydevice_cdev;

/* open時に呼ばれる関数 */
static int mydevice_open(struct inode *inode, struct file *file)
{
	printk("mydevice_open");
	return 0;
}

/* close時に呼ばれる関数 */
static int mydevice_close(struct inode *inode, struct file *file)
{
	printk("mydevice_close");
	return 0;
}

/* read時に呼ばれる関数 */
static ssize_t mydevice_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	printk("mydevice_read");
	buf[0] = 'A';
	return 1;
}

/* write時に呼ばれる関数 */
static ssize_t mydevice_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
	printk("mydevice_write");
	return 1;
}

/* 各種システムコールに対応するハンドラテーブル */
struct file_operations s_mydevice_fops = {
	.open    = mydevice_open,
	.release = mydevice_close,
	.read    = mydevice_read,
	.write   = mydevice_write,
};

static struct task_struct *g_thd[2];

static struct args_t {
    int pid;
    int idx;
    struct semaphore* sem;
} args[2];
static wait_queue_head_t g_wq;
static int g_cond = 0;
static struct completion comp;
static struct semaphore sem[2];

static int _entry_func(void* arg)
{
    printk("@@@@ Enter _entry_func() @@@@\n");

    struct args_t* a = (struct args_t*)arg;
    
    allow_signal(SIGKILL);

    printk("--- Now into wait completion ---\n");
#ifdef USE_SEM
    int ret = down_interruptible(a->sem);
    printk("--- down_interruptible() ret : %d ---\n", ret);
#else
    int ret = wait_for_completion_interruptible(&comp);
    printk("--- wait_for_completion_interruptible() ret : %d ---\n", ret);
#endif // USE_SEM

    return 0;
}

static bool is_wait_by_sem(struct task_struct* t, struct semaphore* s)
{
    // From kernel/locking/semaphore.c
    struct semaphore_waiter {
        struct list_head list;
        struct task_struct *task;
        bool up;
    };

    // s->wait_list.nextはsemaphore_waiterのリンクリストを保持している
    struct list_head* head = s->wait_list.next;
    struct semaphore_waiter* w = (struct semaphore_waiter*)head;
    int i;
    bool ret = false;
    // リストの終端はsemaphore_waiter.taskがNULLである事で判断できる（っぽい）
    // 安全の為に10回の繰り返しで抜けるようにしておく。
    // 失敗すると永久ループになってしまうので。
    for (i = 0; (w->task != NULL) && (i < 10); i++) {
        /* printk("-- w:%p,task:%p,up:%d --\n", w, w->task, w->up); */
        if (w->task == t) {
            ret = true;
            break;
        }
        head = head->next;
        w = (struct semaphore_waiter*)head;
    }
    /* printk("-- t:%p --\n", t); */
    return ret;
}

/* ロード(insmod)時に呼ばれる関数 */
static int mydevice_init(void)
{
	printk("@@@@ Enter mydevice_init() @@@@\n");

	int alloc_ret = 0;
	int cdev_err = 0;
	dev_t dev;

	/* 1. 空いているメジャー番号を確保する */
	alloc_ret = alloc_chrdev_region(&dev, MINOR_BASE, MINOR_NUM, DRIVER_NAME);
	if (alloc_ret != 0) {
		printk(KERN_ERR  "alloc_chrdev_region = %d\n", alloc_ret);
		return -1;
	}

	/* 2. 取得したdev( = メジャー番号 + マイナー番号)からメジャー番号を
       取得して保持しておく */
	mydevice_major = MAJOR(dev);
	dev = MKDEV(mydevice_major, MINOR_BASE);	/* 不要? */

	/* 3. cdev構造体の初期化とシステムコールハンドラテーブルの登録 */
	cdev_init(&mydevice_cdev, &s_mydevice_fops);
	mydevice_cdev.owner = THIS_MODULE;

	/* 4. このデバイスドライバ(cdev)をカーネルに登録する */
	cdev_err = cdev_add(&mydevice_cdev, dev, MINOR_NUM);
	if (cdev_err != 0) {
		printk(KERN_ERR  "cdev_add = %d\n", alloc_ret);
		unregister_chrdev_region(dev, MINOR_NUM);
		return -1;
	}

    // g_thdを起こす
    int i;
#ifdef USE_SEM
    for (i = 0; i < 2; i++)
        sema_init(&sem[i], 0);
#else
    init_completion(&comp);
#endif // USE_SEM
    for (i = 0; i < 2; i++) {
        char name[256];
        sprintf(name, "Tkernel_%d", i+1);
        g_thd[i] = kthread_create(_entry_func, (void*)&args[i], name);
        printk("-- Thread ID:%d, Name:%s --\n", g_thd[i]->pid, name);
        args[i].pid = g_thd[i]->pid;
        args[i].idx = i;
        args[i].sem = &sem[i];
        wake_up_process(g_thd[i]);
    }
    mdelay(100);
    int j;
    for (i = 0; i < 2; i++) {
        for (j = 0; j < 2; j++) {
            int ret = (int)is_wait_by_sem(g_thd[i], &sem[j]);
            printk("-- (i,j)=(%d,%d),is_wait_by_sem():%d --\n", i, j, ret);
        }
    }
	return 0;
}

/* アンロード(rmmod)時に呼ばれる関数 */
static void mydevice_exit(void)
{
	printk("mydevice_exit\n");
#ifdef USE_SEM
    int i;
    for (i = 0; i < sizeof(args)/sizeof(struct args_t); i++) {
        up(&sem[i]);
    }
#else
    complete_all(&comp);
#endif // USE_SEM
    
	dev_t dev = MKDEV(mydevice_major, MINOR_BASE);
	
	/* 5. このデバイスドライバ(cdev)をカーネルから取り除く */
	cdev_del(&mydevice_cdev);

	/* 6. このデバイスドライバで使用していたメジャー番号の登録を取り除く */
	unregister_chrdev_region(dev, MINOR_NUM);
}

module_init(mydevice_init);
module_exit(mydevice_exit);
