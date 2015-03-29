/*
 * Copyright (C) 2005-2015 Junjiro R. Okajima
 *
 * This program, aufs is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * inode operations
 */

#ifndef __AUFS_INODE_H__
#define __AUFS_INODE_H__

#ifdef __KERNEL__

#include <linux/fs.h>
#include "rwsem.h"

struct vfsmount;

struct au_hinode {
	struct inode		*hi_inode;
	aufs_bindex_t		hi_id;

	/* reference to the copied-up whiteout with get/put */
	struct dentry		*hi_whdentry;
};

/* ig_flags */
#define AuIG_HALF_REFRESHED		1
#define au_ig_ftest(flags, name)	((flags) & AuIG_##name)
#define au_ig_fset(flags, name) \
	do { (flags) |= AuIG_##name; } while (0)
#define au_ig_fclr(flags, name) \
	do { (flags) &= ~AuIG_##name; } while (0)

struct au_iigen {
	__u32		ig_generation, ig_flags;
};

struct au_vdir;
struct au_iinfo {
	spinlock_t		ii_genspin;
	struct au_iigen		ii_generation;
	struct super_block	*ii_hsb1;	/* no get/put */

	struct au_rwsem		ii_rwsem;
	aufs_bindex_t		ii_bstart, ii_bend;
	__u32			ii_higen;
	struct au_hinode	*ii_hinode;
	struct au_vdir		*ii_vdir;
};

struct au_icntnr {
	struct au_iinfo iinfo;
	struct inode vfs_inode;
} ____cacheline_aligned_in_smp;

/* au_pin flags */
#define AuPin_DI_LOCKED		1
#define AuPin_MNT_WRITE		(1 << 1)
#define au_ftest_pin(flags, name)	((flags) & AuPin_##name)
#define au_fset_pin(flags, name) \
	do { (flags) |= AuPin_##name; } while (0)
#define au_fclr_pin(flags, name) \
	do { (flags) &= ~AuPin_##name; } while (0)

struct au_pin {
	/* input */
	struct dentry *dentry;
	unsigned int udba;
	unsigned char lsc_di, lsc_hi, flags;
	aufs_bindex_t bindex;

	/* output */
	struct dentry *parent;
	struct au_hinode *hdir;
	struct vfsmount *h_mnt;

	/* temporary unlock/relock for copyup */
	struct dentry *h_dentry, *h_parent;
	struct au_branch *br;
	struct task_struct *task;
};

void au_pin_hdir_unlock(struct au_pin *p);
int au_pin_hdir_lock(struct au_pin *p);
int au_pin_hdir_relock(struct au_pin *p);
void au_pin_hdir_set_owner(struct au_pin *p, struct task_struct *task);
void au_pin_hdir_acquire_nest(struct au_pin *p);
void au_pin_hdir_release(struct au_pin *p);

/* ---------------------------------------------------------------------- */

static inline struct au_iinfo *au_ii(struct inode *inode)
{
	struct au_iinfo *iinfo;

	iinfo = &(container_of(inode, struct au_icntnr, vfs_inode)->iinfo);
	if (iinfo->ii_hinode)
		return iinfo;
	return NULL; /* debugging bad_inode case */
}

/* ---------------------------------------------------------------------- */

/* inode.c */
struct inode *au_igrab(struct inode *inode);
int au_refresh_hinode_self(struct inode *inode);
int au_refresh_hinode(struct inode *inode, struct dentry *dentry);
int au_ino(struct super_block *sb, aufs_bindex_t bindex, ino_t h_ino,
	   unsigned int d_type, ino_t *ino);
struct inode *au_new_inode(struct dentry *dentry, int must_new);
int au_test_ro(struct super_block *sb, aufs_bindex_t bindex,
	       struct inode *inode);
int au_test_h_perm(struct inode *h_inode, int mask);
int au_test_h_perm_sio(struct inode *h_inode, int mask);

/* i_op.c */
extern struct inode_operations aufs_iop, aufs_symlink_iop, aufs_dir_iop;

/* au_wr_dir flags */
#define AuWrDir_ADD_ENTRY	1
#define AuWrDir_ISDIR		(1 << 1)
#define AuWrDir_TMPFILE		(1 << 2)
#define au_ftest_wrdir(flags, name)	((flags) & AuWrDir_##name)
#define au_fset_wrdir(flags, name) \
	do { (flags) |= AuWrDir_##name; } while (0)
#define au_fclr_wrdir(flags, name) \
	do { (flags) &= ~AuWrDir_##name; } while (0)

struct au_wr_dir_args {
	aufs_bindex_t force_btgt;
	unsigned char flags;
};
int au_wr_dir(struct dentry *dentry, struct dentry *src_dentry,
	      struct au_wr_dir_args *args);

struct dentry *au_pinned_h_parent(struct au_pin *pin);
void au_pin_init(struct au_pin *pin, struct dentry *dentry,
		 aufs_bindex_t bindex, int lsc_di, int lsc_hi,
		 unsigned int udba, unsigned char flags);
int au_pin(struct au_pin *pin, struct dentry *dentry, aufs_bindex_t bindex,
	   unsigned int udba, unsigned char flags) __must_check;
int au_do_pin(struct au_pin *pin) __must_check;
void au_unpin(struct au_pin *pin);

/* i_op_add.c */
int au_may_add(struct dentry *dentry, aufs_bindex_t bindex,
	       struct dentry *h_parent, int isdir);
int aufs_mknod(struct inode *dir, struct dentry *dentry, umode_t mode,
	       dev_t dev);
int aufs_symlink(struct inode *dir, struct dentry *dentry, const char *symname);
int aufs_create(struct inode *dir, struct dentry *dentry, umode_t mode,
		bool want_excl);
int aufs_tmpfile(struct inode *dir, struct dentry *dentry, umode_t mode);
int aufs_mkdir(struct inode *dir, struct dentry *dentry, umode_t mode);

/* iinfo.c */
struct inode *au_h_iptr(struct inode *inode, aufs_bindex_t bindex);
void au_hiput(struct au_hinode *hinode);
void au_set_hi_wh(struct inode *inode, aufs_bindex_t bindex,
		  struct dentry *h_wh);
unsigned int au_hi_flags(struct inode *inode, int isdir);

/* hinode flags */
#define AuHi_XINO	1
#define au_ftest_hi(flags, name)	((flags) & AuHi_##name)
#define au_fset_hi(flags, name) \
	do { (flags) |= AuHi_##name; } while (0)
#define au_fclr_hi(flags, name) \
	do { (flags) &= ~AuHi_##name; } while (0)

void au_set_h_iptr(struct inode *inode, aufs_bindex_t bindex,
		   struct inode *h_inode, unsigned int flags);

void au_update_iigen(struct inode *inode, int half);
void au_update_ibrange(struct inode *inode, int do_put_zero);

void au_icntnr_init_once(void *_c);
int au_iinfo_init(struct inode *inode);
void au_iinfo_fin(struct inode *inode);
int au_ii_realloc(struct au_iinfo *iinfo, int nbr);

#ifdef CONFIG_PROC_FS
/* plink.c */
int au_plink_maint(struct super_block *sb, int flags);
struct au_sbinfo;
void au_plink_maint_leave(struct au_sbinfo *sbinfo);
int au_plink_maint_enter(struct super_block *sb);
#ifdef CONFIG_AUFS_DEBUG
void au_plink_list(struct super_block *sb);
#else
AuStubVoid(au_plink_list, struct super_block *sb)
#endif
int au_plink_test(struct inode *inode);
struct dentry *au_plink_lkup(struct inode *inode, aufs_bindex_t bindex);
void au_plink_append(struct inode *inode, aufs_bindex_t bindex,
		     struct dentry *h_dentry);
void au_plink_put(struct super_block *sb, int verbose);
void au_plink_clean(struct super_block *sb, int verbose);
#else
AuStubInt0(au_plink_maint, struct super_block *sb, int flags);
AuStubVoid(au_plink_maint_leave, struct au_sbinfo *sbinfo);
AuStubInt0(au_plink_maint_enter, struct super_block *sb);
AuStubVoid(au_plink_list, struct super_block *sb);
AuStubInt0(au_plink_test, struct inode *inode);
AuStub(struct dentry *, au_plink_lkup, return NULL,
       struct inode *inode, aufs_bindex_t bindex);
AuStubVoid(au_plink_append, struct inode *inode, aufs_bindex_t bindex,
	   struct dentry *h_dentry);
AuStubVoid(au_plink_put, struct super_block *sb, int verbose);
AuStubVoid(au_plink_clean, struct super_block *sb, int verbose);
#endif /* CONFIG_PROC_FS */

/* ---------------------------------------------------------------------- */

/* lock subclass for iinfo */
enum {
	AuLsc_II_CHILD,		/* child first */
	AuLsc_II_CHILD2,	/* rename(2), link(2), and cpup at hnotify */
	AuLsc_II_CHILD3,	/* copyup dirs */
	AuLsc_II_PARENT,	/* see AuLsc_I_PARENT in vfsub.h */
	AuLsc_II_PARENT2,
	AuLsc_II_PARENT3,	/* copyup dirs */
	AuLsc_II_NEW_CHILD
};

/*
 * ii_read_lock_child, ii_write_lock_child,
 * ii_read_lock_child2, ii_write_lock_child2,
 * ii_read_lock_child3, ii_write_lock_child3,
 * ii_read_lock_parent, ii_write_lock_parent,
 * ii_read_lock_parent2, ii_write_lock_parent2,
 * ii_read_lock_parent3, ii_write_lock_parent3,
 * ii_read_lock_new_child, ii_write_lock_new_child,
 */
#define AuReadLockFunc(name, lsc) \
static inline void ii_read_lock_##name(struct inode *i) \
{ \
	au_rw_read_lock_nested(&au_ii(i)->ii_rwsem, AuLsc_II_##lsc); \
}

#define AuWriteLockFunc(name, lsc) \
static inline void ii_write_lock_##name(struct inode *i) \
{ \
	au_rw_write_lock_nested(&au_ii(i)->ii_rwsem, AuLsc_II_##lsc); \
}

#define AuRWLockFuncs(name, lsc) \
	AuReadLockFunc(name, lsc) \
	AuWriteLockFunc(name, lsc)

AuRWLockFuncs(child, CHILD);
AuRWLockFuncs(child2, CHILD2);
AuRWLockFuncs(child3, CHILD3);
AuRWLockFuncs(parent, PARENT);
AuRWLockFuncs(parent2, PARENT2);
AuRWLockFuncs(parent3, PARENT3);
AuRWLockFuncs(new_child, NEW_CHILD);

#undef AuReadLockFunc
#undef AuWriteLockFunc
#undef AuRWLockFuncs

/*
 * ii_read_unlock, ii_write_unlock, ii_downgrade_lock
 */
AuSimpleUnlockRwsemFuncs(ii, struct inode *i, &au_ii(i)->ii_rwsem);

#define IiMustNoWaiters(i)	AuRwMustNoWaiters(&au_ii(i)->ii_rwsem)
#define IiMustAnyLock(i)	AuRwMustAnyLock(&au_ii(i)->ii_rwsem)
#define IiMustWriteLock(i)	AuRwMustWriteLock(&au_ii(i)->ii_rwsem)

/* ---------------------------------------------------------------------- */

static inline void au_icntnr_init(struct au_icntnr *c)
{
#ifdef CONFIG_AUFS_DEBUG
	c->vfs_inode.i_mode = 0;
#endif
}

static inline unsigned int au_iigen(struct inode *inode, struct au_iigen *iigen)
{
	unsigned int gen;
	struct au_iinfo *iinfo;

	iinfo = au_ii(inode);
	spin_lock(&iinfo->ii_genspin);
	if (iigen)
		*iigen = iinfo->ii_generation;
	gen = iinfo->ii_generation.ig_generation;
	spin_unlock(&iinfo->ii_genspin);

	return gen;
}

/* tiny test for inode number */
/* tmpfs generation is too rough */
static inline int au_test_higen(struct inode *inode, struct inode *h_inode)
{
	struct au_iinfo *iinfo;

	iinfo = au_ii(inode);
	AuRwMustAnyLock(&iinfo->ii_rwsem);
	return !(iinfo->ii_hsb1 == h_inode->i_sb
		 && iinfo->ii_higen == h_inode->i_generation);
}

static inline void au_iigen_dec(struct inode *inode)
{
	struct au_iinfo *iinfo;

	iinfo = au_ii(inode);
	spin_lock(&iinfo->ii_genspin);
	iinfo->ii_generation.ig_generation--;
	spin_unlock(&iinfo->ii_genspin);
}

static inline int au_iigen_test(struct inode *inode, unsigned int sigen)
{
	int err;

	err = 0;
	if (unlikely(inode && au_iigen(inode, NULL) != sigen))
		err = -EIO;

	return err;
}

/* ---------------------------------------------------------------------- */

static inline aufs_bindex_t au_ii_br_id(struct inode *inode,
					aufs_bindex_t bindex)
{
	IiMustAnyLock(inode);
	return au_ii(inode)->ii_hinode[0 + bindex].hi_id;
}

static inline aufs_bindex_t au_ibstart(struct inode *inode)
{
	IiMustAnyLock(inode);
	return au_ii(inode)->ii_bstart;
}

static inline aufs_bindex_t au_ibend(struct inode *inode)
{
	IiMustAnyLock(inode);
	return au_ii(inode)->ii_bend;
}

static inline struct au_vdir *au_ivdir(struct inode *inode)
{
	IiMustAnyLock(inode);
	return au_ii(inode)->ii_vdir;
}

static inline struct dentry *au_hi_wh(struct inode *inode, aufs_bindex_t bindex)
{
	IiMustAnyLock(inode);
	return au_ii(inode)->ii_hinode[0 + bindex].hi_whdentry;
}

static inline void au_set_ibstart(struct inode *inode, aufs_bindex_t bindex)
{
	IiMustWriteLock(inode);
	au_ii(inode)->ii_bstart = bindex;
}

static inline void au_set_ibend(struct inode *inode, aufs_bindex_t bindex)
{
	IiMustWriteLock(inode);
	au_ii(inode)->ii_bend = bindex;
}

static inline void au_set_ivdir(struct inode *inode, struct au_vdir *vdir)
{
	IiMustWriteLock(inode);
	au_ii(inode)->ii_vdir = vdir;
}

static inline struct au_hinode *au_hi(struct inode *inode, aufs_bindex_t bindex)
{
	IiMustAnyLock(inode);
	return au_ii(inode)->ii_hinode + bindex;
}

/* ---------------------------------------------------------------------- */

static inline struct dentry *au_pinned_parent(struct au_pin *pin)
{
	if (pin)
		return pin->parent;
	return NULL;
}

static inline struct inode *au_pinned_h_dir(struct au_pin *pin)
{
	if (pin && pin->hdir)
		return pin->hdir->hi_inode;
	return NULL;
}

static inline struct au_hinode *au_pinned_hdir(struct au_pin *pin)
{
	if (pin)
		return pin->hdir;
	return NULL;
}

static inline void au_pin_set_dentry(struct au_pin *pin, struct dentry *dentry)
{
	if (pin)
		pin->dentry = dentry;
}

static inline void au_pin_set_parent_lflag(struct au_pin *pin,
					   unsigned char lflag)
{
	if (pin) {
		if (lflag)
			au_fset_pin(pin->flags, DI_LOCKED);
		else
			au_fclr_pin(pin->flags, DI_LOCKED);
	}
}

#if 0 /* reserved */
static inline void au_pin_set_parent(struct au_pin *pin, struct dentry *parent)
{
	if (pin) {
		dput(pin->parent);
		pin->parent = dget(parent);
	}
}
#endif

#endif /* __KERNEL__ */
#endif /* __AUFS_INODE_H__ */
