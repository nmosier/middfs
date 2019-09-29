#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/systm.h>
#include <sys/mount.h>
#include <sys/sbuf.h>

#define FSNAME "middfs"


int middfs_mount(struct mount *mp);
int middfs_unmount(struct mount *mp, int mntflags);
int middfs_eventhandler(struct module *mp, int event, void *data);
int middfs_vfsop_root(struct mount *mp, int lkflags,
		      struct vnode **vpp);
int middfs_vfsop_statfs(struct mount *mp, struct statfs *sbp);

static struct vfsops middfs_vfsops =
  {.vfs_mount = middfs_mount,
   .vfs_unmount = middfs_unmount,
   .vfs_statfs = middfs_vfsop_statfs,
   .vfs_root = middfs_vfsop_root,
  };

static struct vfsconf middfs_vfsconf =
  {.vfc_version = VFS_VERSION,
   .vfc_name = FSNAME,
   .vfc_vfsops = &middfs_vfsops,
   .vfc_typenum = -1,
   .vfc_flags = VFCF_JAIL | VFCF_SYNTHETIC
  };

	      

static struct moduledata middfs_moddata =
  {.name = FSNAME,
   .evhand = middfs_eventhandler,
   .priv = &middfs_vfsconf
  };


int middfs_mount(struct mount *mp) {
  printf("middfs: mounted\n");
  return 0;
}

int middfs_unmount(struct mount *mp, int mntflags) {
  printf("middfs: unmounted\n");
  return 0;
}

int middfs_eventhandler(struct module *mp, int event, void *data) {
  int err = 0;

  switch (event) {
  case MOD_LOAD:
    if ((err = vfs_modevent(NULL, event, &middfs_vfsconf))) {
      uprintf("middfs loading error\n");
      return err;
    }
    uprintf("middfs loaded\n");
    break;
  case MOD_UNLOAD:
    if ((err = vfs_modevent(NULL, event, &middfs_vfsconf))) {
      uprintf("middfs unloading error\n");
      return err;
    }
    uprintf("middfs unloaded\n");
    break;
  default:
    uprintf("middfs: unsupported event\n");
    err = EOPNOTSUPP;
    break;
  }

  return err;
}

int middfs_vfsop_root(struct mount *mp, int lkflags,
		      struct vnode **vpp) {
  uprintf("middfs root\n");
  return 0;
}

int middfs_vfsop_statfs(struct mount *mp, struct statfs *sbp) {
  uprintf("middfs statfs\n");
  return 0;
}


DECLARE_MODULE(middfs, middfs_moddata, SI_SUB_VFS, SI_ORDER_MIDDLE);
MODULE_VERSION(middfs, 1);
