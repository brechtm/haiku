/*
** Copyright 2001-2002, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/

// 280202 TK: added partition support

#include <kernel.h>
#include <vfs.h>
#include <debug.h>
#include <khash.h>
#include <memheap.h>
#include <lock.h>
#include <vm.h>
#include <Errors.h>
#include <kerrors.h>
#include <Drivers.h>
#include <sys/stat.h>

#include <arch/cpu.h>
#include <devfs.h>

#include <string.h>
#include <stdio.h>


#define DEVFS_TRACE 1

#if DEVFS_TRACE
#	define TRACE(x) dprintf x
#	define INSANE(x) dprintf x
#else
#	define TRACE(x)
#	define INSANE(x)
#endif

typedef enum {
	STREAM_TYPE_DIR = S_IFDIR,
	STREAM_TYPE_DEVICE = S_IFCHR,
	STREAM_TYPE_SYMLINK = S_IFLNK
} stream_type;

struct devfs_part_map {
	off_t offset;
	off_t size;
	uint32 logical_block_size;
	struct devfs_vnode *raw_vnode;
	
	// following info could be recreated on the fly, but it's not worth it
	uint32 session;
	uint32 partition;
};

struct devfs_stream {
	stream_type type;
	union {
		struct stream_dir {
			struct devfs_vnode *dir_head;
			struct devfs_cookie *jar_head;
		} dir;
		struct stream_dev {
			void *ident;
			device_hooks *calls;
			struct devfs_part_map *part_map;
		} dev;
		struct stream_symlink {
			char *path;
		} symlink;
	} u;
};

struct devfs_vnode {
	struct devfs_vnode *all_next;
	vnode_id id;
	char *name;
	struct devfs_vnode *parent;
	struct devfs_vnode *dir_next;
	struct devfs_stream stream;
};

struct devfs {
	fs_id id;
	mutex lock;
	int next_vnode_id;
	void *vnode_list_hash;
	struct devfs_vnode *root_vnode;
};

struct devfs_cookie {
	struct devfs_stream *stream;
	int oflags;
	union {
		struct cookie_dir {
			struct devfs_cookie *next;
			struct devfs_cookie *prev;
			struct devfs_vnode *ptr;
		} dir;
		struct cookie_dev {
			void *dcookie;
		} dev;
	} u;
};

/* the one and only allowed devfs instance */
static struct devfs *gDeviceFileSystem = NULL;

#define BOOTFS_HASH_SIZE 16


static unsigned int
devfs_vnode_hash_func(void *_v, const void *_key, unsigned int range)
{
	struct devfs_vnode *v = _v;
	const vnode_id *key = _key;

	if (v != NULL)
		return v->id % range;

	return (*key) % range;
}


static int
devfs_vnode_compare_func(void *_v, const void *_key)
{
	struct devfs_vnode *v = _v;
	const vnode_id *key = _key;

	if (v->id == *key)
		return 0;

	return -1;
}


static struct devfs_vnode *
devfs_create_vnode(struct devfs *fs, const char *name)
{
	struct devfs_vnode *v;

	v = kmalloc(sizeof(struct devfs_vnode));
	if (v == NULL)
		return NULL;

	memset(v, 0, sizeof(struct devfs_vnode));
	v->id = fs->next_vnode_id++;

	v->name = kstrdup(name);
	if (v->name == NULL) {
		kfree(v);
		return NULL;
	}

	return v;
}


static status_t
devfs_delete_vnode(struct devfs *fs, struct devfs_vnode *v, bool force_delete)
{
	// cant delete it if it's in a directory or is a directory
	// and has children
	if (!force_delete
		&& ((v->stream.type == STREAM_TYPE_DIR && v->stream.u.dir.dir_head != NULL)
			|| v->dir_next != NULL))
		return EPERM;

	// remove it from the global hash table
	hash_remove(fs->vnode_list_hash, v);

	// TK: for partitions, we have to release the raw device
	if (v->stream.type == STREAM_TYPE_DEVICE && v->stream.u.dev.part_map)
		vfs_put_vnode(fs->id, v->stream.u.dev.part_map->raw_vnode->id);

	if (v->name != NULL)
		kfree(v->name);
	kfree(v);

	return 0;
}


