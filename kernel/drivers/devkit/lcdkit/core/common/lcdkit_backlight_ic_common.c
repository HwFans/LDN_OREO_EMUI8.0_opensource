#include "lcdkit_bias_bl_utility.h"
#include "lcdkit_backlight_ic_common.h"
#include "lcdkit_dbg.h"

#if defined (CONFIG_HUAWEI_DSM)
#include <dsm/dsm_pub.h>
extern struct dsm_client *lcd_dclient;
#define OVP_FAULT_BIT_SET     0x01
#define OCP_FAULT_BIT_SET     0x02
#define TSD_FAULT_BIT_SET     0x04
#endif
#define  NUMOFELEMENT 4
static struct lcdkit_bl_ic_device * plcdkit_bl_ic = NULL;
static char chip_name[LCD_BACKLIGHT_IC_NAME_LEN] = "default";
static struct lcdkit_bl_ic_info g_bl_config = {0};
struct class *backlight_class = NULL;
static bool bl_ic_init_status = false;

extern void hisi_blpwm_bl_regisiter(int (*set_bl)(int bl_level));

static int lcdkit_backlight_ic_read_byte(struct lcdkit_bl_ic_device *pbl_ic, u8 reg, u8 *pdata)
{
    int ret = 0;	
    ret = i2c_smbus_read_byte_data(pbl_ic->client, reg);
    if(ret < 0)
    {
        dev_err(&pbl_ic->client->dev, "failed to read 0x%x\n", reg);
        return ret;
    }
    *pdata = (u8)ret;

    return ret;
}

static int lcdkit_backlight_ic_write_byte(struct lcdkit_bl_ic_device *pbl_ic, u8 reg, u8 data)
{
    int ret = 0;
    ret = i2c_smbus_write_byte_data(pbl_ic->client, reg, data);
	
    if(ret < 0)
    {
        dev_err(&pbl_ic->client->dev, "failed to write 0x%.2x\n", reg);
    }
	
    return ret;
}

static int lcdkit_backlight_ic_update_bit(struct lcdkit_bl_ic_device *pbl_ic, u8 reg, u8 mask, u8 data)
{
    int ret = 0;
    unsigned char tmp = 0;

    ret = lcdkit_backlight_ic_read_byte(pbl_ic, reg, &tmp);
    if (ret < 0)
    {
        dev_err(&pbl_ic->client->dev, "failed to read 0x%.2x\n", reg);
        return ret;
    }

    tmp = (unsigned char)ret;
    tmp &= ~mask;
    tmp |= data & mask;

    return lcdkit_backlight_ic_write_byte(pbl_ic, reg, tmp);
}

int lcdkit_backlight_ic_inital(void)
{
    int ret = 0;
    int i = 0;
    printk("lcd_backlight_ic_inital\n");
    if(plcdkit_bl_ic == NULL)
    {
        return -1;
    }
	
    for(i=0; i< plcdkit_bl_ic->bl_config.num_of_init_cmds; i++)
    {
        switch(plcdkit_bl_ic->bl_config.init_cmds[i].ops_type)
        {
            case 0:
                ret = lcdkit_backlight_ic_read_byte(plcdkit_bl_ic, plcdkit_bl_ic->bl_config.init_cmds[i].cmd_reg, &(plcdkit_bl_ic->bl_config.init_cmds[i].cmd_val));
                break;
            case 1:
                ret = lcdkit_backlight_ic_write_byte(plcdkit_bl_ic, plcdkit_bl_ic->bl_config.init_cmds[i].cmd_reg, plcdkit_bl_ic->bl_config.init_cmds[i].cmd_val);
                break;
            case 2:
                ret = lcdkit_backlight_ic_update_bit(plcdkit_bl_ic, plcdkit_bl_ic->bl_config.init_cmds[i].cmd_reg, plcdkit_bl_ic->bl_config.init_cmds[i].cmd_mask, plcdkit_bl_ic->bl_config.init_cmds[i].cmd_val);
                break;
            default:
                break;
        }
        if(ret < 0)
        {
            printk("operation  reg 0x%x failed!\n",plcdkit_bl_ic->bl_config.init_cmds[i].cmd_reg);
            return ret;
        }
    }

    if(plcdkit_bl_ic->bl_config.ic_init_delay)
    {
        mdelay(plcdkit_bl_ic->bl_config.ic_init_delay);
    }

    return ret;
}

int lcdkit_backlight_ic_set_brightness(unsigned int level)
{
    unsigned char level_lsb = 0;
    unsigned char level_msb = 0;
    int ret = 0;
	
    if(plcdkit_bl_ic == NULL)
    {
        return -1;
    }
    if(!bl_ic_init_status)
    {
        LCDKIT_ERR("backlight IC not init!\n");
        return 0;
    }

    if (down_trylock(&(plcdkit_bl_ic->test_sem)))
    {
        LCDKIT_ERR("Now in test mode\n");
        return 0;
    }
    level_lsb = level & plcdkit_bl_ic->bl_config.bl_lsb_reg_cmd.cmd_mask;
    level_msb = (level >> plcdkit_bl_ic->bl_config.bl_lsb_reg_cmd.val_bits)&plcdkit_bl_ic->bl_config.bl_msb_reg_cmd.cmd_mask;
    printk("level_lsb is 0x%x  level_msb is 0x%x\n",level_lsb,level_msb);

    if(plcdkit_bl_ic->bl_config.bl_lsb_reg_cmd.val_bits != 0)
    {
        ret = lcdkit_backlight_ic_write_byte(plcdkit_bl_ic, plcdkit_bl_ic->bl_config.bl_lsb_reg_cmd.cmd_reg, level_lsb);
        if(ret < 0)
        {
            printk("set backlight ic brightness failed!\n");
            up(&(plcdkit_bl_ic->test_sem));
            return ret;
        }
    }
    ret = lcdkit_backlight_ic_write_byte(plcdkit_bl_ic, plcdkit_bl_ic->bl_config.bl_msb_reg_cmd.cmd_reg, level_msb);
    if(ret < 0)
    {
        printk("set backlight ic brightness failed!\n");
    }
    up(&(plcdkit_bl_ic->test_sem));
    return ret;
}

