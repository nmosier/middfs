#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/systm.h>
#include <sys/mount.h>
#include <fs/pseudofs/pseudofs.h>
#include <sys/sbuf.h>

int middfs_init(struct pfs_info *pi, struct vfsconf *vfs);
int middfs_uninit(struct pfs_info *pi, struct vfsconf *vfs);
int middfs_file(struct thread *td, struct proc *p, struct pfs_node *pn,
		struct sbuf* sb, struct uio *uio);


int middfs_file(struct thread *td, struct proc *p, struct pfs_node *pn,
		struct sbuf* sb, struct uio *uio) {
  const char *contents = "this is a synthetic file\n";
  sbuf_bcpy(sb, contents, strlen(contents));
  return 0;
}

int middfs_init(struct pfs_info *pi, struct vfsconf *vfs) {
  struct pfs_node *root = pi->pi_root; /* mount point */
  
  uprintf("middfs initialized\n");
  pfs_create_file(root, "file", middfs_file, NULL, NULL, NULL, PFS_RD);
  return 0;
}
  
int middfs_uninit(struct pfs_info *pi, struct vfsconf *vfs) {
  uprintf("middfs uninitialized\n");  
  return 0;
}

PSEUDOFS(middfs, 1, VFCF_JAIL);
