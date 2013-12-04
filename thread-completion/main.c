#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/completion.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <asm/delay.h>	// for udelay()
#include <linux/nmi.h>	// for touch_nmi_watchdog()
#include <linux/proc_fs.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("GongLei <freshpassport@gmail.com>");

DECLARE_COMPLETION(block_comp);
DECLARE_WAIT_QUEUE_HEAD(agent);

struct task_struct *block_thread, *unblock_thread;

static int thread_block(void *data)
{
	do {
		set_current_state(TASK_INTERRUPTIBLE);

		udelay(500);
		
		printk(KERN_NOTICE"start wait completion!");
		wait_for_completion(&block_comp);
		printk(KERN_NOTICE"end wait completion!");

		schedule();

	} while (!kthread_should_stop());

	return 0;
}

static int thread_unblock(void *data)
{
	DECLARE_WAITQUEUE(wait, current);
	add_wait_queue(&agent, &wait);

	do {
		set_current_state(TASK_INTERRUPTIBLE);
		touch_softlockup_watchdog();
		touch_nmi_watchdog();

		schedule();

		printk(KERN_NOTICE"sent completion done!");
		complete(&block_comp);

	} while(!kthread_should_stop());

	return 0;
}

#define COMP_CTL_NAME "comp"
static struct proc_dir_entry *comp_ctl = NULL;

static int comp_ctl_write(struct file *file, const char __user *buf,
		unsigned long count, void *data)
{
	wake_up(&agent);
	return count;
}

void env_init(void)
{
	comp_ctl = create_proc_entry(COMP_CTL_NAME, 0644, NULL);
	if (comp_ctl) {
		comp_ctl->data = NULL;
		comp_ctl->read_proc = NULL;
		comp_ctl->write_proc = comp_ctl_write;
	}
}

void env_exit(void)
{
	if (comp_ctl)
		remove_proc_entry(COMP_CTL_NAME, NULL);
}

static int completion_test_init(void)
{
	block_thread = kthread_run(thread_block, NULL, "block");
	if (IS_ERR(block_thread)) {
		printk(KERN_NOTICE"fail to create block thread!");
		block_thread = NULL;
		unblock_thread = NULL;
		return -1;
	}

	unblock_thread = kthread_run(thread_unblock, NULL, "unblock");
	if (IS_ERR(unblock_thread)) {
		printk(KERN_NOTICE"fail to create unblock thread!");
		kthread_stop(block_thread);
		block_thread = NULL;
		unblock_thread = NULL;
		return -1;
	}

	env_init();

	return 0;
}

static void completion_test_exit(void)
{
	if (block_thread)
		kthread_stop(block_thread);
	if (unblock_thread)
		kthread_stop(unblock_thread);
	env_exit();
}

module_init(completion_test_init);
module_exit(completion_test_exit);