int lcdkit_backlight_ic_enable_brightness(void)
{
    int ret = 0;
    printk("lcdkit_backlight_ic_enable_brightness\n");
    if(plcdkit_bl_ic == NULL)
    {
        return -1;
    }
	
    ret = lcdkit_backlight_ic_update_bit(plcdkit_bl_ic, plcdkit_bl_ic->bl_config.bl_enable_cmd.cmd_reg, plcdkit_bl_ic->bl_config.bl_enable_cmd.cmd_mask, plcdkit_bl_ic->bl_config.bl_enable_cmd.cmd_val);
    if(ret < 0)
    {
        printk("enable backlight ic brightness failed!\n");
    }

    return ret;
}

int lcdkit_backlight_ic_disable_brightness(void)
{
    int ret = 0;
    printk("lcdkit_backlight_ic_disable_brightness\n");
    if(plcdkit_bl_ic == NULL)
    {
        return -1;
    }
	
    ret = lcdkit_backlight_ic_update_bit(plcdkit_bl_ic, plcdkit_bl_ic->bl_config.bl_disable_cmd.cmd_reg, plcdkit_bl_ic->bl_config.bl_disable_cmd.cmd_mask, plcdkit_bl_ic->bl_config.bl_disable_cmd.cmd_val);
    if(ret < 0)
    {
        printk("disable backlight ic brightness failed!\n");
    }

    return ret;
}
int lcdkit_backlight_ic_disable_device(void)
{
    int ret = 0;
    printk("lcdkit_backlight_ic_disable_device\n");
    if(plcdkit_bl_ic == NULL)
    {
        return -1;
    }
    ret = lcdkit_backlight_ic_update_bit(plcdkit_bl_ic, plcdkit_bl_ic->bl_config.disable_dev_cmd.cmd_reg, plcdkit_bl_ic->bl_config.disable_dev_cmd.cmd_mask, plcdkit_bl_ic->bl_config.disable_dev_cmd.cmd_val);
    if(ret < 0)
    {
        printk("disable backlight ic device failed!\n");
    }
    return ret;
}
int lcdkit_backlight_ic_fault_check(unsigned char *pval)
{
    int ret = 0;
    unsigned char val = 0;
    printk("lcdkit_backlight_ic_fault_check\n");
    if(plcdkit_bl_ic == NULL)
    {
        return -1;
    }
    ret = lcdkit_backlight_ic_read_byte(plcdkit_bl_ic, plcdkit_bl_ic->bl_config.bl_fault_flag_cmd.cmd_reg, &val);
    if(ret < 0)
    {
        printk("read backlight ic fault reg failed!\n");
        return -1;
    }
    if((val & plcdkit_bl_ic->bl_config.bl_fault_flag_cmd.cmd_mask) != plcdkit_bl_ic->bl_config.bl_fault_flag_cmd.cmd_val)
    {
        *pval = val;
        return 1;
    }
    return 0;
}

int lcdkit_backlight_ic_bias(bool enable)
{
    int ret = 0;
    printk("lcdkit_backlight_ic_bias enable is %d\n",enable);
    if(plcdkit_bl_ic == NULL)
    {
        return -1;
    }
    if(enable)
    {
        ret = lcdkit_backlight_ic_update_bit(plcdkit_bl_ic, plcdkit_bl_ic->bl_config.bias_enable_cmd.cmd_reg, plcdkit_bl_ic->bl_config.bias_enable_cmd.cmd_mask, plcdkit_bl_ic->bl_config.bias_enable_cmd.cmd_val);
        if(ret < 0)
        {
            printk("disable backlight ic enable bias failed!\n");
            return ret;
        }
    }
    else
    {
        ret = lcdkit_backlight_ic_update_bit(plcdkit_bl_ic, plcdkit_bl_ic->bl_config.bias_disable_cmd.cmd_reg, plcdkit_bl_ic->bl_config.bias_disable_cmd.cmd_mask, plcdkit_bl_ic->bl_config.bias_disable_cmd.cmd_val);
        if(ret < 0)
        {
            printk("disable backlight ic disable bias failed!\n");
            return ret;
        }
    }
    return 0;
}

void lcdkit_backlight_ic_get_chip_name(char *pname)
{
    if(NULL == pname)
    {
        return;
    }
    memcpy(pname,chip_name,strlen(chip_name)+1);
    return;
}

void lcdkit_backlight_ic_propname_cat(char*pdest, char*psrc, int len)
{
    if(NULL == pdest || NULL == psrc)
    {
        return;
    }
    memset(pdest,0,len);
    sprintf(pdest,"%s,%s",chip_name,psrc);

    return;
}

