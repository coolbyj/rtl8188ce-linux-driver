/******************************************************************************
 *
 * Copyright(c) 2009-2010  Realtek Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110, USA
 *
 * The full GNU General Public License is included in this distribution in the
 * file called LICENSE.
 *
 * Contact Information:
 * wlanfae <wlanfae@realtek.com>
 * Realtek Corporation, No. 2, Innovation Road II, Hsinchu Science Park,
 * Hsinchu 300, Taiwan.
 *
 * Larry Finger <Larry.Finger@lwfinger.net>
 *
 *
 * Bug Fixes and enhancements for Linux Kernels >= 3.2
 * by Benjamin Porter <BenjaminPorter86@gmail.com>
 *
 * Project homepage: https://github.com/FreedomBen/rtl8188ce-linux-driver
 *
 *
 *****************************************************************************/

#include <linux/vmalloc.h>
#include <linux/module.h>

#include "../wifi.h"
#include "../core.h"
#include "../pci.h"
#include "reg.h"
#include "def.h"
#include "phy.h"
#include "dm.h"
#include "hw.h"
#include "sw.h"
#include "trx.h"
#include "led.h"

void rtl92c_init_aspm_vars(struct ieee80211_hw *hw)
{
	struct rtl_pci *rtlpci = rtl_pcidev(rtl_pcipriv(hw));

	/*close ASPM for AMD defaultly */
	rtlpci->const_amdpci_aspm = 0;

	/*
	 * ASPM PS mode.
	 * 0 - Disable ASPM,
	 * 1 - Enable ASPM without Clock Req,
	 * 2 - Enable ASPM with Clock Req,
	 * 3 - Alwyas Enable ASPM with Clock Req,
	 * 4 - Always Enable ASPM without Clock Req.
	 * set defult to RTL8192CE:3 RTL8192E:2
	 * */
	rtlpci->const_pci_aspm = 3;

	/*Setting for PCI-E device */
	rtlpci->const_devicepci_aspm_setting = 0x03;

	/*Setting for PCI-E bridge */
	rtlpci->const_hostpci_aspm_setting = 0x02;

	/*
	 * In Hw/Sw Radio Off situation.
	 * 0 - Default,
	 * 1 - From ASPM setting without low Mac Pwr,
	 * 2 - From ASPM setting with low Mac Pwr,
	 * 3 - Bus D3
	 * set default to RTL8192CE:0 RTL8192SE:2
	 */
	rtlpci->const_hwsw_rfoff_d3 = 0;

	/*
	 * This setting works for those device with
	 * backdoor ASPM setting such as EPHY setting.
	 * 0 - Not support ASPM,
	 * 1 - Support ASPM,
	 * 2 - According to chipset.
	 */
	rtlpci->const_support_pciaspm = 1;
}

