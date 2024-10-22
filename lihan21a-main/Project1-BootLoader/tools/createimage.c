#include <assert.h>
#include <elf.h>
#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "deflate/tinylibdeflate.h"

#define IMAGE_FILE "./image"
#define ARGS "[--extended] [--vm] <bootblock> <executable-file> ..."

#define SECTOR_SIZE 512
#define BOOT_LOADER_SIG_OFFSET 0x1fe
#define OS_SIZE_LOC (BOOT_LOADER_SIG_OFFSET - 2)
#define TASK_INFO_OFFSET_LOC (BOOT_LOADER_SIG_OFFSET - 4)
#define DECOMPRESS_SIZE_LOC (BOOT_LOADER_SIG_OFFSET - 8)
#define BOOT_LOADER_SIG_1 0x55
#define BOOT_LOADER_SIG_2 0xaa

#define NBYTES2SEC(nbytes) (((nbytes) / SECTOR_SIZE) + ((nbytes) % SECTOR_SIZE != 0))

/* TODO: [p1-task4] design your own task_info_t */
typedef struct {
    char task_name[8];
    int task_size;
    int task_offset;
    uint64_t task_entry;
} task_info_t;

#define TASK_MAXNUM 16
static task_info_t taskinfo[TASK_MAXNUM];

/* structure to store command line options */
static struct {
    int vm;
    int extended;
} options;

/* prototypes of local functions */
static void create_image(int nfiles, char *files[]);
static void error(char *fmt, ...);
static void read_ehdr(Elf64_Ehdr *ehdr, FILE *fp);
static void read_phdr(Elf64_Phdr *phdr, FILE *fp, int ph, Elf64_Ehdr ehdr);
static uint64_t get_entrypoint(Elf64_Ehdr ehdr);
static uint32_t get_filesz(Elf64_Phdr phdr);
static uint32_t get_memsz(Elf64_Phdr phdr);
static void write_segment(Elf64_Phdr phdr, FILE *fp, FILE *img, int *phyaddr);
int write_kernel(Elf64_Phdr phdr, FILE *fp, FILE *img, int *phyaddr);
static void write_padding(FILE *img, int *phyaddr, int new_phyaddr);
static void write_img_info(int nbytes_kernel, task_info_t *taskinfo,
                           short tasknum, FILE *img, int nbytes_decompressm, int kernel_size);

int main(int argc, char **argv)
{
    char *progname = argv[0];

    /* process command line options */
    options.vm = 0;
    options.extended = 0;
    while ((argc > 1) && (argv[1][0] == '-') && (argv[1][1] == '-')) {
        char *option = &argv[1][2];

        if (strcmp(option, "vm") == 0) {
            options.vm = 1;
        } else if (strcmp(option, "extended") == 0) {
            options.extended = 1;
        } else {
            error("%s: invalid option\nusage: %s %s\n", progname,
                  progname, ARGS);
        }
        argc--;
        argv++;
    }
    if (options.vm == 1) {
        error("%s: option --vm not implemented\n", progname);
    }
    if (argc < 3) {
        /* at least 3 args (createimage bootblock main) */
        error("usage: %s %s\n", progname, ARGS);
    }
    create_image(argc - 1, argv + 1);
    return 0;
}