void lcdkit_parse_backlight_ic_cmd(struct device_node *pnp, char *node_str, struct backlight_ic_cmd *pcmd)
{
	char tmp_buf[128] = {0};
	struct property *prop = NULL;
	u32 *buf = NULL;
	int ret = 0;

	if((NULL == node_str)||(NULL == pcmd)||(NULL == pnp))
	{
		return;
	}
	lcdkit_backlight_ic_propname_cat(tmp_buf, node_str, sizeof(tmp_buf));
    prop = of_find_property(pnp, tmp_buf, NULL);
    if(NULL == prop)
    {
        LCDKIT_ERR("%s is not config!\n", node_str);
        memset(pcmd, 0, sizeof(struct backlight_ic_cmd));
    }
    else
    {
        if(prop->length == NUMOFELEMENT*sizeof(u32))
        {
            buf = kzalloc(prop->length, GFP_KERNEL);
            if(!buf)
            {
                LCDKIT_ERR("%s malloc failed!\n", node_str);
                memset(pcmd, 0, sizeof(struct backlight_ic_cmd));
            }
            else
            {
                ret = of_property_read_u32_array(pnp, tmp_buf, buf, prop->length/sizeof(u32));
                if(ret)
                {
                    memset(pcmd, 0, sizeof(struct backlight_ic_cmd));
                }
                else
                {
                    pcmd->ops_type = buf[0];
                    pcmd->cmd_reg = buf[1];
                    pcmd->cmd_val = buf[2];
                    pcmd->cmd_mask = buf[3];
                    LCDKIT_ERR("%s   type is 0x%x  reg is 0x%x  val is 0x%x   mask is 0x%x\n",
					           node_str, pcmd->ops_type, pcmd->cmd_reg, pcmd->cmd_val, pcmd->cmd_mask); 
                }
                kfree(buf);
                buf = NULL;
            }
        }
        else
        {
            memset(pcmd, 0, sizeof(struct backlight_ic_cmd));
        }
    }
}

void lcdkit_parse_backlight_ic_backlightcmd(struct device_node *pnp, char *node_str, struct backlight_reg_info *pcmd)
{
	char tmp_buf[128] = {0};
	struct property *prop = NULL;
	u32 *buf = NULL;
	int ret = 0;

	if((NULL == node_str)||(NULL == pcmd)||(NULL == pnp))
	{
		return;
	}
	lcdkit_backlight_ic_propname_cat(tmp_buf, node_str, sizeof(tmp_buf));
    prop = of_find_property(pnp, tmp_buf, NULL);
    if(NULL == prop)
    {
        LCDKIT_ERR("%s is not config!\n", node_str);
        memset(pcmd, 0, sizeof(struct backlight_reg_info));
    }
    else
    {
        if(prop->length == NUMOFELEMENT*sizeof(u32))
        {
            buf = kzalloc(prop->length, GFP_KERNEL);
            if(!buf)
            {
                LCDKIT_ERR("%s malloc failed!\n", node_str);
                memset(pcmd, 0, sizeof(struct backlight_reg_info));
            }
            else
            {
                ret = of_property_read_u32_array(pnp, tmp_buf, buf, prop->length/sizeof(u32));
                if(ret)
                {
                    memset(pcmd, 0, sizeof(struct backlight_reg_info));
                }
                else
                {
                    pcmd->val_bits = buf[0];
                    pcmd->cmd_reg = buf[1];
                    pcmd->cmd_val = buf[2];
                    pcmd->cmd_mask = buf[3];
                    LCDKIT_ERR("%s  val_bits is 0x%x  reg is 0x%x  val is 0x%x   mask is 0x%x\n",
					           node_str, pcmd->val_bits, pcmd->cmd_reg, pcmd->cmd_val, pcmd->cmd_mask); 
                }
                kfree(buf);
                buf = NULL;
            }
        }
        else
        {
            memset(pcmd, 0, sizeof(struct backlight_reg_info));
        }
    }
}

void lcdkit_parse_backlight_ic_param(struct device_node *pnp, char *node_str, unsigned int *pval)
{
	char tmp_buf[128] = {0};
	u32 tmp_32 = 0;
	int ret = 0;
	
	if((NULL == node_str)||(NULL == pval)||(NULL == pnp))
	{
		return;
	}
	
	lcdkit_backlight_ic_propname_cat(tmp_buf, node_str, sizeof(tmp_buf));
    ret = of_property_read_u32(pnp, tmp_buf, &tmp_32);
    *pval = (!ret? tmp_32 : 0);
    LCDKIT_ERR("%s ret is %d param is %d\n",node_str, ret, *pval);
}