int rtl92c_init_sw_vars(struct ieee80211_hw *hw)
{
	int err = 0;
	struct rtl_priv *rtlpriv = rtl_priv(hw);
	struct rtl_pci *rtlpci = rtl_pcidev(rtl_pcipriv(hw));
	const struct firmware *firmware;
	struct rtl_hal *rtlhal = rtl_hal(rtl_priv(hw));
	char *fw_name = NULL;

	rtl8192ce_bt_reg_init(hw);

	rtlpriv->dm.b_dm_initialgain_enable = 1;
	rtlpriv->dm.dm_flag = 0;
	rtlpriv->dm.b_disable_framebursting = 0;;
	rtlpriv->dm.thermalvalue = 0;
	rtlpci->transmit_config = CFENDFORM | BIT(12) | BIT(13);

	/* compatible 5G band 88ce just 2.4G band & smsp */
	rtlpriv->rtlhal.current_bandtype = BAND_ON_2_4G;
	rtlpriv->rtlhal.bandset = BAND_ON_2_4G;
	rtlpriv->rtlhal.macphymode = SINGLEMAC_SINGLEPHY;

	rtlpci->receive_config = (RCR_APPFCS |
				  RCR_AMF |
				  RCR_ADF |
				  RCR_APP_MIC |
				  RCR_APP_ICV |
				  RCR_AICV |
				  RCR_ACRC32 |
				  RCR_AB |
				  RCR_AM |
				  RCR_APM |
				  RCR_APP_PHYST_RXFF | RCR_HTC_LOC_CTRL | 0);

	rtlpci->irq_mask[0] =
	    (u32) (IMR_ROK |
		   IMR_VODOK |
		   IMR_VIDOK |
		   IMR_BEDOK |
		   IMR_BKDOK |
		   IMR_MGNTDOK |
		   IMR_HIGHDOK | IMR_BDOK | IMR_RDU | IMR_RXFOVW | 0);

	rtlpci->irq_mask[1] = (u32) (IMR_CPWM | IMR_C2HCMD | 0);

	/* for LPS & IPS */
	rtlpriv->psc.b_inactiveps = rtlpriv->cfg->mod_params->b_inactiveps;
	rtlpriv->psc.b_swctrl_lps = rtlpriv->cfg->mod_params->b_swctrl_lps;
	rtlpriv->psc.b_fwctrl_lps = rtlpriv->cfg->mod_params->b_fwctrl_lps;
	rtlpriv->psc.b_reg_fwctrl_lps = 3;
	rtlpriv->psc.reg_max_lps_awakeintvl = 5;
	/* for ASPM, you can close aspm through
	 * set const_support_pciaspm = 0 */
	rtl92c_init_aspm_vars(hw);

	if (rtlpriv->psc.b_reg_fwctrl_lps == 1)
		rtlpriv->psc.fwctrl_psmode = FW_PS_MIN_MODE;
	else if (rtlpriv->psc.b_reg_fwctrl_lps == 2)
		rtlpriv->psc.fwctrl_psmode = FW_PS_MAX_MODE;
	else if (rtlpriv->psc.b_reg_fwctrl_lps == 3)
		rtlpriv->psc.fwctrl_psmode = FW_PS_DTIM_MODE;

	/* for firmware buf */
	rtlpriv->rtlhal.pfirmware = (u8 *) vmalloc(0x4000);
	if (!rtlpriv->rtlhal.pfirmware) {
		RT_TRACE(COMP_ERR, DBG_EMERG,
			 ("Can't alloc buffer for fw.\n"));
		return 1;
	}

	if (IS_VENDOR_UMC_A_CUT(rtlhal->version) && !IS_92C_SERIAL(rtlhal->version))
		fw_name = "rtlwifi/rtl8192cfwU.bin";
	else if (IS_81xxC_VENDOR_UMC_B_CUT(rtlhal->version))
		fw_name = "rtlwifi/rtl8192cfwU_B.bin";
	else
		fw_name = rtlpriv->cfg->fw_name;
	err = request_firmware(&firmware, fw_name, rtlpriv->io.dev);

	if (err) {
		RT_TRACE(COMP_ERR, DBG_EMERG,
			 ("Failed to request firmware!\n"));
		return 1;
	}
	if (firmware->size > 0x4000) {
		RT_TRACE(COMP_ERR, DBG_EMERG,
			 ("Firmware is too big!\n"));
		release_firmware(firmware);
		return 1;
	}
	memcpy(rtlpriv->rtlhal.pfirmware, firmware->data, firmware->size);
	rtlpriv->rtlhal.fwsize = firmware->size;
	release_firmware(firmware);

	return err;
}

void rtl92c_deinit_sw_vars(struct ieee80211_hw *hw)
{
	struct rtl_priv *rtlpriv = rtl_priv(hw);

	if (rtlpriv->rtlhal.pfirmware) {
		vfree(rtlpriv->rtlhal.pfirmware);
		rtlpriv->rtlhal.pfirmware = NULL;
	}
}