static void
insert_cookie_in_jar(struct devfs_vnode *dir, struct devfs_cookie *cookie)
{
	cookie->u.dir.next = dir->stream.u.dir.jar_head;
	dir->stream.u.dir.jar_head = cookie;
	cookie->u.dir.prev = NULL;
}


static void
remove_cookie_from_jar(struct devfs_vnode *dir, struct devfs_cookie *cookie)
{
	if (cookie->u.dir.next)
		cookie->u.dir.next->u.dir.prev = cookie->u.dir.prev;
	if (cookie->u.dir.prev)
		cookie->u.dir.prev->u.dir.next = cookie->u.dir.next;
	if (dir->stream.u.dir.jar_head == cookie)
		dir->stream.u.dir.jar_head = cookie->u.dir.next;

	cookie->u.dir.prev = cookie->u.dir.next = NULL;
}


/* makes sure none of the dircookies point to the vnode passed in */
static void
update_dircookies(struct devfs_vnode *dir, struct devfs_vnode *v)
{
	struct devfs_cookie *cookie;

	for (cookie = dir->stream.u.dir.jar_head; cookie; cookie = cookie->u.dir.next) {
		if (cookie->u.dir.ptr == v)
			cookie->u.dir.ptr = v->dir_next;
	}
}


static struct devfs_vnode *
devfs_find_in_dir(struct devfs_vnode *dir, const char *path)
{
	struct devfs_vnode *v;

	if (dir->stream.type != STREAM_TYPE_DIR)
		return NULL;

	if (!strcmp(path, "."))
		return dir;
	if (!strcmp(path, ".."))
		return dir->parent;

	for (v = dir->stream.u.dir.dir_head; v; v = v->dir_next) {
		INSANE(("devfs_find_in_dir: looking at entry '%s'\n", v->name));
		if (strcmp(v->name, path) == 0) {
			INSANE(("devfs_find_in_dir: found it at %p\n", v));
			return v;
		}
	}
	return NULL;
}


static status_t
devfs_insert_in_dir(struct devfs_vnode *dir, struct devfs_vnode *v)
{
	if (dir->stream.type != STREAM_TYPE_DIR)
		return EINVAL;

	v->dir_next = dir->stream.u.dir.dir_head;
	dir->stream.u.dir.dir_head = v;

	v->parent = dir;
	return 0;
}


static status_t
devfs_remove_from_dir(struct devfs_vnode *dir, struct devfs_vnode *findit)
{
	struct devfs_vnode *v;
	struct devfs_vnode *last_v;

	for (v = dir->stream.u.dir.dir_head, last_v = NULL; v; last_v = v, v = v->dir_next) {
		if (v == findit) {
			/* make sure all dircookies dont point to this vnode */
			update_dircookies(dir, v);

			if (last_v)
				last_v->dir_next = v->dir_next;
			else
				dir->stream.u.dir.dir_head = v->dir_next;
			v->dir_next = NULL;
			return 0;
		}
	}
	return -1;
}


/* XXX seems to be unused
static bool
devfs_is_dir_empty(struct devfs_vnode *dir)
{
	if (dir->stream.type != STREAM_TYPE_DIR)
		return false;

	return !dir->stream.u.dir.dir_head;
}
*/


static status_t
devfs_get_partition_info( struct devfs *fs, struct devfs_vnode *v, 
	struct devfs_cookie *cookie, void *buf, size_t len)
{
	devfs_partition_info *info = (devfs_partition_info *)buf;
	struct devfs_part_map *part_map = v->stream.u.dev.part_map;

	if (v->stream.type != STREAM_TYPE_DEVICE || part_map == NULL)
		return EINVAL;

	info->offset = part_map->offset;
	info->size = part_map->size;
	info->logical_block_size = part_map->logical_block_size;
	info->session = part_map->session;
	info->partition = part_map->partition;

	// XXX: todo - create raw device name out of raw_vnode 
	//             we need vfs support for that (see vfs_get_cwd)
	strcpy(info->raw_device, "something_raw");
	
	return B_NO_ERROR;
}


static status_t
devfs_set_partition( struct devfs *fs, struct devfs_vnode *v, 
	struct devfs_cookie *cookie, void *buf, size_t len)
{
	struct devfs_part_map *part_map;
	struct devfs_vnode *part_node;
	int res;
	char part_name[30];
	devfs_partition_info info;
	