void lcdkit_parse_backlight_ic_config(struct device_node *np)
{
    char tmp_buf[128] = {0};
    int ret = 0;
    u32 tmp_32 = 0;
    u32 *buf = NULL;
    int i = 0;
    int j = 0;
    struct property *prop = NULL;

    printk("lcdkit_parse_backlight_ic_config\n");
    if(!strlen(chip_name) || !strcmp(chip_name,"default") ||(NULL == np))
    {
        return -1;
    }

	lcdkit_parse_backlight_ic_param(np, "lcdkit-bl-ic-level", &g_bl_config.bl_level);
	lcdkit_parse_backlight_ic_param(np, "lcdkit-bl-ic-ctrl-mode", &g_bl_config.bl_ctrl_mod);
	lcdkit_parse_backlight_ic_param(np, "lcdkit-bl-ic-type", &g_bl_config.ic_type);
	lcdkit_parse_backlight_ic_param(np, "lcdkit-bl-ic-num-of-init-cmd", &g_bl_config.num_of_init_cmds);	
	lcdkit_parse_backlight_ic_param(np, "lcdkit-bl-ic-led-open-short-test", &g_bl_config.led_open_short_test);	
	lcdkit_parse_backlight_ic_param(np, "lcdkit-bl-ic-num-of-led", &g_bl_config.led_num);
	lcdkit_parse_backlight_ic_param(np, "lcdkit-bl-ic-before-init-delay", &g_bl_config.ic_before_init_delay);
	lcdkit_parse_backlight_ic_param(np, "lcdkit-bl-ic-init-delay", &g_bl_config.ic_init_delay);
	lcdkit_parse_backlight_ic_param(np, "lcdkit-bl-ic-ovp-check", &g_bl_config.ovp_check_enable);
	lcdkit_parse_backlight_ic_param(np, "lcdkit-bl-ic-fake-lcd-ovp-check", &g_bl_config.fake_lcd_ovp_check);
	
    lcdkit_backlight_ic_propname_cat(tmp_buf,"lcdkit-bl-ic-init-cmd",sizeof(tmp_buf));
    prop = of_find_property(np, tmp_buf, NULL);
    if(NULL == prop)
    {
        LCDKIT_ERR("lcdkit-bl-ic-init-cmd is not config!\n");
        memset(g_bl_config.init_cmds, 0, sizeof(g_bl_config.init_cmds));
    }
    else
    {
        if(prop->length == g_bl_config.num_of_init_cmds*NUMOFELEMENT*sizeof(u32))
        {
            buf = kzalloc(prop->length, GFP_KERNEL);
			if(!buf)
            {
                LCDKIT_ERR("lcdkit-bl-ic-init-cmd malloc failed!\n");
                memset(g_bl_config.init_cmds, 0, sizeof(g_bl_config.init_cmds));
            }
            else
            {
                ret = of_property_read_u32_array(np, tmp_buf, buf, prop->length/sizeof(u32));
                if(ret)
                {
                    memset(g_bl_config.init_cmds, 0, sizeof(g_bl_config.init_cmds));
				}
                else
                {
                    for(i=0,j=0; j<g_bl_config.num_of_init_cmds; j++)
                    {
                        g_bl_config.init_cmds[j].ops_type = buf[i+0];
                        g_bl_config.init_cmds[j].cmd_reg = buf[i+1];
                        g_bl_config.init_cmds[j].cmd_val = buf[i+2];
                        g_bl_config.init_cmds[j].cmd_mask = buf[i+3];
                        i+= 4;
                        LCDKIT_ERR("init j is %d, tpye is 0x%x  reg is 0x%x  val is 0x%x   mask is 0x%x\n",
						          j,g_bl_config.init_cmds[j].ops_type,g_bl_config.init_cmds[j].cmd_reg,g_bl_config.init_cmds[j].cmd_val,g_bl_config.init_cmds[j].cmd_mask );
                    }
                }
                kfree(buf);
                buf = NULL;
            }
        }
        else
        {
            memset(g_bl_config.init_cmds, 0, sizeof(g_bl_config.init_cmds));
        }
    }
	
    lcdkit_parse_backlight_ic_backlightcmd(np, "lcdkit-bl-ic-bl-lsb-reg-cmd", &g_bl_config.bl_lsb_reg_cmd);
    lcdkit_parse_backlight_ic_backlightcmd(np, "lcdkit-bl-ic-bl-msb-reg-cmd", &g_bl_config.bl_msb_reg_cmd);
	lcdkit_parse_backlight_ic_cmd(np, "lcdkit-bl-ic-bl-enable-cmd", &g_bl_config.bl_enable_cmd);
    lcdkit_parse_backlight_ic_cmd(np, "lcdkit-bl-ic-bl-disable-cmd", &g_bl_config.bl_disable_cmd);
	lcdkit_parse_backlight_ic_cmd(np, "lcdkit-bl-ic-disable-device-cmd", &g_bl_config.disable_dev_cmd);
	lcdkit_parse_backlight_ic_cmd(np, "lcdkit-bl-ic-fault-flag-cmd", &g_bl_config.bl_fault_flag_cmd);
	lcdkit_parse_backlight_ic_cmd(np, "lcdkit-bl-ic-bias-enable-cmd", &g_bl_config.bias_enable_cmd);
	lcdkit_parse_backlight_ic_cmd(np, "lcdkit-bl-ic-bias-disable-cmd", &g_bl_config.bias_disable_cmd);
	lcdkit_parse_backlight_ic_cmd(np, "lcdkit-bl-ic-brt-ctrl-cmd", &g_bl_config.bl_brt_ctrl_cmd);
	lcdkit_parse_backlight_ic_cmd(np, "lcdkit-bl-ic-fault-ctrl-cmd", &g_bl_config.bl_fault_ctrl_cmd);
}

int lcdkit_backlight_ic_get_ctrl_mode(void)
{
    if(plcdkit_bl_ic == NULL)
    {
        return -1;
    }
    return plcdkit_bl_ic->bl_config.bl_ctrl_mod;
}

void lcdkit_before_init_delay(void)
{
    if(NULL == plcdkit_bl_ic)
    {
        return;
    }
    if(plcdkit_bl_ic->bl_config.ic_before_init_delay)
    {
        mdelay(plcdkit_bl_ic->bl_config.ic_before_init_delay);
    }
    return;
}

struct lcdkit_bl_ic_info* lcdkit_get_lcd_backlight_ic_info(void)
{
    if(NULL == plcdkit_bl_ic)
    {
        return NULL;
    }
    return &plcdkit_bl_ic->bl_config;
}

int lcdkit_backlight_common_set(int bl_level)
{
	struct lcdkit_bl_ic_info *tmp = NULL;
	int bl_ctrl_mode = -1;

	tmp = lcdkit_get_lcd_backlight_ic_info();
	if(NULL != tmp)
	{
		bl_ctrl_mode = tmp->bl_ctrl_mod;
		switch(bl_ctrl_mode)
		{
			case BL_REG_ONLY_MODE:
			case BL_MUL_RAMP_MODE:
			case BL_RAMP_MUL_MODE:
				lcdkit_backlight_ic_set_brightness(bl_level);
				break;
			case BL_PWM_ONLY_MODE:
				break;
		}
	}

	return bl_ctrl_mode;
}

static unsigned int reg = 0x00;
static ssize_t backlight_reg_addr_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
    unsigned int reg_addr = 0;
    int ret = 0;

    ret = sscanf(buf, "reg=0x%x",&reg_addr);
    if(ret < 0)
    {
        printk("check input!\n");
        return -EINVAL;
    }
    reg = reg_addr;
    return size;
}
static DEVICE_ATTR(reg_addr, S_IRUGO|S_IWUSR, NULL, backlight_reg_addr_store);