/* TODO: [p1-task4] assign your task_info_t somewhere in 'create_image' */
static void create_image(int nfiles, char *files[])
{
    int tasknum = nfiles - 3;
    int nbytes_kernel = 0;
    int nbytes_decompress = 0;
    int phyaddr = 0;
    FILE *fp = NULL, *img = NULL;
    Elf64_Ehdr ehdr;
    Elf64_Phdr phdr;
    int kernel_size = 0;

    /* open the image file */
    img = fopen(IMAGE_FILE, "w");
    assert(img != NULL);

    /* for each input file */
    int filenum = 1;
    for (int fidx = 0; fidx < nfiles; ++fidx) {

        int taskidx = fidx - 3;

        /* open input file */
        fp = fopen(*files, "r");
        assert(fp != NULL);

        /* read ELF header */
        read_ehdr(&ehdr, fp);
        printf("0x%04lx: %s\n", ehdr.e_entry, *files);

        /*task4 taskinfo中部分内容*/
        if (taskidx >= 0)
        {
            taskinfo[taskidx].task_offset = phyaddr;
            taskinfo[taskidx].task_name[0] = '\0';
            strcpy(taskinfo[taskidx].task_name, *files);
            taskinfo[taskidx].task_entry = get_entrypoint(ehdr);
        }

        /* for each program header */
        for (int ph = 0; ph < ehdr.e_phnum; ph++) {

            /* read program header */
            read_phdr(&phdr, fp, ph, ehdr);
            if (phdr.p_type != PT_LOAD) continue;

            /* write segment to the image */
            if(taskidx != -1)
                write_segment(phdr, fp, img, &phyaddr);
            else
                kernel_size = write_kernel(phdr, fp, img, &phyaddr);

            /* update nbytes_kernel */
            if (strcmp(*files, "main") == 0) {
                nbytes_kernel += get_filesz(phdr);
            }
            else if(strcmp(*files, "decompress") == 0)
            {
                nbytes_decompress += get_filesz(phdr);
            }
        }

        /*task 4 size*/
        if(taskidx >= 0)
        {
            taskinfo[taskidx].task_size = phyaddr - taskinfo[taskidx].task_offset;
        }

        /* write padding bytes */
        /**
         * TODO:
         * 1. [p1-task3] do padding so that the kernel and every app program
         *  occupies the same number of sectors
         * 2. [p1-task4] only padding bootblock is allowed!
         */

        if (strcmp(*files, "bootblock") == 0) {
            write_padding(img, &phyaddr, SECTOR_SIZE);
        }
        /*p1 task3*/
    /*    else
        {
            write_padding(img, &phyaddr, filenum*15*SECTOR_SIZE+SECTOR_SIZE);
            filenum++;
        }  */
        fclose(fp);
        files++;
    }
    write_img_info(nbytes_kernel, taskinfo, tasknum, img, nbytes_decompress, kernel_size);

    fclose(img);
}

static void read_ehdr(Elf64_Ehdr * ehdr, FILE * fp)
{
    int ret;

    ret = fread(ehdr, sizeof(*ehdr), 1, fp);
    assert(ret == 1);
    assert(ehdr->e_ident[EI_MAG1] == 'E');
    assert(ehdr->e_ident[EI_MAG2] == 'L');
    assert(ehdr->e_ident[EI_MAG3] == 'F');
}

static void read_phdr(Elf64_Phdr * phdr, FILE * fp, int ph,
                      Elf64_Ehdr ehdr)
{
    int ret;

    fseek(fp, ehdr.e_phoff + ph * ehdr.e_phentsize, SEEK_SET);
    ret = fread(phdr, sizeof(*phdr), 1, fp);
    assert(ret == 1);
    if (options.extended == 1) {
        printf("\tsegment %d\n", ph);
        printf("\t\toffset 0x%04lx", phdr->p_offset);
        printf("\t\tvaddr 0x%04lx\n", phdr->p_vaddr);
        printf("\t\tfilesz 0x%04lx", phdr->p_filesz);
        printf("\t\tmemsz 0x%04lx\n", phdr->p_memsz);
    }
}

static uint64_t get_entrypoint(Elf64_Ehdr ehdr)
{
    return ehdr.e_entry;
}

static uint32_t get_filesz(Elf64_Phdr phdr)
{
    return phdr.p_filesz;
}

static uint32_t get_memsz(Elf64_Phdr phdr)
{
    return phdr.p_memsz;
}

static void write_segment(Elf64_Phdr phdr, FILE *fp, FILE *img, int *phyaddr)
{
    if (phdr.p_memsz != 0 && phdr.p_type == PT_LOAD) {
        /* write the segment itself */
        /* NOTE: expansion of .bss should be done by kernel or runtime env! */
        if (options.extended == 1) {
            printf("\t\twriting 0x%04lx bytes\n", phdr.p_filesz);
        }
        fseek(fp, phdr.p_offset, SEEK_SET);
        printf("%d\n",*phyaddr);
        while (phdr.p_filesz-- > 0) {
            fputc(fgetc(fp), img);
            (*phyaddr)++;
        }
    }
}