	info = *(devfs_partition_info *)buf;
	
	if (v->stream.type != STREAM_TYPE_DEVICE)
		return EINVAL;
		
	// we don't support nested partitions
	if (v->stream.u.dev.part_map)
		return EINVAL;
	
	// reduce checks to a minimum - things like negative offsets could be useful
	if (info.size < 0)
		return EINVAL;
				
	// create partition map
	part_map = kmalloc(sizeof(*part_map));
	if (!part_map)
		return ENOMEM;
		
	part_map->offset = info.offset;
	part_map->size = info.size;
	part_map->logical_block_size = info.logical_block_size;
	part_map->session = info.session;
	part_map->partition = info.partition;
		
	sprintf(part_name, "%i_%i", info.session, info.partition);

	mutex_lock(&gDeviceFileSystem->lock);
	
	// you cannot change a partition once set
	if (devfs_find_in_dir( v->parent, part_name)) {
		res = EINVAL;
		goto err1;
	}
	
	// increase reference count of raw device - 
	// the partition device really needs it 
	// (at least to resolve its name on GET_PARTITION_INFO)
	res = vfs_get_vnode(fs->id, v->id, (fs_vnode *)&part_map->raw_vnode);
	if (res < 0)
		goto err1;

	// now create the partition node	
	part_node = devfs_create_vnode(fs, part_name);
	
	if (part_node == NULL) {
		res = ENOMEM;
		goto err2;
	}

	part_node->stream.type = STREAM_TYPE_DEVICE;
	part_node->stream.u.dev.ident = v->stream.u.dev.ident;
	part_node->stream.u.dev.calls = v->stream.u.dev.calls;
	part_node->stream.u.dev.part_map = part_map;

	hash_insert(fs->vnode_list_hash, part_node);

	devfs_insert_in_dir(v->parent, part_node);

	mutex_unlock(&gDeviceFileSystem->lock);
	
	TRACE(("SET_PARTITION: Added partition\n"));

	return B_NO_ERROR;
	
err1:
	mutex_unlock(&gDeviceFileSystem->lock);

	kfree(part_map);
	return res;
		
err2:
	mutex_unlock(&gDeviceFileSystem->lock);

	vfs_put_vnode(fs->id, v->id);
	kfree(part_map);
	return res;
}


//	#pragma mark -


static status_t
devfs_mount(fs_id id, const char *devfs, void *args, fs_cookie *_fs, vnode_id *root_vnid)
{
	struct devfs *fs;
	struct devfs_vnode *v;
	status_t err;

	TRACE(("devfs_mount: entry\n"));

	if (gDeviceFileSystem) {
		dprintf("double mount of devfs attempted\n");
		err = ERR_GENERAL;
		goto err;
	}

	fs = kmalloc(sizeof(struct devfs));
	if (fs == NULL) {
		err = ENOMEM;
		goto err;
	}

	fs->id = id;
	fs->next_vnode_id = 0;

	err = mutex_init(&fs->lock, "devfs_mutex");
	if (err < 0) {
		goto err1;
	}

	fs->vnode_list_hash = hash_init(BOOTFS_HASH_SIZE, (addr)&v->all_next - (addr)v,
		&devfs_vnode_compare_func, &devfs_vnode_hash_func);
	if (fs->vnode_list_hash == NULL) {
		err = ENOMEM;
		goto err2;
	}

	// create a vnode
	v = devfs_create_vnode(fs, "");
	if (v == NULL) {
		err = ENOMEM;
		goto err3;
	}

	// set it up
	v->parent = v;

	// create a dir stream for it to hold
	v->stream.type = STREAM_TYPE_DIR;
	v->stream.u.dir.dir_head = NULL;
	v->stream.u.dir.jar_head = NULL;
	fs->root_vnode = v;

	hash_insert(fs->vnode_list_hash, v);

	*root_vnid = v->id;
	*_fs = fs;

	gDeviceFileSystem = fs;

	return 0;

	devfs_delete_vnode(fs, v, true);
err3:
	hash_uninit(fs->vnode_list_hash);
err2:
	mutex_destroy(&fs->lock);
err1:
	kfree(fs);
err:
	return err;
}


