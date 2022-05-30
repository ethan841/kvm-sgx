//#include <linux/module.h>
//#include <linux/version.h>
//#include <engine/graph.h>
#include <engine/gr.h>
#include <core/device.h>
//#include <core/engine/graph/nvc0.h>
#include <nvkm/engine/gr/gf100.h>
#include <drm/ttm/ttm_bo_api.h>
//#include "nouveau_drm.h"
#include "nouveau_drv.h"
#include "nouveau_dma.h"
#include "nouveau_bo.h"
#include "nouveau_chan.h"
#include "nouveau_abi16.h"
#include "gdev_interface.h"

#define VS_START 0x20000000
#define VS_END (1ull << 40)

extern int nouveau_device_count;
extern struct drm_device **nouveau_drm_device;
extern void (*nouveau_callback_notify)(int subc, uint32_t data);

int gdev_drv_vspace_alloc(struct drm_device *drm, uint64_t size, struct gdev_drv_vspace *drv_vspace)
{
    struct nouveau_channel *chan;
    struct nouveau_drm *nvdrm = nouveau_drm(drm);
    u32 arg0, arg1;

    arg0 = 0xbeef0201; /*NvDmaFB*/
    arg1 = 0xbeef0202; /*NvDmaTT*/

    if (nouveau_channel_new(nvdrm, &(nvdrm->master.device),
		NVDRM_CHAN + 2, 
		arg0, arg1, &chan)) {
	printk("Failed to allocate nouveau channel\n");
	return -ENOMEM;
    }

    drv_vspace->priv = (void *)chan;

    return 0;
}
EXPORT_SYMBOL(gdev_drv_vspace_alloc);

int gdev_drv_vspace_free(struct gdev_drv_vspace *drv_vspace)
{
    struct nouveau_channel *chan = (struct nouveau_channel *)drv_vspace->priv;

    nouveau_channel_del(&chan);

    return 0;
}
EXPORT_SYMBOL(gdev_drv_vspace_free);

int gdev_drv_chan_alloc(struct drm_device *drm, struct gdev_drv_vspace *drv_vspace, struct gdev_drv_chan *drv_chan)
{
    struct nouveau_drm *nvdrm = nouveau_drm(drm);
    //struct nouveau_device *nvdev = nv_device(nvdrm->device);
    struct nvkm_device *nvdev;
    struct nouveau_channel *chan = (struct nouveau_channel *)drv_vspace->priv;
    //struct nouveau_fifo_chan *fifo = (void *)chan->object;
    struct nvkm_fifo_chan *fifo;
    struct nouveau_bo *ib_bo, *pb_bo;
    uint32_t cid;
    volatile uint32_t *regs;
    uint32_t *ib_map, *pb_map;
    uint32_t ib_order, pb_order;
    uint64_t ib_base, pb_base;
    uint32_t ib_mask, pb_mask;
    uint32_t pb_size;
    int ret;

    /* channel ID. */
    cid = chan->chid;
/*
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,7,0)
    /* FIFO push buffer setup. 
    pb_order = 15; /* it's hardcoded. pscnv uses 20, nouveau uses 15. 
    pb_bo = chan->push.buffer;
    pb_base = chan->push.vma->addr;
    pb_map = chan->push.buffer->kmap.virtual;
    pb_mask = (1 << pb_order) - 1;
    pb_size = (1 << pb_order);
    if (chan->push.buffer->bo.mem.size / 2 != pb_size)
	printk("Pushbuf size mismatched!\n");

    /* FIFO indirect buffer setup. 
    ib_order = 12; /* it's hardcoded. pscnv uses 9, nouveau uses 12
    ib_bo = NULL;
    ib_base = pb_base + pb_size;
    ib_map = (void *)((unsigned long)pb_bo->kmap.virtual + pb_size);
    ib_mask = (1 << ib_order) - 1;
*/
//#else

    /* FIFO push buffer setup. */
    pb_order = 16; /* it's hardcoded. */
    pb_bo = chan->push.buffer;
    pb_base = chan->push.vma->addr;
    pb_map = chan->push.buffer->kmap.virtual;
    pb_mask = (1 << pb_order) - 1;
    pb_size = (1 << pb_order);
    /* FIFO indirect buffer setup. */
    ib_order = 10; /* it's hardcoded. */
    ib_bo = NULL;
    ib_base = pb_base + pb_size;
    ib_map = (void *)((unsigned long)pb_bo->kmap.virtual + pb_size);
    ib_mask = (1 << ib_order) - 1;

    if (chan->push.buffer->bo.mem.size != pb_size + 0x2000)
	printk("Pushbuf size mismatched!\n");
//#endif

    /* FIFO init: it has already been done in gdev_vas_new(). */

    switch (nvdev->chipset & 0xf0) {
	case 0xc0:
	case 0xe0:
	    /* FIFO command queue registers. */
	    regs = fifo->user;
	    break;
	default:
	    ret = -EINVAL;
	    goto fail_fifo_reg;
    }

    drv_chan->priv = chan;
    drv_chan->cid = cid;
    drv_chan->regs = regs;
    drv_chan->ib_bo = ib_bo;
    drv_chan->ib_map = ib_map;
    drv_chan->ib_order = ib_order;
    drv_chan->ib_base = ib_base;
    drv_chan->ib_mask = ib_mask;
    drv_chan->pb_bo = pb_bo;
    drv_chan->pb_map = pb_map;
    drv_chan->pb_order = pb_order;
    drv_chan->pb_base = pb_base;
    drv_chan->pb_mask = pb_mask;
    drv_chan->pb_size = pb_size;

    return 0;

fail_fifo_reg:
    return ret;
}
EXPORT_SYMBOL(gdev_drv_chan_alloc);

