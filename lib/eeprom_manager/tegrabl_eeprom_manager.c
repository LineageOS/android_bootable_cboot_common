/*
 * Copyright (c) 2015-2021, NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#define MODULE TEGRABL_ERR_EEPROM_MANAGER

#include <libfdt.h>
#include <tegrabl_error.h>
#include <tegrabl_eeprom_manager.h>
#include <tegrabl_eeprom_layout.h>
#include <tegrabl_devicetree.h>
#include <tegrabl_debug.h>
#include <tegrabl_i2c_dev.h>
#include <tegrabl_malloc.h>
#include <tegrabl_gpio.h>
#include <tegrabl_timer.h>
#include <string.h>

#define I2C_MUX_TYPE_TCA9546A					1
#define I2C_MUX_TCA9546A_MAX_CHANNEL			4
#define BOARD_ID_EEPROM_START_ADD				0x50
#define BOARD_ID_EEPROM_END_ADD					0x58
#define BOARD_ID_EEPROM_MUX_START_INDEX			89

static uint8_t count;
static bool eeprom_manager_initialized;
static struct tegrabl_eeprom eeproms[TEGRABL_EEPROM_MAX_NUM];

#define ALIAS_NAME_LEN 50
#define EEPROM_NAME_LEN 10
#define BUS_NODE_NAME 100

#if defined(CONFIG_ENABLE_GPIO)
static tegrabl_error_t cam_eeprom_read(struct tegrabl_eeprom *eeprom,
									   const void *in_data)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	if (!eeprom) {
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
	}

	void *fdt = NULL;

	int node = *(int *)in_data;
	struct gpio_driver *gpio_drv;
	uint32_t chip_id;
	uint32_t gpio_num;
	uint32_t gpio_prop[2];

	error = tegrabl_dt_get_fdt_handle(TEGRABL_DT_BL, &fdt);
	if (error != TEGRABL_NO_ERROR) {
		pr_error("%s: Failed to get BL-dtb handle\n", __func__);
		TEGRABL_SET_HIGHEST_MODULE(error);
		return error;
	}
	TEGRABL_ASSERT(fdt);

	error = tegrabl_dt_get_prop_u32_array(fdt, node, "enable-gpio", 2,
										gpio_prop, NULL);
	if (error != TEGRABL_NO_ERROR) {
		pr_error("%s: Cannot find enable-gpio property\n", __func__);
		return TEGRABL_ERROR(TEGRABL_ERR_NOT_FOUND, 0);
	}

	chip_id = gpio_prop[0];
	gpio_num = gpio_prop[1];
	pr_info("Enabling gpio chip_id = %u, gpio pin = %u\n", chip_id, gpio_num);

	error = tegrabl_gpio_driver_get(chip_id, &gpio_drv);
	if (error != TEGRABL_NO_ERROR) {
		pr_error("%s: Can't get gpio driver\n", __func__);
		return error;
	}

	error = gpio_config(gpio_drv, gpio_num, GPIO_PINMODE_OUTPUT);
	if (error != TEGRABL_NO_ERROR) {
		pr_error("%s: Error config gpio pin\n", __func__);
		return error;
	}

	error = gpio_write(gpio_drv, gpio_num, GPIO_PIN_STATE_HIGH);
	if (error != TEGRABL_NO_ERROR) {
		pr_error("%s: Error enabling gpio pin\n", __func__);
		return error;
	}
	tegrabl_udelay(100);

	eeprom->data_valid = false;
	error = tegrabl_eeprom_read(eeprom);
	if (error == TEGRABL_NO_ERROR) {
		eeprom->data_valid = true;
	}

	pr_info("Disabling gpio chip_id = %u, gpio pin = %u\n", chip_id, gpio_num);
	error = gpio_write(gpio_drv, gpio_num, GPIO_PIN_STATE_LOW);
	if (error != TEGRABL_NO_ERROR) {
		pr_error("%s: Error disabling gpio pin\n", __func__);
		return error;
	}

	return error;
}
#endif

static tegrabl_error_t eeprom_read(struct tegrabl_eeprom *eeprom,
								   const void *in_data)
{
	TEGRABL_UNUSED(in_data);

	tegrabl_error_t error = TEGRABL_NO_ERROR;

	if (!eeprom) {
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
	}

	eeprom->data_valid = false;
	error = tegrabl_eeprom_read(eeprom);
	if (error != TEGRABL_NO_ERROR) {
		TEGRABL_SET_HIGHEST_MODULE(error);
		return error;
	}
	eeprom->data_valid = true;

	return error;
}

static struct tegrabl_eeprom_ops_info eeprom_ops[TEGRABL_EEPROM_DEVICE_MAX] = {
#if defined(CONFIG_ENABLE_GPIO)
	{
		.name = "cam",
		.ops = cam_eeprom_read,
	},
#endif
	{
		.name = "module",
		.ops = eeprom_read,
	},
	{
		.name = NULL,
		.ops = eeprom_read,
	}
};

static tegrabl_error_t eeprom_general_read(struct tegrabl_eeprom *eeprom,
										   const void *in_data)
{

	if (!eeprom) {
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
	}

	if (!eeprom->name) {
		return eeprom_read(eeprom, in_data);
	}

	uint8_t index = 0;

	for (index = 0; index < TEGRABL_EEPROM_DEVICE_MAX; index++) {
		if (!eeprom_ops[index].name) {
			return eeprom_ops[index].ops(eeprom, in_data);
		}

		if (!strcmp(eeprom->name, eeprom_ops[index].name)) {
			return eeprom_ops[index].ops(eeprom, in_data);
		}
	}

	return TEGRABL_ERROR(TEGRABL_ERR_NOT_SUPPORTED, 0);
}

static tegrabl_error_t tegrabl_get_i2c_instance(int node, int *instance,
						char *i2c_nodename)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;
	void *fdt = NULL;
	int value_len, phandle, i2c_nodeoffset;
	const char *node_value;
	char i2c_alias[ALIAS_NAME_LEN];

	error = tegrabl_dt_get_fdt_handle(TEGRABL_DT_BL, &fdt);
	if (error != TEGRABL_NO_ERROR) {
		pr_error("%s: Failed to get BL-dtb handle\n", __func__);
		TEGRABL_SET_HIGHEST_MODULE(error);
		return error;
	}
	TEGRABL_ASSERT(fdt);

	/* Get i2c-bus property */
	error = tegrabl_dt_get_prop_u32(fdt, node, "i2c-bus", &phandle);
	if (error != TEGRABL_NO_ERROR) {
		pr_error("%s: Cannot find i2c-bus property\n", __func__);
		return TEGRABL_ERROR(TEGRABL_ERR_NOT_FOUND, 0);
	}

	/* Get the i2c bus phandle */
	i2c_nodeoffset = fdt_node_offset_by_phandle(fdt, phandle);
	if (i2c_nodeoffset <= 0) {
		pr_error("%s: Cannot get the i2c nodeoffset\n", __func__);
		return TEGRABL_ERROR(TEGRABL_ERR_NOT_FOUND, 0);
	}

	/* Get the i2c bus's name via node offset */
	node_value = fdt_get_name(fdt, i2c_nodeoffset, &value_len);
	if (node_value == NULL) {
		pr_error("%s: Cannot get i2c node name\n", __func__);
		return TEGRABL_ERROR(TEGRABL_ERR_NOT_FOUND, 0);
	}
	tegrabl_snprintf(i2c_nodename, ALIAS_NAME_LEN, "/%s",
					 (char *)node_value);

	/* Get the i2c alias name and instance. */
	error = tegrabl_get_alias_by_name(fdt, i2c_nodename, i2c_alias,
									  &value_len);
	if (error != TEGRABL_NO_ERROR) {
		pr_error("%s: Alias %s not found\n", __func__, i2c_nodename);
		return error;
	}

	error = tegrabl_get_alias_id("i2c", i2c_alias, instance);
	if (error != TEGRABL_NO_ERROR) {
		pr_error("%s: Alias instance not found\n", __func__);
		return error;
	}

	return error;
}