static ssize_t backlight_reg_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct lcdkit_bl_ic_device *pchip = NULL;
    int ret = 0;
    unsigned int val = 0;

    if(!dev)
    {
        return snprintf(buf, PAGE_SIZE, "dev is null\n");
    }

    pchip = dev_get_drvdata(dev);
    if(!pchip)
    {
        return snprintf(buf, PAGE_SIZE, "data is null\n");
    }

    ret = i2c_smbus_read_byte_data(pchip->client, reg);
    if(ret < 0)
    {
        goto i2c_error;
    }
	val = ret;
    return snprintf(buf, PAGE_SIZE, "value = 0x%x\n",val);
	
i2c_error:
    return snprintf(buf, PAGE_SIZE, "backlight i2c read register error\n");
}

static ssize_t backlight_reg_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
    struct lcdkit_bl_ic_device *pchip = NULL;
    unsigned int reg = 0;
    unsigned int mask = 0;
    unsigned int val = 0;
	int ret = 0;

    ret = sscanf(buf, "reg=0x%x, mask=0x%x, val=0x%x",&reg,&mask,&val);
    if(ret < 0)
    {
        printk("check input!\n");
        return -EINVAL;
    }
    pchip = dev_get_drvdata(dev);
	if(!pchip)
    {
        return -EINVAL;
    }
	ret = lcdkit_backlight_ic_update_bit(pchip, reg, mask, val);
    if(ret < 0)
    {
        printk("backlight i2c update register error\n");
        return ret;
	}
    return size;
}
static DEVICE_ATTR(reg, S_IRUGO|S_IWUSR, backlight_reg_show, backlight_reg_store);

static ssize_t backlight_bl_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct lcdkit_bl_ic_device *pchip = NULL;
    int ret = 0;
    int bl_val = 0;
	int bl_lsb = 0;
	int bl_msb = 0;

    if(!dev)
    {
        return snprintf(buf, PAGE_SIZE, "dev is null\n");
    }

    pchip = dev_get_drvdata(dev);
    if(!pchip)
    {
        return snprintf(buf, PAGE_SIZE, "data is null\n");
    }

    ret = i2c_smbus_read_byte_data(pchip->client, pchip->bl_config.bl_lsb_reg_cmd.cmd_reg);
    if(ret < 0)
    {
        goto i2c_error;
    }
	bl_lsb = ret;
    ret = i2c_smbus_read_byte_data(pchip->client, pchip->bl_config.bl_msb_reg_cmd.cmd_reg);
    if(ret < 0)
    {
        goto i2c_error;
    }
	bl_msb = ret;
    bl_val = (bl_msb<<pchip->bl_config.bl_lsb_reg_cmd.val_bits) | bl_lsb;
    return snprintf(buf, PAGE_SIZE, "bl = 0x%x\n",bl_val);
	
i2c_error:
    return snprintf(buf, PAGE_SIZE, "backlight i2c read register error\n");
}

static ssize_t backlight_bl_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
    struct lcdkit_bl_ic_device *pchip = NULL;
	unsigned int bl_val = 0;
	unsigned int bl_msb = 0;
	unsigned int bl_lsb = 0;
	int ret = 0;

    ret = kstrtouint(buf, 10, &bl_val);
    if(ret)
    {
        printk("check input!\n");
        return -EINVAL;
    }
    pchip = dev_get_drvdata(dev);
	if(!pchip)
    {
        return -EINVAL;
    }
    bl_lsb = bl_val & pchip->bl_config.bl_lsb_reg_cmd.cmd_mask;
    bl_msb = (bl_val >> pchip->bl_config.bl_lsb_reg_cmd.val_bits)&pchip->bl_config.bl_msb_reg_cmd.cmd_mask;

    if(pchip->bl_config.bl_lsb_reg_cmd.val_bits != 0)
    {
        ret = lcdkit_backlight_ic_write_byte(pchip, pchip->bl_config.bl_lsb_reg_cmd.cmd_reg, bl_lsb);
        if(ret < 0)
        {
            printk("set backlight failed!\n");
            return -EINVAL;
        }
    }
    ret = lcdkit_backlight_ic_write_byte(pchip, pchip->bl_config.bl_msb_reg_cmd.cmd_reg, bl_msb);
    if(ret < 0)
    {
        printk("set backlight failed!\n");
        return -EINVAL;
    }
    return size;
}
static DEVICE_ATTR(backlight, S_IRUGO|S_IWUSR, backlight_bl_show, backlight_bl_store);

static int backlight_test_regs_store(struct lcdkit_bl_ic_device *pblchip, unsigned char reg_val[])
{
    int ret = 0;
    
    ret = lcdkit_backlight_ic_read_byte(pblchip, pblchip->bl_config.bl_msb_reg_cmd.cmd_reg, &reg_val[0]);
    if(ret < 0)
    {
        LCDKIT_ERR("TEST_ERROR_I2C\n");
        return TEST_ERROR_I2C;
    }

    ret = lcdkit_backlight_ic_read_byte(pblchip, pblchip->bl_config.bl_lsb_reg_cmd.cmd_reg, &reg_val[1]);
    if(ret < 0)
    {
        LCDKIT_ERR("TEST_ERROR_I2C\n");
        return TEST_ERROR_I2C;
    }

    ret = lcdkit_backlight_ic_read_byte(pblchip, pblchip->bl_config.bl_fault_ctrl_cmd.cmd_reg, &reg_val[2]);
    if(ret < 0)
    {
        LCDKIT_ERR("TEST_ERROR_I2C\n");
        return TEST_ERROR_I2C;
    }

    ret = lcdkit_backlight_ic_read_byte(pblchip, pblchip->bl_config.bl_enable_cmd.cmd_reg, &reg_val[3]);
    if(ret < 0)
    {
        LCDKIT_ERR("TEST_ERROR_I2C\n");
        return TEST_ERROR_I2C;
    }

    LCDKIT_INFO("REG_BRT_VAL_M=%d, REG_BRT_VAL_L=%d, REG_FAULT_CTR=%d, REG_ENABLE=%d\n",
                reg_val[0], reg_val[1], reg_val[2], reg_val[3]);
    return TEST_OK;
}

