/*
 * Copyright (c) 2014-2021, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#if !defined(__TEGRABL_DEVICETREE_H__)
#define __TEGRABL_DEVICETREE_H__

#include <libfdt.h>
#include <tegrabl_error.h>
#include <stdint.h>
#include <stdbool.h>

#define U8_SZ sizeof(uint8_t)
#define U32_SZ sizeof(uint32_t)

/**
 * @brief Types of device tree supported
 */
/* macro tegrabl dt */
typedef uint32_t tegrabl_dt_type_t;
#define TEGRABL_DT_BL 0
#define TEGRABL_DT_KERNEL 1
#define TEGRABL_DT_CBO 2
#define TEGRABL_DT_COUNT 3
#if defined(CONFIG_ENABLE_L4T_RECOVERY)
#define TEGRABL_DT_INVALID TEGRABL_DT_COUNT
#endif

/**
 * @brief Get the next node's offset
 *
 * @param fdt device tree handle
 * @param node Offset of current node
 */
static inline int32_t tegrabl_dt_next_node(const void *fdt, int node)
{
	return fdt_next_node(fdt, node, NULL);
}

/**
 * @brief iterate through all nodes starting from a certain node
 *
 * @param fdt device tree handle
 * @param node integer iterator over tegrabl_dt nodes
 * @param start_node offset of the node to begin traversal with
 */
#define tegrabl_dt_for_each_node_from(fdt, node, start_node)	\
	for (node = start_node;								\
		(node) >= 0;					\
		node = tegrabl_dt_next_node(fdt, node))

/**
 * @brief Iterate through all nodes
 *
 * @param fdt Device Tree handle
 * @param node Integer iterator over tegrabl_dt nodes
 */
#define tegrabl_dt_for_each_node(fdt, node) \
	tegrabl_dt_for_each_node_from(fdt, node, tegrabl_dt_next_node(fdt, -1))

/**
 * @brief Read an <addr,size> tuple in "reg" property
 *		 of a device node specified at particular offset.
 *
 * @param node The offset to the particular node.
 * @param index The index of <addr,size> tuple to be read.
 * @param addr The addr part of the specified element will be read into
 *		  this field. address will not be read if this is NULL.
 * @param size The size part of the specified element will be read into
 *		  this field. size will not be read if this is NULL.
 *
 * @return TEGRABL_NO_ERROR if the specified addr and/or size is read
 *         successfully, otherwise appropriate error.
 */
tegrabl_error_t tegrabl_dt_read_reg_by_index(const void *fdt, int node,
											uint32_t index, uintptr_t *addr,
											uintptr_t *size);

/**
 * @brief Read the interrupt numbers from a device node.
 *
 * @param node The offset to the particular node.
 * @param intr The pointer where the read interrupts are being read.
 * @param count Number of interrupts to be read int 'intr' param.
 *
 * @return TEGRABL_NO_ERROR if the specified number of interrupts is read
 *		   successfully, otherwise appropriate error.
 */
tegrabl_error_t tegrabl_dt_get_gic_intr(const void *fdt, int node,
										uint32_t *intr, uint32_t count);

/**
 * @brief Save the pointer to fdt.
 *
 * @param type Dt type
 * @param fdt Dt handle
 *
 * @return TEGRABL_NO_ERROR if the specified number of interrupts is read
 *		   successfully, otherwise appropriate error.
 */
tegrabl_error_t tegrabl_dt_set_fdt_handle(tegrabl_dt_type_t type,
										  void *fdt);

/**
 * @brief Returns the pointer to fdt.
 *
 * @brief type Dt type
 * @fdt The handle of fdt returned.
 *
 * @return TEGRABL_NO_ERROR if the specified number of interrupts is read
 *		   successfully, otherwise appropriate error.
 */
tegrabl_error_t tegrabl_dt_get_fdt_handle(tegrabl_dt_type_t type,
										  void **fdt);

/**
 * @brief Get the number of children of a node
 *
 * @param fdt Handle to device tree blob
 * @param node Handle to the parent node
 *
 * @return Number of children of the parent node
 */
uint32_t tegrabl_dt_get_child_count(const void *fdt, int node);

/**
 * @brief Retrieve child node offset of a node with the given name
 *
 * @param fdt Handle to device tree blob
 * @param start_offset Node in FDT to start the search from.
 * @param name Name of the child node
 * @param res Callee filled. On success, holds offset of the child node that
 *		      matches the criteria.
 *
 * @return TEGRABL_NO_ERROR if success, else TEGRABL_ERR_NOT_FOUND
 */
tegrabl_error_t tegrabl_dt_get_child_with_name(const void *fdt,
											int start_offset, char *name,
											int *res);