int gdev_drv_chan_free(struct gdev_drv_vspace *drv_vspace, struct gdev_drv_chan *drv_chan)
{
    /* really nothing to do. */
    return 0;
}
EXPORT_SYMBOL(gdev_drv_chan_free);

int gdev_drv_subch_alloc(struct drm_device *drm, void *chan, u32 handle, u16 oclass, void **ctx_obj)
{
    struct nouveau_drm *nvdrm = nouveau_drm(drm);

    //return nouveau_object_new(nv_object(nvdrm),
    return nvkm_object_new(nvdrm->device,
	    NULL, 0,
	    (struct nvkm_object **)ctx_obj);
}
EXPORT_SYMBOL(gdev_drv_subch_alloc);

int gdev_drv_subch_free(struct drm_device *drm, void *chan, u32 handle)
{
    struct nouveau_drm *nvdrm = nouveau_drm(drm);
    
    struct nvkm_object *nvkm_tmp = nvdrm->device->parent;

    nvkm_object_del(&nvkm_tmp);
    //return nouveau_object_del(nv_object(nvdrm),
    return 1;
	    //((struct nouveau_channel *)chan)->handle, handle);
}
EXPORT_SYMBOL(gdev_drv_subch_free);

int gdev_drv_bo_alloc(struct drm_device *drm, uint64_t size, uint32_t drv_flags, struct gdev_drv_vspace *drv_vspace, struct gdev_drv_bo *drv_bo)
{
    struct nouveau_drm *nvdrm = nouveau_drm(drm);
    //struct nouveau_device *nvdev = nv_device(nvdrm->device);
    struct nvkm_device *nvdev;
    struct nouveau_channel *chan = (struct nouveau_channel *)drv_vspace->priv;
    //struct nouveau_client *client;
    struct nvkm_client *client;
    struct nouveau_bo *bo;
    struct nouveau_vma *vma;
    uint32_t flags = 0;
    int ret;

    // ?? dma_resv -> not in nouveau
    struct dma_resv *resv;

    /* ?? need check
    if (chan)
	client = nv_client(chan->cli);
    else -- swap space doesn't have a parent channel, for instance... --
	client = nv_client(&nvdrm->client);
    */

    client = nvdrm->device->client;

    /* set memory type. */
    if (drv_flags & GDEV_DRV_BO_VRAM) {
	flags |= TTM_PL_FLAG_VRAM;
    }
    if (drv_flags & GDEV_DRV_BO_SYSRAM) {
	flags |= TTM_PL_FLAG_TT;
    }

    ret = nouveau_bo_new(&(nvdrm->master), size, 0, flags, 0, 0, NULL, resv, &bo);
    //ret = nouveau_bo_new(&(drm->master), size, 0, flags, 0, 0, NULL, &bo);
    if (ret)
	goto fail_bo_new;

    if (drv_flags & GDEV_DRV_BO_MAPPABLE) {
	ret = nouveau_bo_map(bo);
	if (ret)
	    goto fail_bo_map;
    }
    else
	bo->kmap.virtual = NULL;

    /* allocate virtual address space, if requested. */
    if (drv_flags & GDEV_DRV_BO_VSPACE) {
	if (nvdev->card_type >= NV_50) {
	    vma = kzalloc(sizeof(*vma), GFP_KERNEL);
	    if (!vma) {
		ret = -ENOMEM;
		goto fail_vma_alloc;
	    }

        // not exist!!!
	    //ret = nouveau_bo_vma_add(bo, client->vm, vma);
	    //if (ret)
		//goto fail_vma_add;

	    //drv_bo->addr = vma->offset;
        drv_bo->addr = vma->addr;
	}
	else /* non-supported cards. */
	    drv_bo->addr = 0;
    }
    else
	drv_bo->addr = 0;

    /* address, size, and map. */
    if (bo->kmap.virtual)
	drv_bo->map = bo->kmap.virtual;
    else
	drv_bo->map = NULL;
    drv_bo->size = bo->bo.mem.size;
    drv_bo->priv = bo;

    return 0;

fail_vma_add:
    kfree(vma);
fail_vma_alloc:
    nouveau_bo_unmap(bo);
fail_bo_map:
    nouveau_bo_ref(NULL, &bo);
fail_bo_new:
    return ret;

}
EXPORT_SYMBOL(gdev_drv_bo_alloc);