static int backlight_test_regs_restore(struct lcdkit_bl_ic_device *pblchip, unsigned char reg_val[])
{
    int ret = 0;

    ret = lcdkit_backlight_ic_write_byte(pblchip, pblchip->bl_config.bl_fault_ctrl_cmd.cmd_reg, reg_val[2]);
    if(ret < 0)
    {
        LCDKIT_ERR("TEST_ERROR_I2C\n");
        return TEST_ERROR_I2C;
    }
    msleep(10);
    ret = lcdkit_backlight_ic_write_byte(pblchip, pblchip->bl_config.bl_msb_reg_cmd.cmd_reg, reg_val[0]);
    if(ret < 0)
    {
        LCDKIT_ERR("TEST_ERROR_I2C\n");
        return TEST_ERROR_I2C;
    }

    ret = lcdkit_backlight_ic_write_byte(pblchip, pblchip->bl_config.bl_lsb_reg_cmd.cmd_reg, reg_val[1]);
    if(ret < 0)
    {
        LCDKIT_ERR("TEST_ERROR_I2C\n");
        return TEST_ERROR_I2C;
    }

    return TEST_OK;
}

static int backlight_test_set_brightness(struct lcdkit_bl_ic_device *pchip, unsigned int level)
{
    unsigned char level_lsb = 0;
    unsigned char level_msb = 0;
    int ret = 0;
    int result = TEST_OK;

    level_lsb = level & pchip->bl_config.bl_lsb_reg_cmd.cmd_mask;
    level_msb = (level >> pchip->bl_config.bl_lsb_reg_cmd.val_bits)&pchip->bl_config.bl_msb_reg_cmd.cmd_mask;

    if(pchip->bl_config.bl_lsb_reg_cmd.val_bits != 0)
    {
        ret = lcdkit_backlight_ic_write_byte(pchip, pchip->bl_config.bl_lsb_reg_cmd.cmd_reg, level_lsb);
        if(ret < 0)
        {
            return TEST_ERROR_I2C;;
        }
    }
    ret = lcdkit_backlight_ic_write_byte(pchip, pchip->bl_config.bl_msb_reg_cmd.cmd_reg, level_msb);
    if(ret < 0)
    {
        return TEST_ERROR_I2C;
    }

    return result;
}
static int backlight_test_led_open(struct lcdkit_bl_ic_device *pblchip, int led)
{
    int ret = 0;
    int result = TEST_OK;
    unsigned char val = 0;

    ret = lcdkit_backlight_ic_write_byte(pblchip, pblchip->bl_config.bl_enable_cmd.cmd_reg, pblchip->bl_config.bl_enable_cmd.cmd_val);
    if(ret < 0)
    {
        LCDKIT_ERR("TEST_ERROR_I2C\n");
        return TEST_ERROR_I2C;
    }

    ret |= backlight_test_set_brightness(pblchip, pblchip->bl_config.bl_level);

    ret = lcdkit_backlight_ic_write_byte(pblchip, pblchip->bl_config.bl_brt_ctrl_cmd.cmd_reg, 0x00);
    if(ret < 0)
    {
        LCDKIT_ERR("TEST_ERROR_I2C\n");
        return TEST_ERROR_I2C;
    }

    ret = lcdkit_backlight_ic_write_byte(pblchip, pblchip->bl_config.bl_enable_cmd.cmd_reg, (~(1<<(1 + led))) & pblchip->bl_config.bl_enable_cmd.cmd_val);
    if(ret < 0)
    {
        LCDKIT_ERR("TEST_ERROR_I2C\n");
        return TEST_ERROR_I2C;
    }

    msleep(4);

    ret = lcdkit_backlight_ic_read_byte(pblchip, pblchip->bl_config.bl_fault_flag_cmd.cmd_reg, &val);
    if(ret < 0)
    {
        LCDKIT_ERR("TEST_ERROR_I2C\n");
        return TEST_ERROR_I2C;
    }

    if (val & (1 << 4))
    {
        result |= (1<<(4 + led));
	}

    ret = lcdkit_backlight_ic_write_byte(pblchip, pblchip->bl_config.bl_enable_cmd.cmd_reg, pblchip->bl_config.bl_enable_cmd.cmd_val);
    if(ret < 0)
    {
        LCDKIT_ERR("TEST_ERROR_I2C\n");
        return result|TEST_ERROR_I2C;
    }

    msleep(1000);
	return result;
}