/**
 * @brief Get the next child of a node
 *		  You most certainly do not need to use this function.
 *		  Use the iterator macros instead
 *
 * @param fdt Handle to device tree blob
 * @param node Handle to the parent node
 * @param prev_child Handle to the last sibling visited
 * @param res Callee filled. On success, holds offset of the next child node
 *
 * @return TEGRABL_NO_ERROR if success, else TEGRABL_ERR_NOT_FOUND
 */
tegrabl_error_t tegrabl_dt_get_next_child(const void *fdt, int node,
										int prev_child, int *res);

#define tegrabl_dt_for_each_child(fdt, parent, child)		\
	for (child = fdt_first_subnode(fdt, parent);	\
		(child) != -FDT_ERR_NOTFOUND;				\
		child = fdt_next_subnode(fdt, child))

/**
 * @brief Read a property indexed in an array
 *		  Use tegrabl_dt_get_prop_array if you want the whole array of values
 *
 * @param fdt Handle to device tree blob
 * @param node Handle to the parent node
 * @param prop Name of the property to read
 * @param sz Size of each element in array (in bytes - 1/4)
 * @param idx Index of the value in array
 * @param res Pointer to collect the result
 *
 * @return TEGRABL_ERR_NOT_FOUND if the property is not available
 *		   TEGRABL_NO_ERROR if success
 *		   On success, res points to the value retrieved from tegrabl_dtB
 */
tegrabl_error_t tegrabl_dt_get_prop_by_idx(const void *fdt, int node,
										char *prop, size_t sz,
										uint32_t idx, void *res);

static inline tegrabl_error_t tegrabl_dt_get_prop_u8_by_idx(const void *fdt,
														int node, char *prop,
														uint32_t idx, void *res)
{
	return tegrabl_dt_get_prop_by_idx(fdt, node, prop, U8_SZ, idx, res);
}

static inline tegrabl_error_t tegrabl_dt_get_prop_u32_by_idx(const void *fdt,
														int node, char *prop,
														uint32_t idx, void *res)
{
	return tegrabl_dt_get_prop_by_idx(fdt, node, prop, U32_SZ, idx, res);
}

static inline tegrabl_error_t tegrabl_dt_get_prop(const void *fdt,
											int node, char *prop,
											size_t sz, void *res)
{
	return tegrabl_dt_get_prop_by_idx(fdt, node, prop, sz, 0, res);
}

static inline tegrabl_error_t tegrabl_dt_get_prop_u8(const void *fdt, int node,
										char *prop, void *res)
{
	return tegrabl_dt_get_prop_by_idx(fdt, node, prop, U8_SZ, 0, res);
}

static inline tegrabl_error_t tegrabl_dt_get_prop_u32(const void *fdt, int node,
										char *prop, void *res)
{
	return tegrabl_dt_get_prop_by_idx(fdt, node, prop, U32_SZ, 0, res);
}

/**
 * @brief Read an array property
 *
 * @param fdt Handle to device tree blob
 * @param node offset of the tegrabl_dt node
 * @param prop Name of the property to read
 * @param sz Size of each element in array (in bytes - 1/4)
 * @param nmemb Number of array members to get. If 0, the entire array is
 *				returned. So prevent misuse, we make sure that num is non NULL
 *				when nmemb is zero.
 * @param res Pointer to the result values. Caller allocates the memory
 * @param num Number of array members successfully read; callee filled
 *
 * @return TEGRABL_ERR_NOT_FOUND if the property is not available
 *		   TEGRABL_ERR_NOT_SUPPORTED if an invalid sz value is passed
 *		   TEGRABL_NO_ERROR if success
 *		   On success, the memory pointed to by res contains nmemb legitimate
 *		   values each of size sz. If the array does not have nmemb members,
 *		   num will point to the number of members read (< nmemb)
 */
tegrabl_error_t tegrabl_dt_get_prop_array(const void *fdt, int node,
										char *prop, size_t sz,
										uint32_t nmemb, void *res,
										uint32_t *num);

static inline tegrabl_error_t tegrabl_dt_get_prop_u8_array(const void *fdt,
													int node, char *prop,
													uint32_t nmemb, void *res,
													uint32_t *num)
{
	return tegrabl_dt_get_prop_array(fdt, node, prop, U8_SZ, nmemb, res, num);
}

static inline tegrabl_error_t tegrabl_dt_get_prop_u32_array(const void *fdt,
													int node, char *prop,
													uint32_t nmem, void *res,
													uint32_t *num)
{
	return tegrabl_dt_get_prop_array(fdt, node, prop, U32_SZ, nmem, res, num);
}

/**
 * @brief Read a string property
 *
 * @param fdt Handle to device tree blob
 * @param node offset of the tegrabl_dt node
 * @param prop Name of the property to read
 * @param res Pointer to hold the pointer to string value
 *
 * @return TEGRABL_ERR_NOT_FOUND if the property is not available
 *		   TEGRABL_NO_ERROR if success
 *		   On success, res points to the pointer to the string
 */