static tegrabl_error_t tegrabl_read_muxed_eeprom(uint8_t old_count,
												 uint8_t *new_count)
{
	uint8_t ncount = old_count + 1;
	uint32_t size = eeproms[old_count].size;
	tegrabl_instance_i2c_t instance = eeproms[old_count].instance;
	uint8_t old_slave_addr = eeproms[old_count].slave_addr;
	uint8_t eep_addr;
	tegrabl_error_t error;
	struct tegrabl_i2c_dev *hi2cdev;
	uint8_t mux_slave_addr;
	uint8_t mux_type;
	uint8_t channel;
	uint8_t addr;
	uint8_t fake_buf[10];
	uint8_t mux_base_index = BOARD_ID_EEPROM_MUX_START_INDEX;

	*new_count = old_count + 1;

	while (1) {
		mux_slave_addr = eeproms[old_count].data[mux_base_index];
		mux_type = eeproms[old_count].data[mux_base_index + 1];

		if ((mux_slave_addr == 0x00) || (mux_slave_addr == 0xFF)) {
			break;
		}

		if (mux_type != I2C_MUX_TYPE_TCA9546A) {
			break;
		}

		mux_slave_addr <<= 1;
		pr_info("Mux addr : 0x%02x and type %d\n", mux_slave_addr, mux_type);

		hi2cdev = tegrabl_i2c_dev_open(instance, mux_slave_addr, 1, 1);
		if (!hi2cdev) {
			pr_error("Can't get handle to mux device @%d\n", mux_slave_addr);
			return TEGRABL_ERROR(TEGRABL_ERR_BAD_PARAMETER, 0);
		}

		pr_info("Reading muxed eeprom i2c=%d:0x%x\n", instance, mux_slave_addr);

		for (channel = 0; channel < I2C_MUX_TCA9546A_MAX_CHANNEL; ++channel) {
			error = tegrabl_i2c_dev_write(hi2cdev, fake_buf, 1 << channel, 0);
			if (error != TEGRABL_NO_ERROR) {
				pr_error("%s: Cannot set mi2c mux channel\n", __func__);
				return TEGRABL_ERROR(TEGRABL_ERR_NOT_FOUND, 0);
			}

			tegrabl_udelay(100);

			for (addr = BOARD_ID_EEPROM_START_ADD;
				  addr <= BOARD_ID_EEPROM_END_ADD; ++addr) {
				eep_addr = addr << 1;

				if (old_slave_addr == eep_addr) {
					continue;
				}

				eeproms[ncount].data = tegrabl_malloc(size);
				if (!eeproms[ncount].data) {
					pr_error("%s: Malloc for eeprom data failed\n", __func__);
					return TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 0);
				}

				eeproms[ncount].name = NULL;
				eeproms[ncount].instance = instance;
				eeproms[ncount].crc_valid = false;
				eeproms[ncount].data_valid = false;
				eeproms[ncount].slave_addr = eep_addr;
				eeproms[ncount].size = size;

				error = tegrabl_eeprom_read(&eeproms[ncount]);
				if (error != TEGRABL_NO_ERROR) {
					pr_error("Eeprom read failed 0x%08x\n", error);
					tegrabl_free(eeproms[ncount].data);
					continue;
				}

				eeproms[ncount].data_valid = true;

				eeproms[ncount].bus_node_name = tegrabl_malloc(BUS_NODE_NAME);
				if (!eeproms[ncount].bus_node_name) {
					pr_error("%s: Malloc for eeprom bus node name failed\n",
							 __func__);
					return TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 0);
				}

				tegrabl_snprintf(eeproms[ncount].bus_node_name, BUS_NODE_NAME,
							     "%s:module@0x%02x:i2cmux@0x%0x:i2c@%d",
							     eeproms[old_count].bus_node_name,
								 eeproms[old_count].slave_addr / 2,
							     mux_slave_addr / 2, channel);
				pr_info("Device at %s:module@0x%02x\n",
						eeproms[ncount].bus_node_name, addr);
				ncount++;
			}
		}

		mux_base_index += 2;
	}

	*new_count = ncount;

	return TEGRABL_NO_ERROR;
}

