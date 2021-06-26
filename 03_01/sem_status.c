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

bool is_wait_by_sem(struct task_struct* t, struct semaphore* s)
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