tegrabl_error_t tegrabl_dt_get_prop_string(const void *fdt, int node,
										char *prop, const char **res);

/**
 * @brief Read a property that contains an array of strings
 *
 * @param fdt Handle to device tree blob
 * @param node offset of the tegrabl_dt node
 * @param prop Name of the property to read
 * @param res Pointer to th array of char pointers. Callee filled
 * @param num Callee filled. Contains the number of terminated strings in
 *			  prop
 *
 * @return TEGRABL_ERR_NOT_FOUND if the property is not available
 *		   TEGRABL_NO_ERROR if success
 *		   On success, res points to the array of pointers to individual
 *		   strings, and num contains the number of strings in that array
 */
tegrabl_error_t tegrabl_dt_get_prop_string_array(const void *fdt, int node,
												char *prop, const char **res,
												uint32_t *num);

/**
 * @brief Count number of strings in a string array
 *
 * @param fdt Handle to device tree blob
 * @param node offset of the tegrabl_dt node
 * @param prop Name of the property to read
 * @param num Callee filled. Contains the number of terminated strings in
 *			  prop
 *
 * @return TEGRABL_ERR_NOT_FOUND if the property is not available
 *		   TEGRABL_NO_ERROR if success
 */
static inline tegrabl_error_t tegrabl_dt_get_prop_count_strings(const void *fdt,
				int node, char *prop, uint32_t *num)
{
	return tegrabl_dt_get_prop_string_array(fdt, node, prop, NULL, num);
}

/**
 * @brief Count number of elements of a given size in a tegrabl_dt property
 *
 * @param fdt Handle to device tree blob
 * @param node offset of the tegrabl_dt node
 * @param prop Name of the property to read
 * @param sz Size of each element
 * @param num Callee filled. On success, contains the number of elements of
 *			  size sz in the property value
 *
 * @return TEGRABL_ERR_NOT_FOUND if the property is not available
 *		   TEGRABL_NO_ERROR if success
 */
tegrabl_error_t tegrabl_dt_count_elems_of_size(const void *fdt, int node,
											char *prop, uint32_t sz,
											uint32_t *num);

/**
 * @brief Iterate the properties under a certain node
 *
 * @param fdt Device tree handle
 * @param offset integer iterator over each property
 * @param start_node offset of the node, whose properties to traverse
 */
#define tegrabl_dt_for_each_prop_of(fdt, offset, node) \
	for (offset = fdt_first_property_offset(fdt, node); \
		(offset >= 0); \
		(offset = fdt_next_property_offset(fdt, offset)))

/**
 * @brief Retrieve node offset of a node with the given name
 *		  Can be used to iterate over all nodes with a certain name
 *
 * @param fdt Handle to device tree blob
 * @param start_offset Node in FDT to start the search from.
 * @param name Name of the node
 * @param res Callee filled. On success, holds offset of the node that matches
 *		      the criteria.
 *
 * @return TEGRABL_NO_ERROR if success, else TEGRABL_ERR_NOT_FOUND
 */
tegrabl_error_t tegrabl_dt_get_node_with_name(const void *fdt, int start_offset,
											char *name, int *res);

#define tegrabl_dt_for_each_node_with_name_from(fdt, node, start_offset, name) \
	for (node = start_offset;												   \
		tegrabl_dt_get_node_with_name(fdt, node, name, &(node)) ==			   \
														TEGRABL_NO_ERROR;	   \
		node = tegrabl_dt_next_node(fdt, node))

#define tegrabl_dt_for_each_node_with_name(fdt, node, name)					\
	tegrabl_dt_for_each_node_with_name_from(fdt, node,						\
											tegrabl_dt_next_node(fdt, -1),	\
											name)

/**
 * @brief Retrieve node offset of a node with the given path
 *
 * @param fdt Handle to device tree blob
 * @param path Path of the node relative to root node
 * @param res Callee filled. On success, holds offset of the node that matches
 *		      the criteria.
 *
 * @return TEGRABL_NO_ERROR if success, else TEGRABL_ERR_NOT_FOUND
 */
tegrabl_error_t tegrabl_dt_get_node_with_path(const void *fdt, const char *path,
											int *res);

/**
 * @brief Check if the platform device represented by the DT node is actually
 *	      present on the SOC. This is done by checking if the status property
 *	      is "ok" or "okay". If the property is absent, the device is assumed
 *	      to be present by default
 *
 * @param fdt Handle to device tree blob
 * @param node offset of the tegrabl_dt node
 * @param res Callee filled. Holds true if device is present, else false
 *
 * @return TEGRABL_NO_ERROR if success, else TEGRABL_ERR_NOT_FOUND
 */