static tegrabl_error_t tegrabl_eeprom_manager_init_legacy(void)
{
	void *fdt = NULL;
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	char i2c_nodename[ALIAS_NAME_LEN];

	error = tegrabl_dt_get_fdt_handle(TEGRABL_DT_BL, &fdt);
	if (error != TEGRABL_NO_ERROR) {
		pr_error("%s: Failed to get BL-dtb handle\n", __func__);
		TEGRABL_SET_HIGHEST_MODULE(error);
		return error;
	}
	TEGRABL_ASSERT(fdt);

	int manager_node = 0, manager_subnode = 0;
	int eeprom_node = 0, instance_addr = 0;
	uint32_t eeprom_size = 0, slave_addr = 0;
	const char *node_value;
	uint8_t new_count = 0;
	count = 0;

	error = tegrabl_dt_get_node_with_path(fdt, "/eeprom-manager",
										  &manager_node);
	if (error != TEGRABL_NO_ERROR) {
		pr_error("Cannot find DT node for EEPROM Manager\n");

		/* WAR of Bug 200160786
		 * Recovery will not load any device tree now,
		 * load only cvm eeprom
		 * should return TEGRABL_ERROR(TEGRABL_ERR_NOT_FOUND, 0);
		 */
		pr_info("Loading CVM eeprom only\n");
		eeproms[count].name = "module";
		eeproms[count].size = EEPROM_SZ;
		eeproms[count].instance = TEGRABL_INSTANCE_I2C8;
		eeproms[count].crc_valid = true;
		eeproms[count].data_valid = false;
		eeproms[count].slave_addr = 0xA0;

		eeproms[count].data = tegrabl_malloc(eeproms[count].size);
		if (!eeproms[count].data) {
			pr_error("%s: Malloc for eeprom data failed\n", __func__);
			return TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 0);
		}

		error = tegrabl_eeprom_read(&eeproms[count]);
		if (error != TEGRABL_NO_ERROR) {
			pr_info("Eeprom read failed 0x%08x\n", error);
			tegrabl_free(eeproms[count].data);
			return error;
		}

		eeproms[count].data_valid = true;
		count++;
		return TEGRABL_NO_ERROR;
	}

	/* Get the data size */
	error = tegrabl_dt_get_prop_u32(fdt, manager_node, "data-size",
									&eeprom_size);
	if (error != TEGRABL_NO_ERROR) {
		pr_error("%s: Cannot find data-size\n", __func__);
		return TEGRABL_ERROR(TEGRABL_ERR_NOT_FOUND, 0);
	}

	tegrabl_dt_for_each_child(fdt, manager_node, manager_subnode) {
		/* Get i2c alias instance. */
		tegrabl_get_i2c_instance(manager_subnode, &instance_addr, i2c_nodename);

		/* TODO: with scan-all-eeprom will scan from 0x50 to 0x57 */
		tegrabl_dt_for_each_child(fdt, manager_subnode, eeprom_node) {
			if (count == TEGRABL_EEPROM_MAX_NUM) {
				pr_error("%s: Too many eeprom node scanned\n", __func__);
				return TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 0);
			}

			/* Get slave address */
			error = tegrabl_dt_get_prop_u32(fdt, eeprom_node,
											"slave-address", &slave_addr);
			if (error != TEGRABL_NO_ERROR) {
				pr_error("%s: Slave address not found\n", __func__);
				return TEGRABL_ERROR(TEGRABL_ERR_NOT_FOUND, 0);
			}

			/* Get label (optional) property */
			error = tegrabl_dt_get_prop_string(fdt, eeprom_node,
											   "label", &node_value);
			if (error == TEGRABL_NO_ERROR) {
				/* Do we need to malloc(&copy) it? */
				eeproms[count].name = tegrabl_malloc(EEPROM_NAME_LEN);
				if (!eeproms[count].name) {
					pr_error("%s: Malloc for eeprom name failed\n", __func__);
					return TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 0);
				}

				/* for backwards-compatibility with old device trees */
				if (strcmp(node_value, "cvm") == 0)
					node_value = "module";

				tegrabl_snprintf(eeproms[count].name, EEPROM_NAME_LEN, "%s",
								 (char *)node_value);
			} else {
				eeproms[count].name = NULL;
			}

			eeproms[count].size = (eeprom_size > EEPROM_SZ) ?
								  EEPROM_SZ : eeprom_size;

			eeproms[count].data = tegrabl_malloc(eeproms[count].size);
			if (!eeproms[count].data) {
				pr_error("%s: Malloc for eeprom data failed\n", __func__);
				return TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 0);
			}

			eeproms[count].instance = instance_addr;
			eeproms[count].crc_valid = false;
			eeproms[count].data_valid = false;
			eeproms[count].slave_addr = slave_addr << 1;
			pr_info("Reading eeprom i2c=%d address=0x%x\n",
					instance_addr, slave_addr);

			error = eeprom_general_read(&eeproms[count], &eeprom_node);

			if (!eeproms[count].data_valid) {
				pr_info("Eeprom read failed 0x%08x\n", error);
				if (eeproms[count].name) {
					tegrabl_free(eeproms[count].name);
				}
				tegrabl_free(eeproms[count].data);
				continue;
			}

			eeproms[count].bus_node_name = tegrabl_malloc(BUS_NODE_NAME);
			if (!eeproms[count].bus_node_name) {
				pr_error("%s: Malloc for eeprom bus node name failed\n",
					     __func__);
				return TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, 0);
			}
			tegrabl_snprintf(eeproms[count].bus_node_name, BUS_NODE_NAME, "%s",
							i2c_nodename);
			pr_info("Device at %s:0x%02x\n", i2c_nodename, slave_addr);

			eeproms[count].data_valid = true;

			tegrabl_read_muxed_eeprom(count, &new_count);

			count = new_count;
		}
	}

	return TEGRABL_NO_ERROR;
}