struct rtl_hal_ops rtl8192ce_hal_ops = {
	.init_sw_vars = rtl92c_init_sw_vars,
	.deinit_sw_vars = rtl92c_deinit_sw_vars,
	.read_eeprom_info = rtl92ce_read_eeprom_info,
	.interrupt_recognized = rtl92ce_interrupt_recognized,
	.hw_init = rtl92ce_hw_init,
	.hw_disable = rtl92ce_card_disable,
	.hw_suspend = rtl92ce_suspend,
	.hw_resume = rtl92ce_resume,
	.enable_interrupt = rtl92ce_enable_interrupt,
	.disable_interrupt = rtl92ce_disable_interrupt,
	.set_network_type = rtl92ce_set_network_type,
	.set_chk_bssid = rtl92ce_set_check_bssid,
	.set_qos = rtl92ce_set_qos,
	.set_bcn_reg = rtl92ce_set_beacon_related_registers,
	.set_bcn_intv = rtl92ce_set_beacon_interval,
	.update_interrupt_mask = rtl92ce_update_interrupt_mask,
	.get_hw_reg = rtl92ce_get_hw_reg,
	.set_hw_reg = rtl92ce_set_hw_reg,
	.update_rate_tbl = rtl92ce_update_hal_rate_tbl,
	.fill_tx_desc = rtl92ce_tx_fill_desc,
	.fill_tx_cmddesc = rtl92ce_tx_fill_cmddesc,
	.query_rx_desc = rtl92ce_rx_query_desc,
	.set_channel_access = rtl92ce_update_channel_access_setting,
	.radio_onoff_checking = rtl92ce_gpio_radio_on_off_checking,
	.set_bw_mode = rtl92c_phy_set_bw_mode,
	.switch_channel = rtl92c_phy_sw_chnl,
	.dm_watchdog = rtl92c_dm_watchdog,
	.scan_operation_backup = rtl92c_phy_scan_operation_backup,
	.set_rf_power_state = rtl92c_phy_set_rf_power_state,
	.led_control = rtl92ce_led_control,
	.set_desc = rtl92ce_set_desc,
	.get_desc = rtl92ce_get_desc,
	.tx_polling = rtl92ce_tx_polling,
	.enable_hw_sec = rtl92ce_enable_hw_security_config,
	.set_key = rtl92ce_set_key,
	.init_sw_leds = rtl92ce_init_sw_leds,
	.allow_all_destaddr = rtl92ce_allow_all_destaddr,
	.get_bbreg = rtl92c_phy_query_bb_reg,
	.set_bbreg = rtl92c_phy_set_bb_reg,
	.get_rfreg = rtl92c_phy_query_rf_reg,
	.set_rfreg = rtl92c_phy_set_rf_reg,
};

struct rtl_mod_params rtl92ce_mod_params = {
	.sw_crypto = false,
	.b_inactiveps = true,
	.b_swctrl_lps = false,
	.b_fwctrl_lps = true,
};

struct rtl_hal_cfg rtl92ce_hal_cfg = {
	.bar_id = 2,
	.write_readback = true,
	.name = "rtl92c_pci",
	.fw_name = "rtlwifi/rtl8192cfw.bin",
	.ops = &rtl8192ce_hal_ops,
	.mod_params = &rtl92ce_mod_params,

	.maps[SYS_ISO_CTRL] = REG_SYS_ISO_CTRL,
	.maps[SYS_FUNC_EN] = REG_SYS_FUNC_EN,
	.maps[SYS_CLK] = REG_SYS_CLKR,
	.maps[MAC_RCR_AM] = AM,
	.maps[MAC_RCR_AB] = AB,
	.maps[MAC_RCR_ACRC32] = ACRC32,
	.maps[MAC_RCR_ACF] = ACF,
	.maps[MAC_RCR_AAP] = AAP,

	.maps[EFUSE_TEST] = REG_EFUSE_TEST,
	.maps[EFUSE_CTRL] = REG_EFUSE_CTRL,
	.maps[EFUSE_CLK] = 0,
	.maps[EFUSE_CLK_CTRL] = REG_EFUSE_CTRL,
	.maps[EFUSE_PWC_EV12V] = PWC_EV12V,
	.maps[EFUSE_FEN_ELDR] = FEN_ELDR,
	.maps[EFUSE_LOADER_CLK_EN] = LOADER_CLK_EN,
	.maps[EFUSE_ANA8M] = EFUSE_ANA8M,
	.maps[EFUSE_HWSET_MAX_SIZE] = HWSET_MAX_SIZE,
	.maps[EFUSE_MAX_SECTION_MAP] = EFUSE_MAX_SECTION,
	.maps[EFUSE_REAL_CONTENT_SIZE] = EFUSE_REAL_CONTENT_LEN,
	.maps[EFUSE_OOB_PROTECT_BYTES_LEN] = EFUSE_OOB_PROTECT_BYTES,

