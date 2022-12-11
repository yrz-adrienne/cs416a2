#include "writeonceFS.h"
#include <stdio.h>

#define DEBUG 1

static FILE* disk_file;
static Disk disk;

void create_disk(char* file_name) {
  FILE* fdisk = fopen(file_name, "wb"); // open up the the file for reading
  fclose(fdisk);
}

void format_disk(char* file_name) {
  FILE* fdisk = fopen(file_name, "w"); // open up the the file for writing
  Disk* default_disk = malloc(sizeof(Disk));
  SuperBlock* default_sb = malloc(sizeof(SuperBlock));
  default_sb->valid = 't';
  memset(default_sb->bitmap, 0, 256);
  default_sb->free_count = 256 * 8;

  fwrite(default_sb, sizeof(SuperBlock), 1, fdisk);

  // make the array for the nodes
  Node* nodes = malloc(NODES * sizeof(Node));
  for (int i=0; i< NODES; i++) {
    nodes[i].type = 'u'; // u is for unused
    memset(nodes[i].data, 0, 33);
    fwrite(&nodes[i], sizeof(Node), 1, fdisk);
  }

  // make the arry for the DiskBlocks
  DiskBlock* blocks = malloc(BLOCKS * sizeof(DiskBlock));
  for (int i=0; i< BLOCKS; i++) {
    memset(blocks[i], 0, 1024);
    fwrite(&blocks[i], sizeof(DiskBlock), 1, fdisk);
  }

  fclose(fdisk);
}

int wo_mount(char* file_name, void* mem_addr) {
  disk_file = fopen(file_name, "r+"); // open up the the file for reading
  if (disk_file == NULL) {
    //fclose(disk_file); can't close a null file pointer
    if(DEBUG){
      printf("Disk does not exist, creating... \n");
    }
    create_disk(file_name);
    if(DEBUG){
      printf("Formatting disk... \n");
    }
    format_disk(file_name);
    disk_file = fopen(file_name, "r+"); // open up the the file for reading
    if(DEBUG) printf("done \n");
  }

  // if bytes read is zero then we also need to build it
  int bytes_read = fread(&disk.sb, sizeof(SuperBlock), 1, disk_file);
  if (bytes_read == 0) {
    if(DEBUG) printf("Disk is empty. Formatting.\n");
    format_disk(file_name);
    bytes_read = fread(&disk.sb, sizeof(SuperBlock), 1, disk_file); //update by reading newly formatted superblock
    if(DEBUG) printf("read %d bytes\n", bytes_read); 

  } else {
    //printf("Read %d bytes of SuperBlock. \n", (int) sizeof(SuperBlock));
    if(DEBUG) printf("read %d elements of %d (superblock) size\n", bytes_read, (int) sizeof(SuperBlock)); 
  }
  
  // if the valid byte on the super block isnt set, then it is invalid
  if(disk.sb.valid != 't') {
    printf("Invalid block. Exiting. \n");
    return -1;
  }

  // read in all of the nodes that are on the disk
  for (int i = 0; i < NODES; i++) {
    fread(&disk.nodes[i], sizeof(Node), 1, disk_file);
  }

  for (int i=0; i<BLOCKS; i++) {
    fread(&disk.blocks[i], sizeof(DiskBlock), 1, disk_file);
  }

  memcpy(mem_addr, &disk, sizeof(Disk));
  printf("Successfully loaded the disk \n");
  fclose(disk_file);
  fopen(file_name, "w+"); // open the file for writing
  return 0;
}

int wo_unmount(void* mem_addr) {
  Disk disk = *(Disk*) &mem_addr;
  
  fwrite(&disk.sb, sizeof(SuperBlock), 1, disk_file);

  for (int i = 0; i < NODES; i++) {
    fwrite((void*) &(disk.nodes[i]), sizeof(Node), 1, disk_file);
  }

  for (int i=0; i<BLOCKS; i++) {
    fwrite((void*) &disk.blocks[i], sizeof(DiskBlock), 1, disk_file);
  }

  fclose(disk_file);
  return 0;
}

int main(int argc, char** args) {
  Disk* disk = malloc(sizeof(Disk));

  //test with creating a new disk
  //on second run, when this disk exists already, cannot print valid disk char
  printf("disk does not exist\n");
  int resultnew = wo_mount("test_new", disk);
  print_superblock(disk->sb);
  int unmount_resultnew = wo_unmount(disk);

  //test_empty - create a new file each time with nothing in it
  printf("disk exists but is empty\n");
  int resultempty = wo_mount("test_empty", disk);
  print_superblock(disk->sb);
  int unmount_resultempty = wo_unmount(disk);

  //read something previously created by this program
  //this doesn't work - first run, valid disk char = t but marked as invalid
  printf("disk was previously created\n");
  int resultold = wo_mount("test_new", disk);
  print_superblock(disk->sb);
  int unmount_resultold = wo_unmount(disk);

  return 0;
}