int write_kernel(Elf64_Phdr phdr, FILE *fp, FILE *img, int *phyaddr)
{
    char compressbuf[30*SECTOR_SIZE];
    char origin_file[30*SECTOR_SIZE];
    

    fseek(fp, phdr.p_offset, SEEK_SET);
    int data_len = get_filesz(phdr);
    int i = 0;
    while (phdr.p_filesz-- > 0) {
        origin_file[i++] = (char)(fgetc(fp));
    }

    printf("%d\n",*phyaddr);

    deflate_set_memory_allocator((void * (*)(int))malloc, free);
    struct libdeflate_compressor * compressor = deflate_alloc_compressor(1);
    int out_nbytes = deflate_deflate_compress(compressor, origin_file, data_len, compressbuf, 30*SECTOR_SIZE);
    int compress_size = out_nbytes;
    printf("origin size is %d\n", data_len);
    printf("SUCCESS,compress_size = %d\n",compress_size);
/*  int restore_nbytes = 0;
    if(deflate_deflate_decompress(decompressor, compressbuf, out_nbytes, extracted, 15*SECTOR_SIZE, &restore_nbytes)){
        printf("An error occurred during decompression.\n");
        exit(1);
    }*/
    i = 0;
    while(out_nbytes-- > 0)
    {
        fputc((int)compressbuf[i++],img);
        (*phyaddr)++;
    }
    
    return compress_size;
}


static void write_padding(FILE *img, int *phyaddr, int new_phyaddr)
{
    if (options.extended == 1 && *phyaddr < new_phyaddr) {
        printf("\t\twrite 0x%04x bytes for padding\n", new_phyaddr - *phyaddr);
    }

    while (*phyaddr < new_phyaddr) {
        fputc(0, img);
        (*phyaddr)++;
    }
}

static void write_img_info(int nbytes_kernel, task_info_t *taskinfo,
                           short tasknum, FILE * img, int nbytes_decompress, int kernel_size)
{
    // TODO: [p1-task3] & [p1-task4] write image info to some certain places
    // NOTE: os size, infomation about app-info sector(s) ...
    /*写入os_size*/
    int kernel_offset = SECTOR_SIZE + nbytes_decompress;
    short os_size = NBYTES2SEC(kernel_size + kernel_offset%SECTOR_SIZE);
    fseek(img, OS_SIZE_LOC, SEEK_SET);
    fwrite(&os_size, sizeof(os_size), 1, img);
    fwrite(&tasknum, sizeof(tasknum), 1, img);   //在os_size之后写入tasknum

    fseek(img, BOOT_LOADER_SIG_OFFSET,SEEK_SET);
    char boot_loader_sig_1 = BOOT_LOADER_SIG_1;
    fwrite(&boot_loader_sig_1, sizeof(char), 1, img);

    fseek(img, BOOT_LOADER_SIG_OFFSET + sizeof(char), SEEK_SET);
    char boot_loader_sig_2 = BOOT_LOADER_SIG_2;
    fwrite(&boot_loader_sig_2, sizeof(char), 1, img);

    /*写入taskinfo_offset*/
    short task_info_offset = taskinfo[tasknum-1].task_offset + taskinfo[tasknum-1].task_size;
    fseek(img, TASK_INFO_OFFSET_LOC, SEEK_SET);
    fwrite(&task_info_offset, sizeof(task_info_offset), 1, img);

    /*解压文件的扇区数*/
    int decompress_size = NBYTES2SEC(nbytes_decompress);
    printf("解压文件大小：%d\n", decompress_size);
    fseek(img, DECOMPRESS_SIZE_LOC, SEEK_SET);
    fwrite(&decompress_size, sizeof(decompress_size), 1, img);

    /*kernel的offset*/
    fseek(img, DECOMPRESS_SIZE_LOC-4, SEEK_SET);
    fwrite(&kernel_offset, sizeof(kernel_offset), 1, img);

    /*kernel的大小*/
    fseek(img, DECOMPRESS_SIZE_LOC-8, SEEK_SET);
    fwrite(&kernel_size, sizeof(nbytes_kernel), 1, img);



    /*写入各项任务*/
    for(int i = 0; i < tasknum; i++)
    {
        fseek(img, task_info_offset*SECTOR_SIZE+i*sizeof(task_info_t), SEEK_SET);
        fwrite(&taskinfo[i], sizeof(task_info_t), 1, img);
    }


    
}

/* print an error message and exit */
static void error(char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    if (errno != 0) {
        perror(NULL);
    }
    exit(EXIT_FAILURE);
}
