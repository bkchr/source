/*
 *  ALFA NETWORK Hornet-UB board support
 *
 *  Copyright (C) 2011-2012 Gabor Juhos <juhosg@openwrt.org>
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License version 2 as published
 *  by the Free Software Foundation.
 */

#include <linux/gpio.h>
#include <linux/printk.h>
#include <linux/delay.h>

#include <asm/mach-ath79/ath79.h>
#include <asm/mach-ath79/ar71xx_regs.h>

#include "common.h"
#include "dev-eth.h"
#include "dev-gpio-buttons.h"
#include "dev-leds-gpio.h"
#include "dev-m25p80.h"
#include "dev-usb.h"
#include "dev-wmac.h"
#include "machtypes.h"

#define HORNET_UB_GPIO_LED_WLAN		0
#define HORNET_UB_GPIO_LED_USB		1
#define HORNET_UB_GPIO_LED_LAN		13
#define HORNET_UB_GPIO_LED_WAN		17
#define HORNET_UB_GPIO_LED_WPS		27
#define HORNET_UB_GPIO_EXT_LNA		28

#define HORNET_UB_GPIO_BTN_RESET	12
#define HORNET_UB_GPIO_BTN_WPS		11

#define HORNET_UB_GPIO_USB_POWER	26

#define HORNET_UB_KEYS_POLL_INTERVAL	20	/* msecs */
#define HORNET_UB_KEYS_DEBOUNCE_INTERVAL	(3 * HORNET_UB_KEYS_POLL_INTERVAL)