static status_t
devfs_unmount(fs_cookie _fs)
{
	struct devfs *fs = _fs;
	struct devfs_vnode *v;
	struct hash_iterator i;

	TRACE(("devfs_unmount: entry fs = %p\n", fs));

	// delete all of the vnodes
	hash_open(fs->vnode_list_hash, &i);
	while((v = (struct devfs_vnode *)hash_next(fs->vnode_list_hash, &i)) != NULL) {
		devfs_delete_vnode(fs, v, true);
	}
	hash_close(fs->vnode_list_hash, &i, false);

	hash_uninit(fs->vnode_list_hash);
	mutex_destroy(&fs->lock);
	kfree(fs);

	return 0;
}


static status_t
devfs_sync(fs_cookie fs)
{
	TRACE(("devfs_sync: entry\n"));

	return 0;
}


static status_t
devfs_lookup(fs_cookie _fs, fs_vnode _dir, const char *name, vnode_id *_id, int *_type)
{
	struct devfs *fs = (struct devfs *)_fs;
	struct devfs_vnode *dir = (struct devfs_vnode *)_dir;
	struct devfs_vnode *vnode, *vdummy;
	status_t err;

	TRACE(("devfs_lookup: entry dir %p, name '%s'\n", dir, name));

	if (dir->stream.type != STREAM_TYPE_DIR)
		return B_NOT_A_DIRECTORY;

	mutex_lock(&fs->lock);

	// look it up
	vnode = devfs_find_in_dir(dir, name);
	if (!vnode) {
		err = B_ENTRY_NOT_FOUND;
		goto err;
	}

	err = vfs_get_vnode(fs->id, vnode->id, (fs_vnode *)&vdummy);
	if (err < 0)
		goto err;

	*_id = vnode->id;
	*_type = vnode->stream.type;

err:
	mutex_unlock(&fs->lock);

	return err;
}


static status_t
devfs_get_vnode_name(fs_cookie _fs, fs_vnode _vnode, char *buffer, size_t bufferSize)
{
	struct devfs_vnode *vnode = (struct devfs_vnode *)_vnode;

	TRACE(("devfs_get_vnode_name: vnode = %p\n",vnode));
	
	strlcpy(buffer,vnode->name,bufferSize);
	return B_OK;
}


static status_t
devfs_get_vnode(fs_cookie _fs, vnode_id id, fs_vnode *_vnode, bool reenter)
{
	struct devfs *fs = (struct devfs *)_fs;

	TRACE(("devfs_get_vnode: asking for vnode id = %Ld, vnode = %p, r %d\n", id, _vnode, reenter));

	if (!reenter)
		mutex_lock(&fs->lock);

	*_vnode = hash_lookup(fs->vnode_list_hash, &id);

	if (!reenter)
		mutex_unlock(&fs->lock);

	TRACE(("devfs_get_vnode: looked it up at %p\n", *_vnode));

	if (*_vnode)
		return 0;

	return ERR_NOT_FOUND;
}


static status_t
devfs_put_vnode(fs_cookie _fs, fs_vnode _v, bool reenter)
{
#if DEVFS_TRACE
	struct devfs_vnode *vnode = (struct devfs_vnode *)_v;

	TRACE(("devfs_put_vnode: entry on vnode %p, id = %Ld, reenter %d\n", vnode, vnode->id, reenter));
#endif

	return 0; // whatever
}


static status_t
devfs_remove_vnode(fs_cookie _fs, fs_vnode _v, bool reenter)
{
	struct devfs *fs = (struct devfs *)_fs;
	struct devfs_vnode *vnode = (struct devfs_vnode *)_v;

	TRACE(("devfs_removevnode: remove %p (%Ld), reenter %d\n", vnode, vnode->id, reenter));

	if (!reenter)
		mutex_lock(&fs->lock);

	if (vnode->dir_next) {
		// can't remove node if it's linked to the dir
		panic("devfs_removevnode: vnode %p asked to be removed is present in dir\n", vnode);
	}

	devfs_delete_vnode(fs, vnode, false);

	if (!reenter)
		mutex_unlock(&fs->lock);

	return B_OK;
}


static status_t
devfs_create(fs_cookie _fs, fs_vnode _dir, const char *name, int omode, int perms, file_cookie *_cookie, vnode_id *new_vnid)
{
	return EROFS;
}


