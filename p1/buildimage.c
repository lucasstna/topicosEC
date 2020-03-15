/* Author(s): Gabriel Lucas da Silva, Lucas Santana Escobar
 * Creates operating system image suitable for placement on a boot disk
*/
/* TODO: Comment on the status of your submission. Largely unimplemented */
#include <assert.h>
#include <elf.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define IMAGE_FILE "./image"
#define ARGS "[--extended] [--vm] <bootblock> <executable-file> ..."

#define SECTOR_SIZE 512       /* floppy sector size in bytes */
#define BOOTLOADER_SIG_OFFSET 0x1fe /* offset for boot loader signature */
// more defines...

/* Reads in an executable file in ELF format*/
Elf32_Phdr * read_exec_file(FILE **execfile, char *filename, Elf32_Ehdr **ehdr){
  
  fread(*ehdr, 1, sizeof(Elf32_Ehdr), *execfile);
  if((*ehdr)->e_phoff != 0)
  {
    Elf32_Phdr *ret = malloc(sizeof(Elf32_Phdr));
    fread(ret, 1, sizeof(Elf32_Phdr), *execfile);
    return ret;
  }
  return NULL;
}

/* Writes the bootblock to the image file */
void write_bootblock(FILE **imagefile,FILE *bootfile,Elf32_Ehdr *boot_header, Elf32_Phdr *boot_phdr){
  void* text = malloc(boot_phdr->p_filesz);
  fread(text, 1, boot_phdr->p_filesz, bootfile);
  fwrite(text, 1, boot_phdr->p_filesz, *imagefile);
  //Padding
  void* padd = calloc(512 - boot_phdr->p_filesz%512,1);
  fwrite(padd, 1, 512 - boot_phdr->p_filesz%512 - 2, *imagefile);
  //Signature
  char* ass = malloc(2);
  ass[0] = 0x55;
  ass[1] = 0xAA;
  fwrite(ass, 1, 2, *imagefile);
  //Deallocating memory
  free(text);
}

/* Writes the kernel to the image file */
void write_kernel(FILE **imagefile,FILE *kernelfile,Elf32_Ehdr *kernel_header, Elf32_Phdr *kernel_phdr){
  void* text = malloc(kernel_phdr->p_filesz);
  fread(text, 1, kernel_phdr->p_filesz, kernelfile);
  //Writing on image
  fwrite(text, 1, kernel_phdr->p_filesz, *imagefile); 
  //Padding
  void* padd = calloc(512 - kernel_phdr->p_filesz%512,1);
  fwrite(padd, 1, 512 - kernel_phdr->p_filesz%512, *imagefile);
  //Deallocating memory
  free(text);
  free(padd);
}

/* Counts the number of sectors in the kernel */
int count_kernel_sectors(Elf32_Ehdr *kernel_header, Elf32_Phdr *kernel_phdr){
  int sector_nb = kernel_phdr->p_filesz/512.0; 
  return kernel_phdr->p_filesz%512 == 0?sector_nb:sector_nb+1;
}

/* Records the number of sectors in the kernel */
void record_kernel_sectors(FILE **imagefile,Elf32_Ehdr *kernel_header, Elf32_Phdr *kernel_phdr, int num_sec){
  fseek(*imagefile,2,SEEK_SET);
  fwrite(&num_sec, 1, sizeof(int), *imagefile);
}

/* Prints segment information for --extended option */
void extended_opt(Elf32_Phdr *bph, int k_phnum, Elf32_Phdr *kph, int num_sec, const char * boot_filename, const char * kernel_filename){

  // padding info calculation
  int padd_b = bph->p_filesz + (512 - bph->p_filesz % 512);
  int padd_k = padd_b + kph->p_filesz + (512 - kph->p_filesz % 512);
  
  /* print number of disk sectors used by the image */
  printf("Number of disk sectors used by the image: %d\n", padd_k / 512);

  /*bootblock segment info */
  printf("0x%04x: %s\n", bph->p_paddr, boot_filename);
  printf("\tsegment 0\n");
  printf("\t\toffset 0x%04x\t\t vaddr 0x%04x\n",bph->p_offset, bph->p_vaddr);
  printf("\t\tfilesz 0x%04x\t\t memsz 0x%04x\n", bph->p_filesz, bph->p_memsz);
  printf("\t\twriting 0x%04x bytes\n", bph->p_filesz);
  printf("\t\tpadding up to 0x%04x\n", padd_b);

  /* print kernel segment info */
  printf("0x%04x: %s\n", kph->p_paddr, kernel_filename);
  printf("\tsegment 0\n");
  printf("\t\toffset 0x%04x\t\t vaddr 0x%04x\n",kph->p_offset, kph->p_vaddr);
  printf("\t\tfilesz 0x%04x\t\t memsz 0x%04x\n", kph->p_filesz, kph->p_memsz);
  printf("\t\twriting 0x%04x bytes\n", kph->p_filesz);
  printf("\t\tpadding up to 0x%04x\n", padd_k);

  /* print kernel size in sectors */
  printf("os_size: %d sectors\n", num_sec);
}
// more helper functions...

/* MAIN */
// ignore the --vm argument when implementing (project 1)
int main(int argc, char **argv){
  FILE *kernelfile, *bootfile,*imagefile;  //file pointers for bootblock,kernel and image
  Elf32_Ehdr *boot_header = malloc(sizeof(Elf32_Ehdr));//bootblock ELF header
  Elf32_Ehdr *kernel_header = malloc(sizeof(Elf32_Ehdr));//kernel ELF header

  Elf32_Phdr *boot_program_header; //bootblock ELF program header
  Elf32_Phdr *kernel_program_header; //kernel ELF program header

  /* build image file */
  imagefile = fopen(IMAGE_FILE, "w");
  
  // get the correct bootfile name
  int hasExtended = !strncmp(argv[1],"--extended",11);
  char *bootfile_name = hasExtended?argv[2]:argv[1];
  bootfile = fopen(bootfile_name,"rb");
  if(!bootfile)
    return 1;

  /* read executable bootblock file */  
  boot_program_header = read_exec_file(&bootfile,bootfile_name,&boot_header);


  /* write bootblock */
  write_bootblock(&imagefile, bootfile, boot_header, boot_program_header);
  
  // get the correct kernelfile name
  char *kernelfile_name = hasExtended?argv[3]:argv[2];
  kernelfile = fopen(kernelfile_name,"rb");
  if(!kernelfile)
    return 1;

  /* read executable kernel file */
  kernel_program_header = read_exec_file(&kernelfile,kernelfile_name,&kernel_header);
  
  
  /* write kernel segments to image */
  write_kernel(&imagefile, kernelfile, kernel_header, kernel_program_header);


  /* tell the bootloader how many sectors to read to load the kernel */
  int sector_nb = count_kernel_sectors(kernel_header,kernel_program_header);
  record_kernel_sectors(&imagefile, kernel_header, kernel_program_header, sector_nb);
  
  
  /* check for  --extended option */
  if(!strncmp(argv[1],"--extended",11)){
	  // set reading position to the end of the file
    fseek(imagefile, 0, SEEK_END);
    /* print info */
    extended_opt(boot_program_header, sector_nb, kernel_program_header, sector_nb, bootfile_name, kernelfile_name);
  }
  fclose(imagefile);
  fclose(kernelfile);
  fclose(bootfile);

  return 0;
} // ends main()