#define HORNET_UB_MAC0_OFFSET		0x0000
#define HORNET_UB_MAC1_OFFSET		0x0006
#define HORNET_UB_CALDATA_OFFSET	0x1000
/*
static struct gpio_led hornet_ub_leds_gpio[] __initdata = {
	{
		.name		= "alfa:blue:lan",
		.gpio		= HORNET_UB_GPIO_LED_LAN,
		.active_low	= 0,
	},
	{
		.name		= "alfa:blue:usb",
		.gpio		= HORNET_UB_GPIO_LED_USB,
		.active_low	= 0,
	},
	{
		.name		= "alfa:blue:wan",
		.gpio		= HORNET_UB_GPIO_LED_WAN,
		.active_low	= 1,
	},
	{
		.name		= "alfa:blue:wlan",
		.gpio		= HORNET_UB_GPIO_LED_WLAN,
		.active_low	= 0,
	},
	{
		.name		= "alfa:blue:wps",
		.gpio		= HORNET_UB_GPIO_LED_WPS,
		.active_low	= 1,
	},
};

static struct gpio_keys_button hornet_ub_gpio_keys[] __initdata = {
	{
		.desc		= "WPS button",
		.type		= EV_KEY,
		.code		= KEY_WPS_BUTTON,
		.debounce_interval = HORNET_UB_KEYS_DEBOUNCE_INTERVAL,
		.gpio		= HORNET_UB_GPIO_BTN_WPS,
		.active_low	= 0,
	},
	{
		.desc		= "Reset button",
		.type		= EV_KEY,
		.code		= KEY_RESTART,
		.debounce_interval = HORNET_UB_KEYS_DEBOUNCE_INTERVAL,
		.gpio		= HORNET_UB_GPIO_BTN_RESET,
		.active_low	= 1,
	}
};

static void __init hornet_ub_gpio_setup(void)
{
	u32 t;

	ath79_gpio_function_disable(AR933X_GPIO_FUNC_ETH_SWITCH_LED0_EN |
				     AR933X_GPIO_FUNC_ETH_SWITCH_LED1_EN |
				     AR933X_GPIO_FUNC_ETH_SWITCH_LED2_EN |
				     AR933X_GPIO_FUNC_ETH_SWITCH_LED3_EN |
				     AR933X_GPIO_FUNC_ETH_SWITCH_LED4_EN);

	t = ath79_reset_rr(AR933X_RESET_REG_BOOTSTRAP);
	t |= AR933X_BOOTSTRAP_MDIO_GPIO_EN;
	ath79_reset_wr(AR933X_RESET_REG_BOOTSTRAP, t);

	gpio_request_one(HORNET_UB_GPIO_USB_POWER,
			 GPIOF_OUT_INIT_HIGH | GPIOF_EXPORT_DIR_FIXED,
			 "USB power");
	gpio_request_one(HORNET_UB_GPIO_EXT_LNA,
			GPIOF_OUT_INIT_HIGH | GPIOF_EXPORT_DIR_FIXED,
			"external LNA0");

}

#define CONFIG_SH_TRICOLOR_LED
#define CONFIG_SH_TRICOLOR_RED_LED_GPIO 15
#define CONFIG_SH_TRICOLOR_GREEN_LED_GPIO 14
#define CONFIG_SH_SWITCH_LED_GPIO 16
#define CONFIG_SH_SWITCH_CTRL_OUT_GPIO 28
#define CONFIG_SH_SWITCH_CTRL_IN_GPIO 17
// I don't have any idea where that is defined in the original kernel
#define CONFIG_SH_SWITCH_CTRL_IN_ON 0

static struct proc_dir_entry *smart_home_entry = NULL;

static struct proc_dir_entry *sh_led_off_entry = NULL;
static atomic_t led_off = ATOMIC_INIT(0);
static int gpio_sh_led_off_read (char *page, char **start, off_t off,
               int count, int *eof, void *data)
{
	return sprintf (page, "%d\n", atomic_read(&led_off));
}

static int gpio_sh_led_off_write (struct file *file, const char *buf,
                                        unsigned long count, void *data)
{
	 u_int32_t val;

    if (sscanf(buf, "%d", &val) != 1)
        return -EINVAL;

	if (val == 0){
		atomic_set(&led_off, 0);
	}
	else{
		atomic_set(&led_off, 1);
	}

	return count;
}


#ifdef CONFIG_RESET_GPIO
static struct proc_dir_entry *sh_reset_entry = NULL;
static int rst_btn_down = 0;

static int gpio_sh_reset_read (char *page, char **start, off_t off,
               int count, int *eof, void *data)
{
	int tmp = rst_btn_down;
	return sprintf (page, "%d\n", tmp);
}

static int gpio_sh_reset_write (struct file *file, const char *buf,
                                        unsigned long count, void *data)
{
	return count;
}

irqreturn_t reset_irq(int cpl, void *dev_id, struct pt_regs *regs)
{
	if (rst_btn_down)
	{
		ar7240_gpio_config_int (CONFIG_RESET_GPIO, INT_TYPE_LEVEL, CONFIG_RESET_GPIO_EFFECT ? INT_POL_ACTIVE_HIGH : INT_POL_ACTIVE_LOW);
		rst_btn_down = 0;
	}
	else
	{
		ar7240_gpio_config_int (CONFIG_RESET_GPIO, INT_TYPE_LEVEL, CONFIG_RESET_GPIO_EFFECT ? INT_POL_ACTIVE_LOW : INT_POL_ACTIVE_HIGH);
		rst_btn_down = 1;
	}
	return IRQ_HANDLED;
}
#endif

#ifdef CONFIG_SH_TRICOLOR_LED
static struct proc_dir_entry *sh_tricolor_red_led_entry = NULL;
static struct proc_dir_entry *sh_tricolor_green_led_entry = NULL;

static int gpio_sh_tricolor_red_led_read (char *page, char **start, off_t off,
               int count, int *eof, void *data)
{
    return count;
}

static int gpio_sh_tricolor_red_led_write (struct file *file, const char *buf,
                                        unsigned long count, void *data)
{
  u_int32_t val;

    if (sscanf(buf, "%d", &val) != 1)
        return -EINVAL;

	if (val == 0)
		ar7240_gpio_out_val (CONFIG_SH_TRICOLOR_RED_LED_GPIO, !CONFIG_SH_TRICOLOR_LED_ON);
	else
		ar7240_gpio_out_val (CONFIG_SH_TRICOLOR_RED_LED_GPIO, CONFIG_SH_TRICOLOR_LED_ON);

	return count;
}

static int gpio_sh_tricolor_green_led_read (char *page, char **start, off_t off,
               int count, int *eof, void *data)
{
    return count;
}

static int gpio_sh_tricolor_green_led_write (struct file *file, const char *buf,
                                        unsigned long count, void *data)
{
	 u_int32_t val;

    if (sscanf(buf, "%d", &val) != 1)
        return -EINVAL;

	if (val == 0)
		ar7240_gpio_out_val (CONFIG_SH_TRICOLOR_GREEN_LED_GPIO, !CONFIG_SH_TRICOLOR_LED_ON);
	else
		ar7240_gpio_out_val (CONFIG_SH_TRICOLOR_GREEN_LED_GPIO, CONFIG_SH_TRICOLOR_LED_ON);

	return count;
}

#endif

#ifdef CONFIG_SH_SWITCH_LED_GPIO
static struct proc_dir_entry *sh_switch_led_entry = NULL;

static int gpio_sh_switch_led_read (char *page, char **start, off_t off,
               int count, int *eof, void *data)
{
    return count;
}

static int gpio_sh_switch_led_write (struct file *file, const char *buf,
                                        unsigned long count, void *data)
{
	 u_int32_t val;

    if (sscanf(buf, "%d", &val) != 1)
        return -EINVAL;

	if (val == 0)
		ar7240_gpio_out_val (CONFIG_SH_SWITCH_LED_GPIO, !CONFIG_SH_SWITCH_LED_ON);
	else
		ar7240_gpio_out_val (CONFIG_SH_SWITCH_LED_GPIO, CONFIG_SH_SWITCH_LED_ON);

	return count;
}
#endif

static atomic_t co_state = ATOMIC_INIT(0);

#ifdef CONFIG_SH_SWITCH_CTRL_IN_GPIO
static struct proc_dir_entry *sh_switch_ctrl_in_entry = NULL;
static int ci_presss_cnt = 0;
static int sw_btn_last = -1;
static int sw_btn_pressed = -1;
static void ctrlin_times_out(unsigned long dummy);
static DEFINE_TIMER(ctrlin_timer, ctrlin_times_out, 0, 0);

#define TIMER_CYC_10MS ((HZ/100) == 0 ? 1 : HZ/100)
#define BTN_LOW_TO_HIGH 1
#define BTN_HIGH_TO_LOW	0

static void ctrlin_times_out(unsigned long dummy)
{
	if (sw_btn_last == 1 && !ar7240_gpio_in_val(CONFIG_SH_SWITCH_CTRL_IN_GPIO)){
		mdelay(2);
		if (!ar7240_gpio_in_val(CONFIG_SH_SWITCH_CTRL_IN_GPIO))
			sw_btn_pressed = BTN_HIGH_TO_LOW;
	}
	else if (sw_btn_last == 0 && ar7240_gpio_in_val(CONFIG_SH_SWITCH_CTRL_IN_GPIO)){
		mdelay(2);
		if (ar7240_gpio_in_val(CONFIG_SH_SWITCH_CTRL_IN_GPIO))
			sw_btn_pressed = BTN_LOW_TO_HIGH;
	}
	if ((CONFIG_SH_SWITCH_CTRL_IN_ON && sw_btn_pressed == BTN_HIGH_TO_LOW) ||
		(!CONFIG_SH_SWITCH_CTRL_IN_ON && sw_btn_pressed == BTN_LOW_TO_HIGH)){
#ifdef CONFIG_SH_SWITCH_CTRL_OUT_GPIO
		if (atomic_read(&co_state)){
			atomic_set(&co_state, 0);
			ar7240_gpio_out_val (CONFIG_SH_SWITCH_CTRL_OUT_GPIO, !CONFIG_SH_SWITCH_CTRL_OUT_ON);
#ifdef CONFIG_SH_SWITCH_LED_GPIO
			if (!atomic_read(&led_off))
				ar7240_gpio_out_val (CONFIG_SH_SWITCH_LED_GPIO, !CONFIG_SH_SWITCH_LED_ON);
#endif
		}
		else{
			atomic_set(&co_state, 1);
			ar7240_gpio_out_val (CONFIG_SH_SWITCH_CTRL_OUT_GPIO, CONFIG_SH_SWITCH_CTRL_OUT_ON);
#ifdef CONFIG_SH_SWITCH_LED_GPIO
			if (!atomic_read(&led_off))
				ar7240_gpio_out_val (CONFIG_SH_SWITCH_LED_GPIO, CONFIG_SH_SWITCH_LED_ON);
#endif
		}
#endif
		ci_presss_cnt++;
	}
	sw_btn_pressed = -1;

	sw_btn_last = (ar7240_gpio_in_val(CONFIG_SH_SWITCH_CTRL_IN_GPIO) >> CONFIG_SH_SWITCH_CTRL_IN_GPIO);
	mod_timer(&ctrlin_timer, jiffies + TIMER_CYC_10MS);
}


static int gpio_sh_switch_ctrl_in_read (char *page, char **start, off_t off,
               int count, int *eof, void *data)
{
	int tmp = ci_presss_cnt;
	ci_presss_cnt = 0;
	return sprintf (page, "%d\n", tmp);
}

static int gpio_sh_switch_ctrl_in_write (struct file *file, const char *buf,
                                        unsigned long count, void *data)
{
	return count;
}

#endif

#ifdef CONFIG_SH_SWITCH_CTRL_OUT_GPIO
static struct proc_dir_entry *sh_switch_ctrl_out_entry = NULL;
static int gpio_sh_switch_ctrl_out_read (char *page, char **start, off_t off,
               int count, int *eof, void *data)
{
	return sprintf (page, "%d\n", atomic_read(&co_state));
}

static int gpio_sh_switch_ctrl_out_write (struct file *file, const char *buf,
                                        unsigned long count, void *data)
{
	 u_int32_t val;

    if (sscanf(buf, "%d", &val) != 1)
        return -EINVAL;

	if (val == 0){
		atomic_set(&co_state, 0);
		ar7240_gpio_out_val (CONFIG_SH_SWITCH_CTRL_OUT_GPIO, !CONFIG_SH_SWITCH_CTRL_OUT_ON);
	}
	else{
		atomic_set(&co_state, 1);
		ar7240_gpio_out_val (CONFIG_SH_SWITCH_CTRL_OUT_GPIO, CONFIG_SH_SWITCH_CTRL_OUT_ON);
	}

	return count;
}
#endif

static struct proc_dir_entry *sh_reboot_entry = NULL;

static int gpio_sh_reboot_read (char *page, char **start, off_t off,
               int count, int *eof, void *data)
{
	return sprintf (page, "0\n");
}

static int gpio_sh_reboot_write (struct file *file, const char *buf,
                                        unsigned long count, void *data)
{
	ar7240_reg_wr(AR7240_WATCHDOG_TMR, 0x80000000);
	ar7240_reg_wr(AR7240_WATCHDOG_TMR_CONTROL, 0x3);
	ar7240_reg_wr(AR7240_WATCHDOG_TMR, 0x2);

	return 0;
}

static int create_smart_home_proc_entry (void)
{
	int req;

    if (smart_home_entry != NULL) {
        printk ("Already have a proc entry for /proc/smart_home\n");
        return -ENOENT;
    }

    smart_home_entry = proc_mkdir("smart_home", NULL) ;
    if (!smart_home_entry)
        return -ENOENT;

	sh_led_off_entry = create_proc_entry ("led_off", 0644,
                                                      smart_home_entry);
	if (!sh_led_off_entry)
    	return -ENOENT;
    sh_led_off_entry->write_proc = gpio_sh_led_off_write;
    sh_led_off_entry->read_proc = gpio_sh_led_off_read;
#ifdef CONFIG_RESET_GPIO
	sh_reset_entry = create_proc_entry ("reset", 0644,
                                                      smart_home_entry);
    if (!sh_reset_entry)
        return -ENOENT;
    sh_reset_entry->write_proc = gpio_sh_reset_write;
    sh_reset_entry->read_proc = gpio_sh_reset_read;
	ar7240_gpio_config_input(CONFIG_RESET_GPIO);
    ar7240_gpio_config_int(CONFIG_RESET_GPIO, INT_TYPE_LEVEL, CONFIG_RESET_GPIO_EFFECT ? INT_POL_ACTIVE_HIGH : INT_POL_ACTIVE_LOW);
	req = request_irq (AR7240_GPIO_IRQn(CONFIG_RESET_GPIO), reset_irq, 0,
						  "switch_ctrl_in", NULL);
	if (req != 0) {
	   printk (KERN_ERR "unable to request IRQ for switch_ctrl_in GPIO (error %d)\n", req);
	   ar7240_gpio_intr_shutdown(AR7240_GPIO_IRQn(CONFIG_RESET_GPIO));
	   return -1;
	}
#endif

#ifdef CONFIG_SH_TRICOLOR_LED
	sh_tricolor_red_led_entry = create_proc_entry ("tricolor_red", 0644,
                                                      smart_home_entry);
    if (!sh_tricolor_red_led_entry)
        return -ENOENT;
    sh_tricolor_red_led_entry->write_proc = gpio_sh_tricolor_red_led_write;
    sh_tricolor_red_led_entry->read_proc = gpio_sh_tricolor_red_led_read;

	sh_tricolor_green_led_entry = create_proc_entry ("tricolor_green", 0644,
                                                      smart_home_entry);
    if (!sh_tricolor_green_led_entry)
        return -ENOENT;
    sh_tricolor_green_led_entry->write_proc = gpio_sh_tricolor_green_led_write;
    sh_tricolor_green_led_entry->read_proc = gpio_sh_tricolor_green_led_read;

    ar7240_gpio_config_output (CONFIG_SH_TRICOLOR_RED_LED_GPIO);
	ar7240_gpio_config_output (CONFIG_SH_TRICOLOR_GREEN_LED_GPIO);
	if (CONFIG_SH_TRICOLOR_REAL_ORANGE == 1)
	{
		ar7240_gpio_out_val(CONFIG_SH_TRICOLOR_RED_LED_GPIO, CONFIG_SH_TRICOLOR_LED_ON);
		ar7240_gpio_out_val(CONFIG_SH_TRICOLOR_GREEN_LED_GPIO, !CONFIG_SH_TRICOLOR_LED_ON);
	}
	else
	{
		ar7240_gpio_out_val(CONFIG_SH_TRICOLOR_RED_LED_GPIO, CONFIG_SH_TRICOLOR_LED_ON);
		ar7240_gpio_out_val(CONFIG_SH_TRICOLOR_GREEN_LED_GPIO, CONFIG_SH_TRICOLOR_LED_ON);
	}

#endif

#ifdef CONFIG_SH_SWITCH_LED_GPIO
    sh_switch_led_entry = create_proc_entry ("switch_led", 0644,
                                                      smart_home_entry);
    if (!sh_switch_led_entry)
        return -ENOENT;

    sh_switch_led_entry->write_proc = gpio_sh_switch_led_write;
    sh_switch_led_entry->read_proc = gpio_sh_switch_led_read;
    ar7240_gpio_config_output (CONFIG_SH_SWITCH_LED_GPIO);
    ar7240_gpio_out_val(CONFIG_SH_SWITCH_LED_GPIO, !CONFIG_SH_SWITCH_LED_ON);
#endif

#ifdef CONFIG_SH_SWITCH_CTRL_IN_GPIO
	sh_switch_ctrl_in_entry = create_proc_entry ("switch_ctrl_in", 0644,
                                                      smart_home_entry);
    if (!sh_switch_ctrl_in_entry)
        return -ENOENT;
    sh_switch_ctrl_in_entry->write_proc = gpio_sh_switch_ctrl_in_write;
    sh_switch_ctrl_in_entry->read_proc = gpio_sh_switch_ctrl_in_read;
	ar7240_reg_rmw_clear(AR7240_GPIO_FUNCTIONS, 0xF8);
	ar7240_gpio_config_input(CONFIG_SH_SWITCH_CTRL_IN_GPIO);
	mod_timer(&ctrlin_timer, jiffies + TIMER_CYC_10MS);
#endif

#ifdef CONFIG_SH_SWITCH_CTRL_OUT_GPIO
    sh_switch_ctrl_out_entry = create_proc_entry ("switch_ctrl_out", 0644,
                                                      smart_home_entry);
    if (!sh_switch_ctrl_out_entry)
        return -ENOENT;
    sh_switch_ctrl_out_entry->write_proc = gpio_sh_switch_ctrl_out_write;
    sh_switch_ctrl_out_entry->read_proc = gpio_sh_switch_ctrl_out_read;

    ar7240_gpio_config_output (CONFIG_SH_SWITCH_CTRL_OUT_GPIO);
    ar7240_gpio_out_val(CONFIG_SH_SWITCH_CTRL_OUT_GPIO, 0);
#endif

	sh_reboot_entry = create_proc_entry ("reboot", 0644,
                                                      smart_home_entry);
    if (!sh_reboot_entry)
        return -ENOENT;
    sh_reboot_entry->write_proc = gpio_sh_reboot_write;
    sh_reboot_entry->read_proc = gpio_sh_reboot_read;

	return 0;
}
*/
static void __init hornet_ub_setup(void)
{
	u8 *art = (u8 *) KSEG1ADDR(0x1fff0000);
	
	//hornet_ub_gpio_setup();

	ath79_register_m25p80(NULL);
	/*ath79_register_leds_gpio(-1, ARRAY_SIZE(hornet_ub_leds_gpio),
					hornet_ub_leds_gpio);
	ath79_register_gpio_keys_polled(-1, HORNET_UB_KEYS_POLL_INTERVAL,
					 ARRAY_SIZE(hornet_ub_gpio_keys),
					 hornet_ub_gpio_keys);
*/
	ath79_init_mac(ath79_eth1_data.mac_addr,
			art + HORNET_UB_MAC0_OFFSET, 0);
	ath79_init_mac(ath79_eth0_data.mac_addr,
			art + HORNET_UB_MAC1_OFFSET, 0);

	ath79_register_mdio(0, 0x0);

	ath79_register_eth(1);
	ath79_register_eth(0);

    art = (u8 *) KSEG1ADDR(0x1f000000);
    printk(">>>>>>>>>>> flash base %x%x%x%x%x%x%x%x",
            art[0],
            art[1],
            art[2],
            art[3],
            art[4],
            art[5],
            art[6],
            art[7]);
    art += 0x3f0000;
    printk(">>>>>>>>>>> art partition %x%x%x%x%x%x%x%x",
            art[0],
            art[1],
            art[2],
            art[3],
            art[4],
            art[5],
            art[6],
            art[7]);
    art += 0x1000;
    printk(">>>>>>>>>>> with offset  %x%x%x%x%x%x%x%x",
            art[0],
            art[1],
            art[2],
            art[3],
            art[4],
            art[5],
            art[6],
            art[7]);

	ath79_register_wmac(art, NULL);
	ath79_register_usb();

//	create_smart_home_proc_entry();
}

MIPS_MACHINE(ATH79_MACH_TL_HS110, "TL-HS110", "TP-LINK HS110",
	     hornet_ub_setup);