static status_t
devfs_open(fs_cookie _fs, fs_vnode _v, int oflags, file_cookie *_cookie)
{
	struct devfs *fs = _fs;
	struct devfs_vnode *vnode = _v;
	struct devfs_cookie *cookie;
	status_t status = 0;

	TRACE(("devfs_open: fs_cookie %p vnode %p, oflags 0x%x, file_cookie %p \n", fs, vnode, oflags, _cookie));

	cookie = kmalloc(sizeof(struct devfs_cookie));
	if (cookie == NULL)
		return ENOMEM;

	if (vnode->stream.type != STREAM_TYPE_DEVICE)
		return EINVAL;

	status = vnode->stream.u.dev.calls->open(vnode->name, oflags, &cookie->u.dev.dcookie);
	*_cookie = cookie;

	return status;
}


static status_t
devfs_close(fs_cookie _fs, fs_vnode _v, file_cookie _cookie)
{
	struct devfs_vnode *vnode = _v;
	struct devfs_cookie *cookie = _cookie;

	TRACE(("devfs_close: entry vnode %p, cookie %p\n", vnode, cookie));

	if (vnode->stream.type == STREAM_TYPE_DEVICE) {
		// pass the call through to the underlying device
		return vnode->stream.u.dev.calls->close(cookie->u.dev.dcookie);
	}

	return B_OK;
}


static status_t
devfs_free_cookie(fs_cookie _fs, fs_vnode _v, file_cookie _cookie)
{
	struct devfs_vnode *vnode = _v;
	struct devfs_cookie *cookie = _cookie;

	TRACE(("devfs_freecookie: entry vnode %p, cookie %p\n", vnode, cookie));

	if (vnode->stream.type == STREAM_TYPE_DEVICE) {
		// pass the call through to the underlying device
		vnode->stream.u.dev.calls->free(cookie->u.dev.dcookie);
	}

	if (cookie)
		kfree(cookie);

	return 0;
}


static status_t
devfs_fsync(fs_cookie _fs, fs_vnode _v)
{
	return 0;
}


static ssize_t
devfs_read(fs_cookie _fs, fs_vnode _v, file_cookie _cookie, off_t pos, void *buffer, size_t *length)
{
	struct devfs *fs = _fs;
	struct devfs_vnode *vnode = _v;
	struct devfs_cookie *cookie = _cookie;
	struct devfs_part_map *part_map;

	TRACE(("devfs_read: vnode %p, cookie %p, pos %Ld, len %p\n", vnode, cookie, pos, length));

	// Whoa! If the next to lines are uncommented, our kernel crashes at some point
	// I haven't yet found the time to investigate, but I'll doubtlessly have to do
	// that at some point -- axeld.
	//if (cookie->stream->type != STREAM_TYPE_DEVICE)
	//	return EINVAL;

	part_map = vnode->stream.u.dev.part_map;
	if (part_map) {
		if (pos < 0)
			pos = 0;

		if (pos > part_map->size)
			return 0;

		*length = min(*length, part_map->size - pos );
		pos += part_map->offset;
	}

	// pass the call through to the device
	return vnode->stream.u.dev.calls->read(cookie->u.dev.dcookie, pos, buffer, length);
}


static ssize_t
devfs_write(fs_cookie _fs, fs_vnode _v, file_cookie _cookie, off_t pos, const void *buf, size_t *len)
{
	struct devfs_vnode *vnode = _v;
	struct devfs_cookie *cookie = _cookie;
	
	TRACE(("devfs_write: vnode %p, cookie %p, pos %Ld, len %p\n", vnode, cookie, pos, len));

	if (vnode->stream.type == STREAM_TYPE_DEVICE) {
		struct devfs_part_map *part_map = vnode->stream.u.dev.part_map;
		int written;

		if (part_map) {
			if (pos < 0)
				pos = 0;

			if (pos > part_map->size)
				return 0;

			*len = min(*len, part_map->size - pos);
			pos += part_map->offset;
		}

		written = vnode->stream.u.dev.calls->write(cookie->u.dev.dcookie, pos, buf, len);
		return written;
	}
	return EINVAL;
}


static off_t
devfs_seek(fs_cookie _fs, fs_vnode _v, file_cookie _cookie, off_t pos, int seekType)
{
#ifdef DEBUG
	struct devfs *fs = _fs;
	struct devfs_vnode *vnode = _v;
	struct devfs_cookie *cookie = _cookie;

	TRACE(("devfs_seek: vnode %p, cookie %p, pos %Ld, seekType %d\n", vnode, cookie, pos, seekType));
#endif

	return ESPIPE;
}


