#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "../unicorn/include/unicorn/unicorn.h"

#define PT_LOAD  1
#define EM_ARM   40

typedef struct {
    uint8_t  e_ident[16];
    uint16_t e_type,e_machine;
    uint32_t e_version,e_entry,e_phoff,e_shoff,e_flags;
    uint16_t e_ehsize,e_phentsize,e_phnum,e_shentsize,e_shnum,e_shstrndx;
} Elf32_Ehdr;

typedef struct {
    uint32_t p_type,p_offset,p_vaddr,p_paddr,p_filesz,p_memsz,p_flags,p_align;
} Elf32_Phdr;

uint32_t elf_load(void *uc, const char *path,
                  uint32_t fb, uint32_t fs, uint32_t sb, uint32_t ss) {
    (void)fb;(void)fs;(void)sb;(void)ss;
    FILE *f=fopen(path,"rb");
    if(!f){fprintf(stderr,"[veemu] cannot open elf: %s\n",path);return 0;}
    Elf32_Ehdr eh;
    if(fread(&eh,sizeof(eh),1,f)!=1){fclose(f);return 0;}
    if(eh.e_ident[0]!=0x7F||eh.e_ident[1]!='E'||
       eh.e_ident[2]!='L' ||eh.e_ident[3]!='F'){
        fprintf(stderr,"[veemu] not ELF\n");fclose(f);return 0;}
    if(eh.e_machine!=EM_ARM){
        fprintf(stderr,"[veemu] not ARM ELF\n");fclose(f);return 0;}
    fprintf(stderr,"[veemu] elf entry=0x%08X segments=%d\n",eh.e_entry,eh.e_phnum);
    for(int i=0;i<eh.e_phnum;i++){
        Elf32_Phdr ph;
        fseek(f,(long)(eh.e_phoff+i*sizeof(ph)),SEEK_SET);
        if(fread(&ph,sizeof(ph),1,f)!=1) continue;
        if(ph.p_type!=PT_LOAD||!ph.p_filesz) continue;
        uint8_t *data=malloc(ph.p_filesz);
        if(!data) continue;
        fseek(f,(long)ph.p_offset,SEEK_SET);
        if(fread(data,1,ph.p_filesz,f)!=ph.p_filesz){free(data);continue;}
        uc_err e=uc_mem_write((uc_engine*)uc,ph.p_vaddr,data,ph.p_filesz);
        fprintf(stderr,"[veemu] seg vaddr=0x%08X size=%u %s\n",
                ph.p_vaddr,ph.p_filesz,e==UC_ERR_OK?"OK":"FAIL");
        free(data);
    }
    fclose(f);
    return eh.e_entry;
}