int gdev_drv_bo_free(struct gdev_drv_vspace *drv_vspace, struct gdev_drv_bo *drv_bo)
{
    struct nouveau_channel *chan = (struct nouveau_channel *)drv_vspace->priv;
    struct drm_device *drm = (struct drm_device *)drv_vspace->drm;
    struct nouveau_drm *nvdrm;
    //struct nouveau_client *client; // = nv_client(chan->cli);
    //struct nouveau_client *client;
    struct nvkm_client *client;
    struct nouveau_bo *bo = (struct nouveau_bo *)drv_bo->priv;
    struct nouveau_vma *vma;
    uint64_t addr = drv_bo->addr;
    void *map = drv_bo->map;

    /*
    if (chan)
	client = nv_client(chan->cli);
    else { /* swap space doesn't have a parent channel, for instance... 
	nvdrm = nouveau_drm(drm);
	client = nv_client(&nvdrm->client);
    }
    */

    client = nvdrm->device->client;

    /* Not exist!!
    if (map && bo->kmap.bo)  dirty validation.. 
	nouveau_bo_unmap(bo);

    if (addr) {
	vma = nouveau_bo_vma_find(bo, client->vm);
	if (vma) {
	    nouveau_bo_vma_del(bo, vma);
	    kfree(vma);
	}
	else {
	    return -ENOENT;
	}
    }
    */

    // not exist!!
    //nouveau_bo_ref(NULL, &bo);

    return 0;
}
EXPORT_SYMBOL(gdev_drv_bo_free);

int gdev_drv_bo_bind(struct drm_device *drm, struct gdev_drv_vspace *drv_vspace, struct gdev_drv_bo *drv_bo)
{
    struct nouveau_drm *nvdrm = nouveau_drm(drm);
    //struct nouveau_device *nvdev = nv_device(nvdrm->device);
    struct nvkm_device *nvdev;
    struct nouveau_channel *chan = (struct nouveau_channel *)drv_vspace->priv;
    //struct nouveau_client *client = nv_client(chan->cli);
    struct nvkm_client *client;
    struct nouveau_bo *bo = (struct nouveau_bo *)drv_bo->priv;
    struct nouveau_vma *vma;
    int ret;

    /* allocate virtual address space, if requested. */
    if (nvdev->card_type >= NV_50) {
	vma = kzalloc(sizeof(*vma), GFP_KERNEL);
	if (!vma) {
	    ret = -ENOMEM;
	    goto fail_vma_alloc;
	}

    //cannot find nouveau_bo_vma_add !!
	//ret = nouveau_bo_vma_add(bo, client->vm, vma);
	//if (ret)
	//    goto fail_vma_add;

	//drv_bo->addr = vma->offset;
    drv_bo->addr = vma->addr;
    }
    else /* non-supported cards. */
	drv_bo->addr = 0;

    drv_bo->map = bo->kmap.virtual; /* could be NULL. */
    drv_bo->size = bo->bo.mem.size;

    return 0;

fail_vma_add:
    kfree(vma);
fail_vma_alloc:
    return ret;
}
EXPORT_SYMBOL(gdev_drv_bo_bind);

