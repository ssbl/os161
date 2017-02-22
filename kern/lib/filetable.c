#include <types.h>
#include <lib.h>
#include <kern/fcntl.h>
#include <synch.h>
#include <vm.h>
#include <vfs.h>
#include <vnode.h>

#define FILETABLE_INLINE
#include "filetable.h"


struct file_entry *
file_entry_create(const char *name, int openflags, mode_t mode)
{
	KASSERT(name != NULL);

	int result;

	struct file_entry *fentry = kmalloc(sizeof(*fentry));
	if (fentry == NULL) {
		return NULL;
	}

	fentry->f_name = kstrdup(name);
	if (fentry->f_name == NULL) {
		kfree(fentry);
		return NULL;
	}

	result = vfs_open(fentry->f_name, openflags, mode, &fentry->f_node);
	if (result) {
		kfree(fentry->f_name);
		kfree(fentry);
		return NULL;
	}

	lock_create("file_entry lock");
	if (fentry->f_lk == NULL) {
		kfree(fentry->f_name);
		vfs_close(fentry->f_node);
		kfree(fentry);
		return NULL;
	}

	fentry->f_offset = 0;
	fentry->f_mode = mode;

	return fentry;
}

void
file_entry_destroy(struct file_entry *fentry)
{
	KASSERT(fentry != NULL);

	kfree(fentry->f_name);
	vfs_close(fentry->f_node);
	lock_destroy(fentry->f_lk);
	kfree(fentry);
}

/*
 * should be called only when the first user process is initialized.
 */
struct file_entry *
stdin(void)
{
	int result;
	struct file_entry *stdin;
	struct lock *lock;

	stdin = kmalloc(sizeof(stdin));
	if (stdin == NULL) {
		return NULL;
	}

    result = vfs_open("con:", O_RDONLY, 0644, &stdin->f_node);
    if (result) {
        kfree(stdin);
        return NULL;
    }

	lock = lock_create("stdin lock");
	if (lock == NULL) {
		kfree(stdin);
		return NULL;
	}

	stdin->f_offset = 0;
	stdin->f_mode = O_WRONLY;

	return stdin;
}

/*
 * should be called only when the first user process is initialized.
 */
struct file_entry *
stdout(void)
{
	int result;
	struct file_entry *stdout;
	struct lock *lock;

	stdout = kmalloc(sizeof(stdout));
	if (stdout == NULL) {
		return NULL;
	}

    result = vfs_open("con:", O_WRONLY, 0644, &stdout->f_node);
    if (result) {
        kfree(stdout);
        return NULL;
    }

	lock = lock_create("stdout lock");
	if (lock == NULL) {
		kfree(stdout);
		return NULL;
	}

	stdout->f_offset = 0;
	stdout->f_mode = O_RDONLY;

	return stdout;
}
