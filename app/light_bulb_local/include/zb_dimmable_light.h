/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef ZB_DIMMABLE_LIGHT_H
#define ZB_DIMMABLE_LIGHT_H 1

#define ZB_DIMMABLE_LIGHT_DEVICE_ID 0x0101
#define ZB_DEVICE_VER_DIMMABLE_LIGHT 1
#define ZB_DIMMABLE_LIGHT_IN_CLUSTER_NUM 6
#define ZB_DIMMABLE_LIGHT_OUT_CLUSTER_NUM 0
#define ZB_DIMMABLE_LIGHT_CLUSTER_NUM \
	(ZB_DIMMABLE_LIGHT_IN_CLUSTER_NUM + ZB_DIMMABLE_LIGHT_OUT_CLUSTER_NUM)
#define ZB_DIMMABLE_LIGHT_REPORT_ATTR_COUNT \
	(ZB_ZCL_ON_OFF_REPORT_ATTR_COUNT + ZB_ZCL_LEVEL_CONTROL_REPORT_ATTR_COUNT)
#define ZB_DIMMABLE_LIGHT_CVC_ATTR_COUNT 1

#define ZB_DECLARE_DIMMABLE_LIGHT_CLUSTER_LIST(					   \
	cluster_list_name,							   \
	basic_attr_list,							   \
	identify_attr_list,							   \
	groups_attr_list,							   \
	scenes_attr_list,							   \
	on_off_attr_list,							   \
	level_control_attr_list)						   \
	zb_zcl_cluster_desc_t cluster_list_name[] =				   \
	{									   \
		ZB_ZCL_CLUSTER_DESC(ZB_ZCL_CLUSTER_ID_IDENTIFY,			   \
			ZB_ZCL_ARRAY_SIZE(identify_attr_list, zb_zcl_attr_t),	   \
			(identify_attr_list), ZB_ZCL_CLUSTER_SERVER_ROLE,		   \
			ZB_ZCL_MANUF_CODE_INVALID),				   \
		ZB_ZCL_CLUSTER_DESC(ZB_ZCL_CLUSTER_ID_BASIC,			   \
			ZB_ZCL_ARRAY_SIZE(basic_attr_list, zb_zcl_attr_t),	   \
			(basic_attr_list), ZB_ZCL_CLUSTER_SERVER_ROLE,		   \
			ZB_ZCL_MANUF_CODE_INVALID),				   \
		ZB_ZCL_CLUSTER_DESC(ZB_ZCL_CLUSTER_ID_SCENES,			   \
			ZB_ZCL_ARRAY_SIZE(scenes_attr_list, zb_zcl_attr_t),	   \
			(scenes_attr_list), ZB_ZCL_CLUSTER_SERVER_ROLE,		   \
			ZB_ZCL_MANUF_CODE_INVALID),				   \
		ZB_ZCL_CLUSTER_DESC(ZB_ZCL_CLUSTER_ID_GROUPS,			   \
			ZB_ZCL_ARRAY_SIZE(groups_attr_list, zb_zcl_attr_t),	   \
			(groups_attr_list), ZB_ZCL_CLUSTER_SERVER_ROLE,		   \
			ZB_ZCL_MANUF_CODE_INVALID),				   \
		ZB_ZCL_CLUSTER_DESC(ZB_ZCL_CLUSTER_ID_ON_OFF,			   \
			ZB_ZCL_ARRAY_SIZE(on_off_attr_list, zb_zcl_attr_t),	   \
			(on_off_attr_list), ZB_ZCL_CLUSTER_SERVER_ROLE,		   \
			ZB_ZCL_MANUF_CODE_INVALID),				   \
		ZB_ZCL_CLUSTER_DESC(ZB_ZCL_CLUSTER_ID_LEVEL_CONTROL,		   \
			ZB_ZCL_ARRAY_SIZE(level_control_attr_list, zb_zcl_attr_t), \
			(level_control_attr_list), ZB_ZCL_CLUSTER_SERVER_ROLE,	   \
			ZB_ZCL_MANUF_CODE_INVALID)				   \
	}

#define ZB_ZCL_DECLARE_HA_DIMMABLE_LIGHT_SIMPLE_DESC(ep_name, ep_id, in_clust_num, out_clust_num) \
	ZB_DECLARE_SIMPLE_DESC(in_clust_num, out_clust_num);					  \
	ZB_AF_SIMPLE_DESC_TYPE(in_clust_num, out_clust_num) simple_desc_##ep_name =		  \
	{											  \
		ep_id,										  \
		ZB_AF_HA_PROFILE_ID,								  \
		ZB_DIMMABLE_LIGHT_DEVICE_ID,							  \
		ZB_DEVICE_VER_DIMMABLE_LIGHT,							  \
		0,										  \
		in_clust_num,									  \
		out_clust_num,									  \
		{										  \
			ZB_ZCL_CLUSTER_ID_BASIC,						  \
			ZB_ZCL_CLUSTER_ID_IDENTIFY,						  \
			ZB_ZCL_CLUSTER_ID_SCENES,						  \
			ZB_ZCL_CLUSTER_ID_GROUPS,						  \
			ZB_ZCL_CLUSTER_ID_ON_OFF,						  \
			ZB_ZCL_CLUSTER_ID_LEVEL_CONTROL,					  \
		}										  \
	}

#define ZB_DECLARE_DIMMABLE_LIGHT_EP(ep_name, ep_id, cluster_list)		      \
	ZB_ZCL_DECLARE_HA_DIMMABLE_LIGHT_SIMPLE_DESC(ep_name, ep_id,		      \
		ZB_DIMMABLE_LIGHT_IN_CLUSTER_NUM, ZB_DIMMABLE_LIGHT_OUT_CLUSTER_NUM); \
	ZBOSS_DEVICE_DECLARE_REPORTING_CTX(reporting_info## ep_name,		      \
		ZB_DIMMABLE_LIGHT_REPORT_ATTR_COUNT);				      \
	ZBOSS_DEVICE_DECLARE_LEVEL_CONTROL_CTX(cvc_alarm_info## ep_name,	      \
		ZB_DIMMABLE_LIGHT_CVC_ATTR_COUNT);				      \
	ZB_AF_DECLARE_ENDPOINT_DESC(ep_name, ep_id, ZB_AF_HA_PROFILE_ID,	      \
		0, NULL,							      \
		ZB_ZCL_ARRAY_SIZE(cluster_list, zb_zcl_cluster_desc_t), cluster_list, \
		(zb_af_simple_desc_1_1_t *)&simple_desc_## ep_name,		      \
		ZB_DIMMABLE_LIGHT_REPORT_ATTR_COUNT, reporting_info## ep_name,	      \
		ZB_DIMMABLE_LIGHT_CVC_ATTR_COUNT, cvc_alarm_info## ep_name)

#endif