int gdev_drv_bo_unbind(struct gdev_drv_vspace *drv_vspace, struct gdev_drv_bo *drv_bo)
{
    struct nouveau_channel *chan = (struct nouveau_channel *)drv_vspace->priv;
    //struct nouveau_client *client = nv_client(chan->cli);
    struct nvkm_client *client;
    struct nouveau_bo *bo = (struct nouveau_bo *)drv_bo->priv;
    struct nouveau_vma *vma;

    /* -> not found func
    vma = nouveau_bo_vma_find(bo, client->vm);
    if (vma) {
	nouveau_bo_vma_del(bo, vma);
	kfree(vma);
    }
    else
	return -ENOENT;
    */

    return 0;
}
EXPORT_SYMBOL(gdev_drv_bo_unbind);

int gdev_drv_bo_map(struct drm_device *drm, struct gdev_drv_bo *drv_bo)
{
    struct nouveau_bo *bo = (struct nouveau_bo *)drv_bo->priv;
    int ret;

    ret = nouveau_bo_map(bo);
    if (ret)
	return ret;

    drv_bo->map = bo->kmap.virtual;

    return 0;
}
EXPORT_SYMBOL(gdev_drv_bo_map);

int gdev_drv_bo_unmap(struct gdev_drv_bo *drv_bo)
{
    struct nouveau_bo *bo = (struct nouveau_bo *)drv_bo->priv;

    if (bo->kmap.bo) /* dirty validation.. */
	nouveau_bo_unmap(bo);
    else
	return -ENOENT;

    return 0;
}
EXPORT_SYMBOL(gdev_drv_bo_unmap);

int gdev_drv_read32(struct drm_device *drm, struct gdev_drv_vspace *drv_vspace, struct gdev_drv_bo *drv_bo, uint64_t offset, uint32_t *p)
{
    if (drv_bo->map)
	*p = ioread32_native(drv_bo->map + offset);
    else
	return -EINVAL;

    return 0;
}
EXPORT_SYMBOL(gdev_drv_read32);

int gdev_drv_write32(struct drm_device *drm, struct gdev_drv_vspace *drv_vspace, struct gdev_drv_bo *drv_bo, uint64_t offset, uint32_t val)
{
    if (drv_bo->map)
	iowrite32_native(val, drv_bo->map + offset);
    else
	return -EINVAL;

    return 0;
}
EXPORT_SYMBOL(gdev_drv_write32);

int gdev_drv_read(struct drm_device *drm, struct gdev_drv_vspace *drv_vspace, struct gdev_drv_bo *drv_bo, uint64_t offset, uint64_t size, void *buf)
{
    if (drv_bo->map)
	memcpy_fromio(buf, drv_bo->map + offset, size);
    else
	return -EINVAL;

    return 0;
}
EXPORT_SYMBOL(gdev_drv_read);

int gdev_drv_write(struct drm_device *drm, struct gdev_drv_vspace *drv_vspace, struct gdev_drv_bo *drv_bo, uint64_t offset, uint64_t size, const void *buf)
{
    if (drv_bo->map)
	memcpy_toio(drv_bo->map + offset, buf, size);
    else
	return -EINVAL;

    return 0;
}
EXPORT_SYMBOL(gdev_drv_write);

int gdev_drv_getdevice(int *count)
{
    *count = nouveau_device_count;
    return 0;
}
EXPORT_SYMBOL(gdev_drv_getdevice);

int gdev_drv_getdrm(int minor, struct drm_device **pptr)
{
    if (minor < nouveau_device_count) {
	if (nouveau_drm_device[minor]) {
	    *pptr = nouveau_drm_device[minor];
	    return 0;
	}
    }

    *pptr = NULL;

    return -ENODEV;
}
EXPORT_SYMBOL(gdev_drv_getdrm);