tegrabl_error_t tegrabl_eeprom_manager_init(void)
{
	int parent, len, err, node = -1;
	uint32_t address, size;
	tegrabl_error_t error;
	const char *name;
	char path[1024];
	uintptr_t base;
	void *fdt;

	if (eeprom_manager_initialized) {
		/* return early */
		return TEGRABL_NO_ERROR;
	}

	error = tegrabl_dt_get_fdt_handle(TEGRABL_DT_BL, &fdt);
	if (error != TEGRABL_NO_ERROR) {
		return error;
	}

	/* reset the global count */
	count = 0;

	while (count < TEGRABL_EEPROM_MAX_NUM) {
		node = fdt_next_node(fdt, node, NULL);
		if (node < 0) {
			if (node == -FDT_ERR_NOTFOUND)
				break;

			return node;
		}

		name = fdt_get_name(fdt, node, &len);
		if (!name) {
			pr_error("failed to get name for node %d\n", node);
			continue;
		}

		if (strncmp(name, "eeprom", 6) != 0) {
			continue;
		}

		pr_debug("  @%08x: %s (%d)\n", node, name, len);

		err = fdt_get_path(fdt, node, path, sizeof(path));
		if (err < 0) {
			pr_error("failed to get path for node %d: %d\n", node, err);
			continue;
		}

		pr_debug("    path: %s (%d)\n", path, err);

		error = tegrabl_dt_get_prop_string(fdt, node, "label", &name);
		if (error != TEGRABL_NO_ERROR) {
			pr_error("failed to read label property for node %d: %d\n", node, error);
			continue;
		}

		pr_debug("    label: %s\n", name);

		error = tegrabl_dt_get_prop_u32(fdt, node, "reg", &address);
		if (error != TEGRABL_NO_ERROR) {
			pr_error("failed to read reg property for node %d: %d\n", node, error);
			continue;
		}

		error = tegrabl_dt_get_prop_u32(fdt, node, "size", &size);
		if (error != TEGRABL_NO_ERROR) {
			pr_error("failed to read size property for node %d: %d\n", node, error);
			continue;
		}

		pr_debug("    address: %02x\n", address);
		pr_debug("    size: %u\n", size);

		parent = fdt_parent_offset(fdt, node);
		if (parent < 0) {
			pr_error("failed to get parent for node %s: %d\n", name, parent);
			continue;
		}

		error = tegrabl_dt_read_reg_by_index(fdt, parent, 0, &base, NULL);
		if (error != TEGRABL_NO_ERROR) {
			pr_error("failed to get registers for %s: %d\n", name, error);
			continue;
		}

		pr_debug("    i2c: @%lx\n", base);

		eeproms[count].size = MAX(size, EEPROM_SZ);
		eeproms[count].name = strdup(name);

		eeproms[count].data = tegrabl_malloc(eeproms[count].size);
		if (!eeproms[count].data) {
			pr_error("failed to allocate memory for EEPROM data\n");
			continue;
		}

		error = tegrabl_i2c_lookup(base, &eeproms[count].instance);
		if (error != TEGRABL_NO_ERROR) {
			pr_error("no I2C instance found for base %lx\n", base);
			continue;
		}

		eeproms[count].crc_valid = false;
		eeproms[count].data_valid = false;
		eeproms[count].slave_addr = address << 1;

		error = eeprom_general_read(&eeproms[count], &node);
		if (error != TEGRABL_NO_ERROR) {
			tegrabl_free(eeproms[count].data);
			continue;
		}

		if (!eeproms[count].data_valid) {
			tegrabl_free(eeproms[count].data);
			continue;
		}

		eeproms[count].bus_node_name = tegrabl_malloc(BUS_NODE_NAME);
		if (!eeproms[count].bus_node_name) {
			pr_error("failed to allocate memory for EEPROM bus node name\n");
			tegrabl_free(eeproms[count].data);
			continue;
		}

		err = fdt_get_path(fdt, parent, eeproms[count].bus_node_name, BUS_NODE_NAME);
		if (err < 0) {
			tegrabl_free(eeproms[count].data);
			continue;
		}

		pr_info("EEPROM: %s: %s (%u bytes)\n", path, name, size);

		count++;
	}

	if (count == 0) {
		error = tegrabl_eeprom_manager_init_legacy();
		if (error != TEGRABL_NO_ERROR) {
			pr_error("failed to initialize legacy EEPROM support\n");
			return error;
		}
	}

	eeprom_manager_initialized = true;
	return TEGRABL_NO_ERROR;
}