	.maps[RWCAM] = REG_CAMCMD,
	.maps[WCAMI] = REG_CAMWRITE,
	.maps[RCAMO] = REG_CAMREAD,
	.maps[CAMDBG] = REG_CAMDBG,
	.maps[SECR] = REG_SECCFG,
	.maps[SEC_CAM_NONE] = CAM_NONE,
	.maps[SEC_CAM_WEP40] = CAM_WEP40,
	.maps[SEC_CAM_TKIP] = CAM_TKIP,
	.maps[SEC_CAM_AES] = CAM_AES,
	.maps[SEC_CAM_WEP104] = CAM_WEP104,

	.maps[RTL_IMR_BCNDMAINT6] = IMR_BCNDMAINT6,
	.maps[RTL_IMR_BCNDMAINT5] = IMR_BCNDMAINT5,
	.maps[RTL_IMR_BCNDMAINT4] = IMR_BCNDMAINT4,
	.maps[RTL_IMR_BCNDMAINT3] = IMR_BCNDMAINT3,
	.maps[RTL_IMR_BCNDMAINT2] = IMR_BCNDMAINT2,
	.maps[RTL_IMR_BCNDMAINT1] = IMR_BCNDMAINT1,
	.maps[RTL_IMR_BCNDOK8] = IMR_BCNDOK8,
	.maps[RTL_IMR_BCNDOK7] = IMR_BCNDOK7,
	.maps[RTL_IMR_BCNDOK6] = IMR_BCNDOK6,
	.maps[RTL_IMR_BCNDOK5] = IMR_BCNDOK5,
	.maps[RTL_IMR_BCNDOK4] = IMR_BCNDOK4,
	.maps[RTL_IMR_BCNDOK3] = IMR_BCNDOK3,
	.maps[RTL_IMR_BCNDOK2] = IMR_BCNDOK2,
	.maps[RTL_IMR_BCNDOK1] = IMR_BCNDOK1,
	.maps[RTL_IMR_TIMEOUT2] = IMR_TIMEOUT2,
	.maps[RTL_IMR_TIMEOUT1] = IMR_TIMEOUT1,

	.maps[RTL_IMR_TXFOVW] = IMR_TXFOVW,
	.maps[RTL_IMR_PSTIMEOUT] = IMR_PSTIMEOUT,
	.maps[RTL_IMR_BcnInt] = IMR_BCNINT,
	.maps[RTL_IMR_RXFOVW] = IMR_RXFOVW,
	.maps[RTL_IMR_RDU] = IMR_RDU,
	.maps[RTL_IMR_ATIMEND] = IMR_ATIMEND,
	.maps[RTL_IMR_BDOK] = IMR_BDOK,
	.maps[RTL_IMR_MGNTDOK] = IMR_MGNTDOK,
	.maps[RTL_IMR_TBDER] = IMR_TBDER,
	.maps[RTL_IMR_HIGHDOK] = IMR_HIGHDOK,
	.maps[RTL_IMR_TBDOK] = IMR_TBDOK,
	.maps[RTL_IMR_BKDOK] = IMR_BKDOK,
	.maps[RTL_IMR_BEDOK] = IMR_BEDOK,
	.maps[RTL_IMR_VIDOK] = IMR_VIDOK,
	.maps[RTL_IMR_VODOK] = IMR_VODOK,
	.maps[RTL_IMR_ROK] = IMR_ROK,
	.maps[RTL_IBSS_INT_MASKS] = (IMR_BCNINT | IMR_TBDOK | IMR_TBDER),