static status_t
devfs_create_dir(fs_cookie _fs, fs_vnode _dir, const char *name, int perms, vnode_id *new_vnid)
{
	return EROFS;
}


static status_t
devfs_open_dir(fs_cookie _fs, fs_vnode _v, file_cookie *_cookie)
{
	struct devfs *fs = _fs;
	struct devfs_vnode *vnode = _v;
	struct devfs_cookie *cookie;

	TRACE(("devfs_open_dir: vnode %p\n", vnode));

	if (vnode->stream.type != STREAM_TYPE_DIR)
		return EINVAL;

	cookie = kmalloc(sizeof(struct devfs_cookie));
	if (cookie == NULL)
		return ENOMEM;

	mutex_lock(&fs->lock);

	cookie->stream = &vnode->stream;
	cookie->u.dir.ptr = vnode->stream.u.dir.dir_head;
	*_cookie = cookie;

	mutex_unlock(&fs->lock);
	return B_OK;
}


static status_t
devfs_read_dir(fs_cookie _fs, fs_vnode _vnode, file_cookie _cookie, struct dirent *dirent, size_t bufferSize, uint32 *_num)
{
	struct devfs_cookie *cookie = _cookie;
	struct devfs *fs = _fs;
	status_t status = 0;

	TRACE(("devfs_read_dir: vnode %p, cookie %p, buffer %p, size %ld\n", _vnode, cookie, dirent, bufferSize));

	if (cookie->stream->type != STREAM_TYPE_DIR)
		return EINVAL;

	mutex_lock(&fs->lock);

	if (cookie->u.dir.ptr == NULL) {
		*_num = 0;
		status = ENOENT;
		goto err;
	}

	dirent->d_dev = fs->id;
	dirent->d_ino = cookie->u.dir.ptr->id;
	dirent->d_reclen = strlen(cookie->u.dir.ptr->name);

	if (sizeof(struct dirent) + dirent->d_reclen + 1 > bufferSize) {
		status = ENOBUFS;
		goto err;
	}

	status = user_strcpy(dirent->d_name, cookie->u.dir.ptr->name);
	if (status < 0)
		goto err;

	cookie->u.dir.ptr = cookie->u.dir.ptr->dir_next;

err:
	mutex_unlock(&fs->lock);

	return status;
}


static status_t
devfs_rewind_dir(fs_cookie _fs, fs_vnode _vnode, file_cookie _cookie)
{
	struct devfs *fs = _fs;
	struct devfs_cookie *cookie = _cookie;

	TRACE(("devfs_rewind_dir: vnode %p, cookie %p\n", _vnode, _cookie));

	if (cookie->stream->type != STREAM_TYPE_DIR)
		return EINVAL;
	
	mutex_lock(&fs->lock);

	cookie->u.dir.ptr = cookie->stream->u.dir.dir_head;

	mutex_unlock(&fs->lock);
	return B_OK;
}


static status_t
devfs_ioctl(fs_cookie _fs, fs_vnode _v, file_cookie _cookie, ulong op, void *buf, size_t len)
{
	struct devfs *fs = _fs;
	struct devfs_vnode *v = _v;
	struct devfs_cookie *cookie = _cookie;

	TRACE(("devfs_ioctl: vnode %p, cookie %p, op %ld, buf %p, len %ld\n", _v, _cookie, op, buf, len));
	
	if (v->stream.type == STREAM_TYPE_DEVICE) {
		switch (op) {
			case IOCTL_DEVFS_GET_PARTITION_INFO:
				return devfs_get_partition_info(fs, v, cookie, buf, len);

			case IOCTL_DEVFS_SET_PARTITION:
				return devfs_set_partition(fs, v, cookie, buf, len);
		}

		return v->stream.u.dev.calls->control(cookie->u.dev.dcookie, op, buf, len);
	}
	return EINVAL;
}