static int backlight_test_led_short(struct lcdkit_bl_ic_device *pblchip, int led)
{
    int ret = 0;
    int result = TEST_OK;
    unsigned char val = 0;
    int enable_reg = 0xF;

    switch(pblchip->bl_config.led_num)
    {
        case LED_ONE:
            enable_reg = 0x3;
            break;
        case LED_TWO:
            enable_reg = 0x7;
            break;
        case LED_THREE:
            enable_reg = 0xF;
            break;
        default:
            enable_reg = 0xF;
            break;
    }

    ret = lcdkit_backlight_ic_write_byte(pblchip, pblchip->bl_config.bl_enable_cmd.cmd_reg, (1<<(1 + led)) | 0x01);
    if(ret < 0)
    {
        LCDKIT_ERR("TEST_ERROR_I2C\n");
        return TEST_ERROR_I2C;
    }

    ret = lcdkit_backlight_ic_write_byte(pblchip, pblchip->bl_config.bl_fault_ctrl_cmd.cmd_reg, enable_reg);
    if(ret < 0)
    {
        LCDKIT_ERR("TEST_ERROR_I2C\n");
        return TEST_ERROR_I2C;
    }
    
    ret |= backlight_test_set_brightness(pblchip, pblchip->bl_config.bl_level);

    ret = lcdkit_backlight_ic_write_byte(pblchip, pblchip->bl_config.bl_brt_ctrl_cmd.cmd_reg, 0x00);
    if(ret < 0)
    {
        LCDKIT_ERR("TEST_ERROR_I2C\n");
        return TEST_ERROR_I2C;
    }

    msleep(4);

    ret = lcdkit_backlight_ic_read_byte(pblchip, pblchip->bl_config.bl_fault_flag_cmd.cmd_reg, &val);
    if(ret < 0)
    {
        LCDKIT_ERR("TEST_ERROR_I2C\n");
        return TEST_ERROR_I2C;
    }

    if (val & (1 << 3))
    {
        result |= (1<<(7 + led));
    }

    ret = lcdkit_backlight_ic_write_byte(pblchip, pblchip->bl_config.bl_enable_cmd.cmd_reg, 0x00);
    if(ret < 0)
    {
        LCDKIT_ERR("TEST_ERROR_I2C\n");
        return result|TEST_ERROR_I2C;
    }

    msleep(1000);
	return result;
}

static ssize_t backlight_self_test_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    struct lcdkit_bl_ic_device *pchip = NULL;
    struct i2c_client *client = NULL;
    int result = TEST_OK;
    int led_num = 0;
    unsigned char regval[4] = {0};
    int ret = 0;
    int i = 0;

    if(!bl_ic_init_status)
    {
        LCDKIT_ERR("init fail, return.\n");
        result |= TEST_ERROR_CHIP_INIT;
        return snprintf(buf, PAGE_SIZE, "%d\n", result);
    }

    if(!dev)
    {
        result = TEST_ERROR_DEV_NULL;
        LCDKIT_ERR("TEST_ERROR_DEV_NULL\n");
        return snprintf(buf, PAGE_SIZE, "%d\n", result);
    }

    pchip = dev_get_drvdata(dev);
    if(!pchip)
    {
        result = TEST_ERROR_DATA_NULL;
        LCDKIT_ERR("TEST_ERROR_DATA_NULL\n");
        return snprintf(buf, PAGE_SIZE, "%d\n", result);		
	}

    client = pchip->client;
    if(!client)
    {
        result = TEST_ERROR_CLIENT_NULL;
        LCDKIT_ERR("TEST_ERROR_CLIENT_NULL\n");
        return snprintf(buf, PAGE_SIZE, "%d\n", result);
    }

    if(!pchip->bl_config.led_open_short_test)
    {
        return snprintf(buf, PAGE_SIZE, "%d\n", result);
	}

    down(&(pchip->test_sem));
    result = backlight_test_regs_store(pchip, regval);
    if(result & TEST_ERROR_I2C)
    {
        up(&(pchip->test_sem));
        goto test_out;
    }

    led_num = pchip->bl_config.led_num;

    for(i=0; i<led_num; i++)
    {
        result |= backlight_test_led_open(pchip, i);
    }

    for(i=0; i<led_num; i++)
    {
        result |= backlight_test_led_short(pchip, i);
    }

    ret = lcdkit_backlight_ic_inital();
    if (ret < 0)
    {
        result |= TEST_ERROR_CHIP_INIT;
    }

    result |= backlight_test_regs_restore(pchip, regval);
    up(&(pchip->test_sem));
test_out:
    LCDKIT_INFO("self test out:%d\n", result);
    return snprintf(buf, PAGE_SIZE, "%d\n", result);
}
static DEVICE_ATTR(self_test, S_IRUGO|S_IWUSR, backlight_self_test_show, NULL);

static struct attribute *backlight_attributes[] = {
    &dev_attr_reg_addr.attr,
    &dev_attr_reg.attr,
    &dev_attr_backlight.attr,
    &dev_attr_self_test.attr,
    NULL,
};

static const struct attribute_group backlight_group = {
    .attrs = backlight_attributes,
};
#if defined (CONFIG_HUAWEI_DSM)
void lcdkit_backlight_ic_ovp_check(void)
{
    unsigned char val = 0;
    int ret = 0;
    char tmp_name[LCD_BACKLIGHT_IC_NAME_LEN] = {0};

    LCDKIT_INFO("backlight lcdkit_backlight_ic_ovp_check REG_FAULT_FLAG start!\n");

    ret = lcdkit_backlight_ic_fault_check(&val);
    if(ret > 0)
    {
        ret = dsm_client_ocuppy(lcd_dclient);
        if (!ret)
        {
            LCDKIT_ERR( "fail : REG_FAULT_FLAG statues error ,reg val 0x%x=0x%x!\n", plcdkit_bl_ic->bl_config.bl_fault_flag_cmd.cmd_reg, val);
            ret = lcdkit_get_backlight_ic_name(tmp_name,sizeof(tmp_name));
            if(0 != ret)
            {
                LCDKIT_ERR( "get chip name fail!\n");
                return;
            }
            if(OVP_FAULT_BIT_SET == (val & OVP_FAULT_BIT_SET))
            {
                dsm_client_record(lcd_dclient, "%s : reg val 0x%x=0x%x!\n", tmp_name, plcdkit_bl_ic->bl_config.bl_fault_flag_cmd.cmd_reg, val);
                dsm_client_notify(lcd_dclient, DSM_LCD_OVP_ERROR_NO);
            }

            if(OCP_FAULT_BIT_SET == (val & OCP_FAULT_BIT_SET))
            {
                dsm_client_record(lcd_dclient, "%s : reg val 0x%x=0x%x!\n", tmp_name, plcdkit_bl_ic->bl_config.bl_fault_flag_cmd.cmd_reg, val);
                dsm_client_notify(lcd_dclient, DSM_LCD_BACKLIGHT_OCP_ERROR_NO);
            }

            if(TSD_FAULT_BIT_SET == (val & TSD_FAULT_BIT_SET))
            {
                dsm_client_record(lcd_dclient, "%s : reg val 0x%x=0x%x!\n", tmp_name, plcdkit_bl_ic->bl_config.bl_fault_flag_cmd.cmd_reg, val);
                dsm_client_notify(lcd_dclient, DSM_LCD_BACKLIGHT_TSD_ERROR_NO);
            }
        }
        else
        {
            LCDKIT_ERR("dsm_client_ocuppy fail:  ret=%d!\n", ret);
        }
    }
}
#endif
static int lcdkit_backlight_ic_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    int ret = 0;
    struct device_node* np = NULL;

    printk("lcdkit_backlight_ic_probe");
    plcdkit_bl_ic = devm_kzalloc(&client->dev, sizeof(struct lcdkit_bl_ic_device) , GFP_KERNEL);

    if (!plcdkit_bl_ic)
    {
        return -ENOMEM;
    }

    plcdkit_bl_ic->client = client;
    np = lcdkit_get_lcd_node();
    if(!np)
    {
		printk("lcdkit_backlight_ic_probe  np is NULL");
        devm_kfree(&client->dev, plcdkit_bl_ic);
        plcdkit_bl_ic = NULL;
        return -EINVAL;
    }
    lcdkit_parse_backlight_ic_config(np);
    plcdkit_bl_ic->bl_config = g_bl_config;
    sema_init(&(plcdkit_bl_ic->test_sem), 1);

    ret = lcdkit_backlight_ic_inital();
    if(ret < 0)
    {
        devm_kfree(&client->dev, plcdkit_bl_ic);
        return ret;
    }

