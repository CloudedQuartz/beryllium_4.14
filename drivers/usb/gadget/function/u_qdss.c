/*
 * Copyright (c) 2012-2017, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details
 */

#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/usb_bam.h>
#include <linux/dma-mapping.h>

#include "f_qdss.h"
static int alloc_sps_req(struct usb_ep *data_ep)
{
	struct usb_request *req = NULL;
	struct f_qdss *qdss = data_ep->driver_data;
	u32 sps_params = 0;

	pr_debug("send_sps_req\n");

	req = usb_ep_alloc_request(data_ep, GFP_ATOMIC);
	if (!req) {
		pr_err("usb_ep_alloc_request failed\n");
		return -ENOMEM;
	}

	req->length = 32*1024;
	sps_params = MSM_SPS_MODE | MSM_DISABLE_WB |
			qdss->bam_info.usb_bam_pipe_idx;

	req->udc_priv = sps_params;
	qdss->endless_req = req;

	return 0;
}

static int init_data(struct usb_ep *ep);
int set_qdss_data_connection(struct f_qdss *qdss, int enable)
{
	enum usb_ctrl		usb_bam_type;
	int			res = 0;
	int			idx;
	struct usb_qdss_bam_connect_info bam_info;
	struct usb_gadget *gadget;
	struct device *dev;
	int ret;

	pr_debug("%s\n", __func__);

	if (!qdss) {
		pr_err("%s: qdss ptr is NULL\n", __func__);
		return -EINVAL;
	}

	gadget = qdss->gadget;
	usb_bam_type = usb_bam_get_bam_type(gadget->name);
	dev = gadget->dev.parent;

	bam_info = qdss->bam_info;
	/* There is only one qdss pipe, so the pipe number can be set to 0 */
	idx = usb_bam_get_connection_idx(usb_bam_type, QDSS_P_BAM,
		PEER_PERIPHERAL_TO_USB, USB_BAM_DEVICE, 0);
	if (idx < 0) {
		pr_err("%s: usb_bam_get_connection_idx failed\n", __func__);
		return idx;
	}

	if (enable) {
		ret = get_qdss_bam_info(usb_bam_type, idx,
				&bam_info.qdss_bam_phys,
				&bam_info.qdss_bam_size);
		if (ret) {
			pr_err("%s(): failed to get qdss bam info err(%d)\n",
								__func__, ret);
			return ret;
		}

		bam_info.qdss_bam_iova = dma_map_resource(dev->parent,
				bam_info.qdss_bam_phys, bam_info.qdss_bam_size,
				DMA_BIDIRECTIONAL, 0);
		if (!bam_info.qdss_bam_iova) {
			pr_err("dma_map_resource failed\n");
			return -ENOMEM;
		}

		usb_bam_alloc_fifos(usb_bam_type, idx);
		bam_info.data_fifo =
			kzalloc(sizeof(struct sps_mem_buffer), GFP_KERNEL);
		if (!bam_info.data_fifo) {
			usb_bam_free_fifos(usb_bam_type, idx);
			return -ENOMEM;
		}

		pr_debug("%s(): qdss_bam: iova:%lx p_addr:%lx size:%x\n",
				__func__, bam_info.qdss_bam_iova,
				(unsigned long)bam_info.qdss_bam_phys,
				bam_info.qdss_bam_size);

		get_bam2bam_connection_info(usb_bam_type, idx,
				&bam_info.usb_bam_pipe_idx,
				NULL, bam_info.data_fifo, NULL);

		alloc_sps_req(qdss->port.data);
		msm_data_fifo_config(qdss->port.data,
			bam_info.data_fifo->iova,
			bam_info.data_fifo->size,
			bam_info.usb_bam_pipe_idx);

		init_data(qdss->port.data);

		res = usb_bam_connect(usb_bam_type, idx,
					&(bam_info.usb_bam_pipe_idx),
					bam_info.qdss_bam_iova);
	} else {
		res = usb_bam_disconnect_pipe(usb_bam_type, idx);
		if (res)
			pr_err("usb_bam_disconnection error\n");
		dma_unmap_resource(dev->parent, bam_info.qdss_bam_iova,
				bam_info.qdss_bam_size, DMA_BIDIRECTIONAL, 0);
		usb_bam_free_fifos(usb_bam_type, idx);
		kfree(bam_info.data_fifo);
	}

	return res;
}

static int init_data(struct usb_ep *ep)
{
	struct f_qdss *qdss = ep->driver_data;
	int res = 0;

	/* No EP init required for CI controller: already done in its driver */
	return res;

	pr_debug("%s\n", __func__);

	res = msm_ep_config(ep, qdss->endless_req);
	if (res)
		pr_err("msm_ep_config failed\n");

	return res;
}

int uninit_data(struct usb_ep *ep)
{
	int res = 0;

	return res;

	pr_debug("%s\n", __func__);

	res = msm_ep_unconfig(ep);
	if (res)
		pr_err("msm_ep_unconfig failed\n");

	return res;
}