/*
static int devfs_canpage(fs_cookie _fs, fs_vnode _v)
{
	struct devfs_vnode *v = _v;

	TRACE(("devfs_canpage: vnode 0x%x\n", v));

	if(v->stream.type == STREAM_TYPE_DEVICE) {
		if(!v->stream.u.dev.calls->dev_canpage)
			return 0;
		return v->stream.u.dev.calls->dev_canpage(v->stream.u.dev.ident);
	} else {
		return 0;
	}
}

static ssize_t devfs_readpage(fs_cookie _fs, fs_vnode _v, iovecs *vecs, off_t pos)
{
	struct devfs_vnode *v = _v;

	TRACE(("devfs_readpage: vnode 0x%x, vecs 0x%x, pos 0x%x 0x%x\n", v, vecs, pos));

	if(v->stream.type == STREAM_TYPE_DEVICE) {
		struct devfs_part_map *part_map = v->stream.u.dev.part_map;
			
		if(!v->stream.u.dev.calls->dev_readpage)
			return ERR_NOT_ALLOWED;
			
		if( part_map ) {
			if( pos < 0 )
				return ERR_INVALID_ARGS;
				
			if( pos > part_map->size )
				return 0;

			// XXX we modify a passed-in structure
			vecs->total_len = min( vecs->total_len, part_map->size - pos );
			pos += part_map->offset;
		}

		return v->stream.u.dev.calls->dev_readpage(v->stream.u.dev.ident, vecs, pos);
	} else {
		return ERR_NOT_ALLOWED;
	}
}

static ssize_t devfs_writepage(fs_cookie _fs, fs_vnode _v, iovecs *vecs, off_t pos)
{
	struct devfs_vnode *v = _v;

	TRACE(("devfs_writepage: vnode 0x%x, vecs 0x%x, pos 0x%x 0x%x\n", v, vecs, pos));

	if(v->stream.type == STREAM_TYPE_DEVICE) {
		struct devfs_part_map *part_map = v->stream.u.dev.part_map;

		if(!v->stream.u.dev.calls->dev_writepage)
			return ERR_NOT_ALLOWED;

		if( part_map ) {
			if( pos < 0 )
				return ERR_INVALID_ARGS;
				
			if( pos > part_map->size )
				return 0;

			// XXX we modify a passed-in structure
			vecs->total_len = min( vecs->total_len, part_map->size - pos );
			pos += part_map->offset;
		}

		return v->stream.u.dev.calls->dev_writepage(v->stream.u.dev.ident, vecs, pos);
	} else {
		return ERR_NOT_ALLOWED;
	}
}
*/

static status_t
devfs_unlink(fs_cookie _fs, fs_vnode _dir, const char *name)
{
	struct devfs *fs = _fs;
	struct devfs_vnode *dir = _dir;
	struct devfs_vnode *vnode;
	status_t status = B_NO_ERROR;

	mutex_lock(&fs->lock);

	vnode = devfs_find_in_dir(dir, name);
	if (!vnode) {
		status = B_ENTRY_NOT_FOUND;
		goto err;
	}
	
	// you can unlink partitions only
	if (vnode->stream.type != STREAM_TYPE_DEVICE || !vnode->stream.u.dev.part_map) {
		status = EROFS;
		goto err;
	}

	status = devfs_remove_from_dir(vnode->parent, vnode);
	if (status < 0)
		goto err;

	status = vfs_remove_vnode(fs->id, vnode->id);

err:
	mutex_unlock(&fs->lock);

	return status;
}


static status_t
devfs_rename(fs_cookie _fs, fs_vnode _olddir, const char *oldname, fs_vnode _newdir, const char *newname)
{
	return EROFS;
}


static status_t
devfs_read_stat(fs_cookie _fs, fs_vnode _v, struct stat *stat)
{
	struct devfs_vnode *vnode = _v;

	TRACE(("devfs_rstat: vnode %p (%Ld), stat %p\n", vnode, vnode->id, stat));

	stat->st_ino = vnode->id;
	stat->st_size = 0;
	// ToDo: or should this be just DEFFILEMODE (0666 instead of 0644)?
	stat->st_mode = vnode->stream.type | S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;

	return 0;
}


static status_t
devfs_write_stat(fs_cookie _fs, fs_vnode _v, const struct stat *stat, int stat_mask)
{
#if DEVFS_TRACE
	struct devfs_vnode *v = _v;

	TRACE(("devfs_wstat: vnode %p (%Ld), stat %p\n", v, v->id, stat));
#endif
	// cannot change anything
	return EPERM;
}


