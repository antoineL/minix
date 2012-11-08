
/* This file implements the methods of memory-mapped files. */

#include <assert.h>

#include "proto.h"
#include "vm.h"
#include "region.h"
#include "glo.h"
#include "cache.h"

/* These functions are static so as to not pollute the
 * global namespace, and are accessed through their function
 * pointers.
 */

static void mappedfile_split(struct vmproc *vmp, struct vir_region *vr,
	struct vir_region *r1, struct vir_region *r2);
static int mappedfile_unreference(struct phys_region *pr);
static int mappedfile_pagefault(struct vmproc *vmp, struct vir_region *region, 
       struct phys_region *ph, int write, vfs_callback_t callback, void *, int);
static int mappedfile_sanitycheck(struct phys_region *pr, char *file, int line);
static int mappedfile_writable(struct phys_region *pr);
static int mappedfile_copy(struct vir_region *vr, struct vir_region *newvr);

#if 0
static void mappedfile_delete(struct vir_region *region);
#endif

struct mem_type mem_type_mappedfile = {
	.name = "file-mapped memory",
	.ev_unreference = mappedfile_unreference,
	.ev_pagefault = mappedfile_pagefault,
	.ev_sanitycheck = mappedfile_sanitycheck,
	.ev_copy = mappedfile_copy,
	.writable = mappedfile_writable,
	.ev_split = mappedfile_split,
#if 0
	.ev_delete = mappedfile_delete
#endif
};

#if 0
static void mappedfile_delete(struct vir_region *region)
{
	/* let's not do this.. also doesn't work with split */
	int vmfd = region->param.file.fd;

	assert(region->param.file.inited);
	assert(region->param.file.dev != NO_DEV);

	vfs_request(VMVFSREQ_FDCLOSE, vmfd, 
		&vmproc[VM_PROC_NR], 0, 0, NULL, NULL, NULL, 0);
}
#endif

static int mappedfile_unreference(struct phys_region *pr)
{
	assert(pr->ph->refcount == 0);
	if(pr->ph->phys != MAP_NONE)
		free_mem(ABS2CLICK(pr->ph->phys), 1);
	return OK;
}

static int cow_block(struct vmproc *vmp, struct vir_region *region,
	struct phys_region *ph, u16_t clearend)
{
	int r;

#if 0
	printf("mappedfile_pagefault: doing COW\n");
#endif

	if((r=mem_cow(region, ph, MAP_NONE, MAP_NONE)) != OK) {
		printf("mappedfile_pagefault: COW failed\n");
		return r;
	}

	/* After COW we are a normal piece of anonymous memory. */
	ph->memtype = &mem_type_anon;

	if(clearend) {
		phys_bytes phaddr = ph->ph->phys, po = VM_PAGE_SIZE-clearend;
		assert(clearend < VM_PAGE_SIZE);
		phaddr += po;
		if(sys_memset(NONE, 0, phaddr, clearend) != OK) {
			panic("cow_block: clearend failed\n");
		}
#if 0
		printf("clearended vaddr 0x%lx-0x%lx\n",
			region->vaddr+ph->offset+po,
			region->vaddr+ph->offset+po+clearend);
#endif
	}

	return OK;
}