	.maps[RTL_RC_CCK_RATE1M] = DESC92C_RATE1M,
	.maps[RTL_RC_CCK_RATE2M] = DESC92C_RATE2M,
	.maps[RTL_RC_CCK_RATE5_5M] = DESC92C_RATE5_5M,
	.maps[RTL_RC_CCK_RATE11M] = DESC92C_RATE11M,
	.maps[RTL_RC_OFDM_RATE6M] = DESC92C_RATE6M,
	.maps[RTL_RC_OFDM_RATE9M] = DESC92C_RATE9M,
	.maps[RTL_RC_OFDM_RATE12M] = DESC92C_RATE12M,
	.maps[RTL_RC_OFDM_RATE18M] = DESC92C_RATE18M,
	.maps[RTL_RC_OFDM_RATE24M] = DESC92C_RATE24M,
	.maps[RTL_RC_OFDM_RATE36M] = DESC92C_RATE36M,
	.maps[RTL_RC_OFDM_RATE48M] = DESC92C_RATE48M,
	.maps[RTL_RC_OFDM_RATE54M] = DESC92C_RATE54M,

	.maps[RTL_RC_HT_RATEMCS7] = DESC92C_RATEMCS7,
	.maps[RTL_RC_HT_RATEMCS15] = DESC92C_RATEMCS15,
};

static struct pci_device_id rtl92ce_pci_ids[] __devinitdata = {
	{RTL_PCI_DEVICE(PCI_VENDOR_ID_REALTEK, 0x8191, rtl92ce_hal_cfg)},
	{RTL_PCI_DEVICE(PCI_VENDOR_ID_REALTEK, 0x8178, rtl92ce_hal_cfg)},
	{RTL_PCI_DEVICE(PCI_VENDOR_ID_REALTEK, 0x8177, rtl92ce_hal_cfg)},
	{RTL_PCI_DEVICE(PCI_VENDOR_ID_REALTEK, 0x8176, rtl92ce_hal_cfg)},
	{},
};

MODULE_DEVICE_TABLE(pci, rtl92ce_pci_ids);

MODULE_AUTHOR("Benjamin Porter	<BenjaminPorter86@gmail.com>");
MODULE_AUTHOR("lizhaoming	<chaoming_li@realsil.com.cn>");
MODULE_AUTHOR("Realtek WlanFAE	<wlanfae@realtek.com>");
MODULE_AUTHOR("Larry Finger	<Larry.Finger@lwfinger.net>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Realtek 8192C/8188C 802.11n PCI wireless");
MODULE_FIRMWARE("rtlwifi/rtl8192cfw.bin");

module_param_named(swenc, rtl92ce_mod_params.sw_crypto, bool, 0444);
module_param_named(ips, rtl92ce_mod_params.b_inactiveps, bool, 0444);
module_param_named(swlps, rtl92ce_mod_params.b_swctrl_lps, bool, 0444);
module_param_named(fwlps, rtl92ce_mod_params.b_fwctrl_lps, bool, 0444);
MODULE_PARM_DESC(swenc, "using hardware crypto (default 0 [hardware])\n");
MODULE_PARM_DESC(ips, "using no link power save (default 1 is open)\n");
MODULE_PARM_DESC(fwlps, "using linked fw control power save (default 1 is open)\n");

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,29))
static const SIMPLE_DEV_PM_OPS(rtlwifi_pm_ops, rtl_pci_suspend, rtl_pci_resume);
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,29))
compat_pci_suspend(rtl_pci_suspend)
compat_pci_resume(rtl_pci_resume)
#endif

static struct pci_driver rtl92ce_driver = {
	.name = KBUILD_MODNAME,
	.id_table = rtl92ce_pci_ids,
	.probe = rtl_pci_probe,
	.remove = rtl_pci_disconnect,

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,29))
	.driver.pm = &rtlwifi_pm_ops,
#elif defined(CONFIG_PM)
	.suspend = rtl_pci_suspend_compat,
	.resume = rtl_pci_resume_compat,
#endif

};

static int __init rtl92ce_module_init(void)
{
	int ret;

	ret = pci_register_driver(&rtl92ce_driver);
	if (ret)
		RT_ASSERT(false, (": No device found\n"));

	return ret;
}

static void __exit rtl92ce_module_exit(void)
{
	pci_unregister_driver(&rtl92ce_driver);
}

module_init(rtl92ce_module_init);
module_exit(rtl92ce_module_exit);
