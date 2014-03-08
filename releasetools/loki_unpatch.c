/*
 * loki_unpatch
 *
 * A utility to extract the original .img out of a .lok
 *
 * by Eric McCann (@nuclearmistake)
 * based on loki_patch by Dan Rosenberg (@djrbliss)
 */

#include <stdio.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#define VERSION "1.3"

#define BOOT_MAGIC_SIZE 8
#define BOOT_NAME_SIZE 16
#define BOOT_ARGS_SIZE 512

struct boot_img_hdr
{
	unsigned char magic[BOOT_MAGIC_SIZE];
	unsigned kernel_size;	/* size in bytes */
	unsigned kernel_addr;	/* physical load addr */
	unsigned ramdisk_size;	/* size in bytes */
	unsigned ramdisk_addr;	/* physical load addr */
	unsigned second_size;	/* size in bytes */
	unsigned second_addr;	/* physical load addr */
	unsigned tags_addr;		/* physical addr for kernel tags */
	unsigned page_size;		/* flash page size we assume */
	unsigned dt_size;		/* device_tree in bytes */
	unsigned unused;		/* future expansion: should be 0 */
	unsigned char name[BOOT_NAME_SIZE];	/* asciiz product name */
	unsigned char cmdline[BOOT_ARGS_SIZE];
	unsigned id[8];			/* timestamp / checksum / sha1 / etc */
};

struct loki_hdr
{
	unsigned char magic[4];		/* 0x494b4f4c */
	unsigned int recovery;		/* 0 = boot.img, 1 = recovery.img */
	unsigned char build[128];	/* Build number */
};

int main(int argc, char **argv)
{

	int ifd, ofd;
	unsigned int orig_ramdisk_size, orig_kernel_size, page_kernel_size, page_ramdisk_size, page_size, page_mask;
	void *orig;
	struct stat st;
	struct boot_img_hdr *hdr;
	struct loki_hdr *loki_hdr;

	if (argc != 4) {
		printf("Usage: %s [boot|recovery] [in.lok] [out.img]\n", argv[0]);
		return 1;
	}

	printf("[+] loki_unpatch v%s\n", VERSION);

	if (strcmp(argv[1], "boot") && strcmp(argv[1], "recovery")) {
		printf("[+] First argument must be \"boot\" or \"recovery\".\n");
    // ... but i wouldn't trust the output of this for a recovery.lok   :-D
		return 1;
	}

	ifd = open(argv[2], O_RDONLY);
	if (ifd < 0) {
		printf("[-] Failed to open %s for reading.\n", argv[3]);
		return 1;
	}

	ofd = open(argv[3], O_WRONLY|O_CREAT|O_TRUNC, 0644);
	if (ofd < 0) {
		printf("[-] Failed to open %s for writing.\n", argv[4]);
		return 1;
	}

	/* Map the original boot/recovery image */
	if (fstat(ifd, &st)) {
		printf("[-] fstat() failed.\n");
		return 1;
	}

	orig = mmap(0, (st.st_size + 0x2000 + 0xfff) & ~0xfff, PROT_READ|PROT_WRITE, MAP_PRIVATE, ifd, 0);
	if (orig == MAP_FAILED) {
		printf("[-] Failed to mmap input file.\n");
		return 1;
	}

	hdr = orig;
	loki_hdr = orig + 0x400;

	if (memcmp(loki_hdr->magic, "LOKI", 4)) {
		printf("[-] Input file is NOT a Loki image.\n");
		return 1;
	}

	page_size = hdr->page_size;
	page_mask = hdr->page_size - 1;

	/* suck all of the brains out of the unused fields loki_patch stuck them in */
	orig_kernel_size = hdr->dt_size;
	orig_ramdisk_size = hdr->unused;
	page_kernel_size = (orig_kernel_size + page_mask) & ~page_mask;
	page_ramdisk_size = (orig_ramdisk_size + page_mask) & ~page_mask;
	hdr->ramdisk_size = page_ramdisk_size;
	hdr->kernel_size = page_kernel_size;
	hdr->ramdisk_addr = orig + page_size + page_kernel_size;
	hdr->dt_size = 0;
	hdr->unused = 0;

#ifdef DEBUG
	printf("[+] Original kernel address: %.08x\n", hdr->kernel_addr);
	printf("[+] Original kernel size: %.08x\n", hdr->kernel_size);
	printf("[+] Original ramdisk address: %.08x\n", hdr->ramdisk_addr);
	printf("[+] Original ramdisk size: %.08x\n", hdr->ramdisk_size);
	printf("[+] Original second addr: %.08x\n", hdr->second_addr);
	printf("[+] Original second size: %.08x\n", hdr->second_size);
	printf("[+] Original tags addr: %.08x\n", hdr->tags_addr);
	printf("[+] Original page size: %.08x\n", hdr->page_size);
	printf("[+] Original dt size: %.08x\n", hdr->dt_size);
	printf("[+] Original unused: %.08x\n", hdr->unused);
	printf("[+] Original name: %s\n", hdr->name);
	printf("[+] Original cmdline: %s\n", hdr->cmdline);
#endif

	/* Write the image header */
	if (write(ofd, orig, page_size) != page_size) {
		printf("[-] Failed to write header to output file.\n");
		return 1;
	}

	/* Write the kernel */
	if (write(ofd, orig + page_size, page_kernel_size) != page_kernel_size) {
		printf("[-] Failed to write kernel to output file.\n");
		return 1;
	}

	/* Write the ramdisk */
	if (write(ofd, orig + page_size + page_kernel_size, page_ramdisk_size) != page_ramdisk_size) {
		printf("[-] Failed to write ramdisk to output file.\n");
		return 1;
	}

	printf("[+] Output file written to %s\n", argv[3]);

	close(ifd);
	close(ofd);
	return 0;
}