//	#pragma mark -


static struct fs_calls devfs_calls = {
	&devfs_mount,
	&devfs_unmount,
	NULL,
	NULL,
	&devfs_sync,

	&devfs_lookup,
	&devfs_get_vnode_name,

	&devfs_get_vnode,
	&devfs_put_vnode,
	&devfs_remove_vnode,

	NULL,	// can page (currently commented out for whatever reason...)
	NULL,	// read page
	NULL,	// write page

	/* common */
	&devfs_ioctl,
	&devfs_fsync,

	NULL,	// read_link
	NULL,	// write_link
	NULL,	// symlink
	NULL,	// link
	&devfs_unlink,
	&devfs_rename,

	NULL,	// access
	&devfs_read_stat,
	&devfs_write_stat,

	/* file */
	&devfs_create,
	&devfs_open,
	&devfs_close,
	&devfs_free_cookie,
	&devfs_read,
	&devfs_write,
	&devfs_seek,

	/* directory */
	&devfs_create_dir,
	NULL,	// remove_dir
	&devfs_open_dir,
	&devfs_close,			// we are using the same operations for directories
	&devfs_free_cookie,		// and files here - that's intended, not by accident
	&devfs_read_dir,
	&devfs_rewind_dir,
};


status_t
bootstrap_devfs(void)
{

	TRACE(("bootstrap_devfs: entry\n"));

	return vfs_register_filesystem("devfs", &devfs_calls);
}


status_t
devfs_publish_device(const char *path, void *ident, device_hooks *calls)
{
	int err = 0;
	int i, last;
	char temp[SYS_MAX_PATH_LEN+1];
	struct devfs_vnode *dir;
	struct devfs_vnode *v;
	bool at_leaf;

	TRACE(("devfs_publish_device: entry path '%s', ident %p, hooks %p\n", path, ident, calls));

	if (calls == NULL || path == NULL) {
		panic("devfs_publish_device called with NULL pointer!\n");
		return EINVAL;
	}

	if (!gDeviceFileSystem) {
		panic("devfs_publish_device called before devfs mounted\n");
		return ERR_GENERAL;
	}

	// copy the path over to a temp buffer so we can munge it
	strncpy(temp, path, SYS_MAX_PATH_LEN);
	temp[SYS_MAX_PATH_LEN] = 0;

	mutex_lock(&gDeviceFileSystem->lock);

	// create the path leading to the device
	// parse the path passed in, stripping out '/'
	dir = gDeviceFileSystem->root_vnode;
	v = NULL;
	i = 0;
	last = 0;
	at_leaf = false;
	for (;;) {
		if(temp[i] == 0) {
			at_leaf = true; // we'll be done after this one
		} else if(temp[i] == '/') {
			temp[i] = 0;
			i++;
		} else {
			i++;
			continue;
		}

		TRACE(("\tpath component '%s'\n", &temp[last]));

		// we have a path component
		v = devfs_find_in_dir(dir, &temp[last]);
		if (v) {
			if (!at_leaf) {
				// we are not at the leaf of the path, so as long as
				// this is a dir we're okay
				if (v->stream.type == STREAM_TYPE_DIR) {
					last = i;
					dir = v;
					continue;
				}
			}
			// we are at the leaf and hit another node
			// or we aren't but hit a non-dir node.
			// we're screwed
			err = ERR_VFS_ALREADY_EXISTS;
			goto err;
		} else {
			v = devfs_create_vnode(gDeviceFileSystem, &temp[last]);
			if (!v) {
				err = ENOMEM;
				goto err;
			}
		}

		// set up the new vnode
		if (at_leaf) {
			// this is the last component
			v->stream.type = STREAM_TYPE_DEVICE;
			v->stream.u.dev.ident = ident;
			v->stream.u.dev.calls = calls;
		} else {
			// this is a dir
			v->stream.type = STREAM_TYPE_DIR;
			v->stream.u.dir.dir_head = NULL;
			v->stream.u.dir.jar_head = NULL;
		}

		hash_insert(gDeviceFileSystem->vnode_list_hash, v);

		devfs_insert_in_dir(dir, v);

		if (at_leaf)
			break;
		last = i;
		dir = v;
	}

err:
	mutex_unlock(&gDeviceFileSystem->lock);
	return err;
}