tegrabl_error_t tegrabl_eeprom_manager_max(uint8_t *num)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	error = tegrabl_eeprom_manager_init();

	if (error != TEGRABL_NO_ERROR) {
		TEGRABL_SET_HIGHEST_MODULE(error);
		return error;
	}

	*num = count;

	return TEGRABL_NO_ERROR;
}

tegrabl_error_t tegrabl_eeprom_manager_get_eeprom_by_id(uint8_t module_id,
												struct tegrabl_eeprom **eeprom)
{
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	error = tegrabl_eeprom_manager_init();

	if (error != TEGRABL_NO_ERROR) {
		TEGRABL_SET_HIGHEST_MODULE(error);
		return error;
	}

	if (module_id >= count) {
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
	}

	*eeprom = &eeproms[module_id];

	return TEGRABL_NO_ERROR;
}

tegrabl_error_t tegrabl_eeprom_manager_get_eeprom_by_name(const char *name,
												struct tegrabl_eeprom **eeprom)
{
	uint8_t index = 0;
	tegrabl_error_t error = TEGRABL_NO_ERROR;

	if (!name) {
		return TEGRABL_ERROR(TEGRABL_ERR_INVALID, 0);
	}

	error = tegrabl_eeprom_manager_init();

	if (error != TEGRABL_NO_ERROR) {
		TEGRABL_SET_HIGHEST_MODULE(error);
		return error;
	}

	for (index = 0; index < count; index++) {
		if (eeproms[index].name == NULL) {
			continue;
		}

		if (!strcmp(eeproms[index].name, (const char *)name)) {
			error = tegrabl_eeprom_manager_get_eeprom_by_id(index, eeprom);
			return error;
		}
	}

	return TEGRABL_ERROR(TEGRABL_ERR_NOT_SUPPORTED, 0);
}

void tegrabl_eeprom_manager_release_resources(void)
{
	return;
}