#if defined (CONFIG_HUAWEI_DSM)
    if(plcdkit_bl_ic->bl_config.ovp_check_enable)
    {
        if(plcdkit_bl_ic->bl_config.fake_lcd_ovp_check)
        {
            if(lcdkit_check_lcd_plugin())
            {
                lcdkit_backlight_ic_ovp_check();
            }
        }
        else
        {
            lcdkit_backlight_ic_ovp_check();
        }			
    }
#endif

    hisi_blpwm_bl_regisiter(lcdkit_backlight_common_set);
    if(backlight_class)
    {
        plcdkit_bl_ic->dev = device_create(backlight_class, NULL, 0, "%s", "lcd_backlight");
        if (IS_ERR(plcdkit_bl_ic->dev))
        {
		    /* Not fatal */
            printk("Unable to create device; errno = %ld\n", PTR_ERR(plcdkit_bl_ic->dev));
            plcdkit_bl_ic->dev = NULL;
        } 
        else
        {
            dev_set_drvdata(plcdkit_bl_ic->dev, plcdkit_bl_ic);
            ret = sysfs_create_group(&plcdkit_bl_ic->dev->kobj, &backlight_group);
            if (ret)
            {
                printk("Create backlight sysfs group node failed!\n");
                device_destroy(backlight_class,0);
            }
        }
    }
    bl_ic_init_status = true;
    return 0;
}

static int lcdkit_backlight_ic_remove(struct i2c_client *client)
{
    return 0;
}

static struct i2c_device_id bl_ic_common_id[] = {
    {"bl_ic_common", 0},
    {},
};

MODULE_DEVICE_TABLE(i2c, bl_ic_common_id);

static struct of_device_id lcdkit_backlight_ic_match_table[] = {
    { .compatible = "bl_ic_common",},
    {},
};

static struct i2c_driver lcdkit_backlight_ic_driver = {
	.probe = lcdkit_backlight_ic_probe,
	.remove = lcdkit_backlight_ic_remove,
	.driver = {
		.name = "bl_ic_common",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(lcdkit_backlight_ic_match_table),
	},
	.id_table = bl_ic_common_id,
};

static int __init lcdkit_backlight_ic_module_init(void)
{
    int ret = 0;
    char tmp_name[LCD_BACKLIGHT_IC_NAME_LEN] = {0};
	char compatible_name[128] = {0};

	ret = lcdkit_get_backlight_ic_name(tmp_name,sizeof(tmp_name));
	if(0 != ret)
    {
        return 0;
    }
    printk("lcdkit_backlight_ic_module_init chip_name is %s\n",tmp_name);
    if(!strcmp(tmp_name,"default"))
    {
        return 0;
    }

    memset(chip_name,0,LCD_BACKLIGHT_IC_NAME_LEN);
    memcpy(chip_name,tmp_name,LCD_BACKLIGHT_IC_NAME_LEN);
    chip_name[LCD_BACKLIGHT_IC_NAME_LEN-1] = 0;
    memcpy(bl_ic_common_id[0].name, chip_name, LCD_BACKLIGHT_IC_NAME_LEN);
    memcpy(lcdkit_backlight_ic_match_table[0].compatible, chip_name, LCD_BACKLIGHT_IC_NAME_LEN);
    lcdkit_backlight_ic_driver.driver.name = chip_name;
	snprintf(compatible_name, sizeof(compatible_name), "huawei,%s", chip_name);
    memcpy(lcdkit_backlight_ic_match_table[0].compatible, compatible_name, 128);
    lcdkit_backlight_ic_match_table[0].compatible[127] = 0;

    backlight_class = class_create(THIS_MODULE, "lcd_backlight");
    if (IS_ERR(backlight_class))
    {
        printk("Unable to create backlight class; errno = %ld\n", PTR_ERR(backlight_class));
        backlight_class = NULL;
    }

    return i2c_add_driver(&lcdkit_backlight_ic_driver);
}
module_init(lcdkit_backlight_ic_module_init);

MODULE_DESCRIPTION("AWINIC backlight ic common driver");
MODULE_LICENSE("GPL v2");