tegrabl_error_t tegrabl_dt_is_device_available(const void *fdt, int node,
											bool *res);

/**
 * @brief Get the next 'available' (Refer to tegrabl_dt_is_device_available for
 *		  more info) node with offset > start_node
 *
 * @param fdt Device Tree handle
 * @param start_node Offset of the node to begin traversal with
 * @param res Callee filled. Contains the offset of the next available node
 *
 * @return TEGRABL_NO_ERROR if success, else TEGRABL_ERR_NOT_FOUND
 */
tegrabl_error_t tegrabl_dt_get_next_available(const void *fdt, int start_node,
											int *res);

#define tegrabl_dt_for_each_available_node_from(fdt, node, start)		\
	for (node = start;											\
		tegrabl_dt_get_next_available(fdt, node, &(node)) == TEGRABL_NO_ERROR; \
		node = tegrabl_dt_next_node(fdt, node))

#define tegrabl_dt_for_each_available_node(fdt, node) \
	tegrabl_dt_for_each_available_node_from(fdt, node,\
											tegrabl_dt_next_node(fdt, -1))

/**
 * @brief Check if a given string is enlisted amongst a device's compatibles
 *		  A node can enlist multiple compatibles to offer a range of drivers
 *		  that can be used to handle that device.
 *
 * @param fdt Handle to device tree blob
 * @param node Offset of tegrabl_dt node
 * @param comp Compatible string to check for
 * @param res Callee filled. On success, holds true if the device is
 *		  compatible, else false
 *
 * @return TEGRABL_ERR_NOT_FOUND if the compatible property is not found
 *		   TEGRABL_ERR_NO_MEMORY if insufficient heap memory to hold compatibles
 *		   TEGRABL_NO_ERROR if success
 */
tegrabl_error_t tegrabl_dt_is_device_compatible(const void *fdt, int node,
												const char *comp, bool *res);

/**
 * @brief Retrieve node offset of a node with the given compatible property
 *		  Can be used to iterate over all nodes with a certain compatible
 *
 * @param fdt Handle to device tree blob
 * @param start_offset Node in FDT to start the search from.
 * @param comp Compatible property to search for
 * @param res Callee filled. On success, holds offset of the node that matches
 *		      the criteria.
 *
 * @return TEGRABL_NO_ERROR if success, else TEGRABL_ERR_NOT_FOUND
 */
tegrabl_error_t tegrabl_dt_get_node_with_compatible(const void *fdt,
													int start_offset,
													char *comp, int *res);

/**
 * @brief Add a subnode to the given parent node if absent and
 *		  return the offset of the subnode
 *
 * @param fdt Handle to device tree blob
 * @param parentnode Node in FDT to which the subnode to be added
 * @param nodename Name of the subnode to be added
 *
 * @return offset of the subnode which is added/existing
 */
int tegrabl_add_subnode_if_absent(void *fdt, int parentnode, char *nodename);

#define tegrabl_dt_for_each_compatible_from(fdt, node, start_offset, comp)	\
	for (node = start_offset;												\
		tegrabl_dt_get_node_with_compatible(fdt, node, comp, &(node)) ==	\
														TEGRABL_NO_ERROR;	\
		node = tegrabl_dt_next_node(fdt, node))

#define tegrabl_dt_for_each_compatible(fdt, node, comp)					\
	tegrabl_dt_for_each_compatible_from(fdt, node,						\
										tegrabl_dt_next_node(fdt, -1),	\
										comp)

/**
 * @brief Get alias from node phandle.
 *
 * @param fdt_ptr Pointer to the FDT.
 * @param name name of property
 * @param namep alias_name whose property match.
 * @param lenp Pointer to integer on which length of property will be returned.
 *
 * @return TEGRABL_NO_ERROR in case of success else error code.
 */
tegrabl_error_t tegrabl_get_alias_by_name(const void *fdt_ptr, char *name,
		char *namep, int *lenp);

/**
 * @brief Get alias ID from alias name.
 *
 * @param prefix Prefix in the alias name.
 * @param alias_name Alias string from which ID need to be extracted.
 * @param alias_id Pointer on which Alias ID will be saved.
 *
 * @return TEGRABL_NO_ERROR in case of success else error code.
 */
tegrabl_error_t tegrabl_get_alias_id(char *prefix, char *alias_name,
		int *alias_id);


/**
 * @brief Create DT space by given size for fdt handle
 *
 * @param fdt pointer to fdt handle.
 * @param inc_size increaed size to be created.
 * @param max_size max space size available.
 *
 * @return TEGRABL_NO_ERROR in case of success else error code.
 */
tegrabl_error_t tegrabl_dt_create_space(void *fdt, uint32_t inc_size, uint32_t max_size);

#endif /* __TEGRABL_DEVICETREE_H__ */