int gdev_drv_getparam(struct drm_device *drm, uint32_t type, uint64_t *res)
{
    struct drm_nouveau_getparam getparam;
    struct nouveau_drm *nvdrm = nouveau_drm(drm);
    //struct nouveau_device *nvdev = nv_device(nvdrm->device);
    struct nvkm_device *nvdev;
    //struct nouveau_graph *nvgraph = nouveau_graph(nvdrm->device);
    
    //Macro error
    //struct nvkm_gr *nvgraph = nvkm_gr(nvdrm->device);
    struct nvkm_gr *nvgraph;

    int ret = 0;

    switch (type) {
	case GDEV_DRV_GETPARAM_MP_COUNT:
	    if ((nvdev->chipset & 0xf0) == 0xc0) {
		//struct nvc0_graph_priv *priv = (void *) nvgraph;
        struct gf100_gr *priv = (void *) nvgraph;
		*res = priv->tpc_total;
	    }
	    else {
		*res = 0;
		ret = -EINVAL;
	    }
	    break;
	case GDEV_DRV_GETPARAM_FB_SIZE:
	    getparam.param = NOUVEAU_GETPARAM_FB_SIZE;
	    ret = nouveau_abi16_ioctl_getparam(drm, &getparam, NULL);
	    *res = getparam.value;
	    break;
	case GDEV_DRV_GETPARAM_AGP_SIZE:
	    getparam.param = NOUVEAU_GETPARAM_AGP_SIZE;
	    ret = nouveau_abi16_ioctl_getparam(drm, &getparam, NULL);
	    *res = getparam.value;
	    break;
	case GDEV_DRV_GETPARAM_CHIPSET_ID:
	    getparam.param = NOUVEAU_GETPARAM_CHIPSET_ID;
	    ret = nouveau_abi16_ioctl_getparam(drm, &getparam, NULL);
	    *res = getparam.value;
	    break;
	case GDEV_DRV_GETPARAM_BUS_TYPE:
	    getparam.param = NOUVEAU_GETPARAM_BUS_TYPE;
	    ret = nouveau_abi16_ioctl_getparam(drm, &getparam, NULL);
	    *res = getparam.value;
	    break;
	case GDEV_DRV_GETPARAM_PCI_VENDOR:
	    getparam.param = NOUVEAU_GETPARAM_PCI_VENDOR;
	    ret = nouveau_abi16_ioctl_getparam(drm, &getparam, NULL);
	    *res = getparam.value;
	    break;
	case GDEV_DRV_GETPARAM_PCI_DEVICE:
	    getparam.param = NOUVEAU_GETPARAM_PCI_DEVICE;
	    ret = nouveau_abi16_ioctl_getparam(drm, &getparam, NULL);
	    *res = getparam.value;
	    break;
	default:
	    ret = -EINVAL;
    }

    return ret;
}
EXPORT_SYMBOL(gdev_drv_getparam);

int gdev_drv_getaddr(struct drm_device *drm, struct gdev_drv_vspace *drv_vspace, struct gdev_drv_bo *drv_bo, uint64_t offset, uint64_t *addr)
{
    struct nouveau_bo *bo = (struct nouveau_bo *)drv_bo->priv;
    int page = offset / PAGE_SIZE;
    uint32_t x = offset - page * PAGE_SIZE;

    if (drv_bo->map) {
	if (bo->bo.mem.mem_type & TTM_PL_TT)
	    *addr = ((struct ttm_dma_tt *)bo->bo.ttm)->dma_address[page] + x;
	else
	    *addr = bo->bo.mem.bus.base + bo->bo.mem.bus.offset + x;
    }
    else {
	*addr = 0;
    }

    return 0;
}
EXPORT_SYMBOL(gdev_drv_getaddr);

int gdev_drv_setnotify(void (*func)(int subc, uint32_t data))
{
    nouveau_callback_notify = func;
    return 0;
}
EXPORT_SYMBOL(gdev_drv_setnotify);

int gdev_drv_unsetnotify(void (*func)(int subc, uint32_t data))
{
    if (nouveau_callback_notify != func)
	return -EINVAL;
    nouveau_callback_notify = NULL;

    return 0;
}
EXPORT_SYMBOL(gdev_drv_unsetnotify);