static int mappedfile_pagefault(struct vmproc *vmp, struct vir_region *region,
	struct phys_region *ph, int write, vfs_callback_t cb,
	void *state, int statelen)
{
	u32_t allocflags;
	int procfd = region->param.file.procfd;

	allocflags = vrallocflags(region->flags);

	assert(ph->ph->refcount > 0);
	assert(region->param.file.inited);
	assert(region->param.file.dev != NO_DEV);

	/* Totally new block? Create it. */
	if(ph->ph->phys == MAP_NONE) {
		struct cached_page *cp;
		u64_t referenced_offset =
			region->param.file.offset + ph->offset;
		if(region->param.file.ino == VMC_NO_INODE) {
			cp = find_cached_page_bydev(region->param.file.dev,
				referenced_offset, VMC_NO_INODE, 0, 1);
		} else {
			cp = find_cached_page_byino(region->param.file.dev,
				region->param.file.ino, referenced_offset, 1);
		}
		if(cp) {
			int result = OK;
			pb_unreferenced(region, ph, 0);
			pb_link(ph, cp->page, ph->offset, region);
#if 0
			printf("VM: mmapped ino %u offset %llx dev 0x%x found\n",
				region->param.file.ino, referenced_offset,
				region->param.file.dev);
#endif

			if(roundup(ph->offset+region->param.file.clearend,
				VM_PAGE_SIZE) >= region->length) {
#if 0
				printf("clearend of 0x%x hit, offset 0x%lx, len 0x%lx\n",
					region->param.file.clearend,
					ph->offset, region->length);
#endif
				result = cow_block(vmp, region, ph,
					region->param.file.clearend);
			} else if(result == OK && write) {
				result = cow_block(vmp, region, ph, 0);
			}

			return result;
		}

#if 0
		printf("VM: mmapped ino %u offset %llx dev 0x%x not found\n",
			region->param.file.ino, referenced_offset,
			region->param.file.dev);
#endif

		if(!cb) {
			printf("VM: mem_file: no callback, returning EFAULT\n");
			sys_sysctl_stacktrace(vmp->vm_endpoint);
			return EFAULT;
		}

                if(vfs_request(VMVFSREQ_FDIO, procfd, vmp, referenced_offset,
			VM_PAGE_SIZE, cb, NULL, state, statelen) != OK) {
			printf("VM: mappedfile_pagefault: vfs_request failed\n");
			return ENOMEM;
		}
#if 0
		else {
			printf("VM: mappedfile_pagefault: vfs_request done\n");
		}
#endif

		return SUSPEND;
	}

	if(!write) {
		printf("mappedfile_pagefault: nonwrite fault?\n");
		return EFAULT;
	}

	return cow_block(vmp, region, ph, 0);
}

static int mappedfile_sanitycheck(struct phys_region *pr, char *file, int line)
{
	MYASSERT(usedpages_add(pr->ph->phys, VM_PAGE_SIZE) == OK);
	return OK;
}

static int mappedfile_writable(struct phys_region *pr)
{
	/* We are never writable. */
	return 0;
}

int mappedfile_copy(struct vir_region *vr, struct vir_region *newvr)
{
	assert(vr->param.file.inited);
	mappedfile_setfile(newvr, vr->param.file.procfd, vr->param.file.offset,
		vr->param.file.dev, vr->param.file.ino, vr->param.file.clearend, 0);
	assert(newvr->param.file.inited);

	return OK;
}

void mappedfile_setfile(struct vir_region *region, int fd, u64_t offset,
	dev_t dev, ino_t ino, u16_t clearend, int prefill)
{
	vir_bytes vaddr;

	assert(!region->param.file.inited);
	assert(dev != NO_DEV);
	region->param.file.procfd = fd;
	region->param.file.dev = dev;
	region->param.file.ino = ino;
	region->param.file.offset = offset;
	region->param.file.clearend = clearend;
	region->param.file.inited = 1;

	if(!prefill) return;

	for(vaddr = 0; vaddr < region->length; vaddr+=VM_PAGE_SIZE) {
		struct cached_page *cp = NULL;
		struct phys_region *pr;
		u64_t referenced_offset = offset + vaddr;

		if(roundup(vaddr+region->param.file.clearend,
			VM_PAGE_SIZE) >= region->length) {
#if 0
			printf("mappedfile_setfile: stopping at vaddr 0x%lx/0x%lx, clearend 0x%x\n",
				vaddr, region->length, clearend);
#endif
			break;
		}

		if(ino == VMC_NO_INODE) {
			cp = find_cached_page_bydev(dev, referenced_offset,
			  	VMC_NO_INODE, 0, 1);
		} else {
			cp = find_cached_page_byino(dev, ino,
				referenced_offset, 1);
		}
		if(!cp) continue;
		if(!(pr = pb_reference(cp->page, vaddr, region,
			&mem_type_mappedfile))) {
			printf("mappedfile_setfile: pb_reference failed\n");
			break;
		}
		if(map_ph_writept(region->parent, region, pr) != OK) {
			printf("mappedfile_setfile: map_ph_writept failed\n");
			break;
		}
	}
}

static void mappedfile_split(struct vmproc *vmp, struct vir_region *vr,
	struct vir_region *r1, struct vir_region *r2)
{
	assert(!r1->param.file.inited);
	assert(!r2->param.file.inited);
	assert(vr->param.file.inited);
	assert(r1->length + r2->length == vr->length);
	assert(vr->def_memtype == &mem_type_mappedfile);
	assert(r1->def_memtype == &mem_type_mappedfile);
	assert(r2->def_memtype == &mem_type_mappedfile);

	r1->param.file = vr->param.file;
	r2->param.file = vr->param.file;

	r1->param.file.clearend = 0;
	r2->param.file.offset += r1->length;

	assert(r1->param.file.inited);
	assert(r2->param.file.inited);
}

