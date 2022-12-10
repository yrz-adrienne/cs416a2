#include "writeonceFS.h"
#define DISK_SIZE 4194304 
#define BLOCK_QUANTA 1024
#define BLOCKS DISK_SIZE/BLOCK_QUANTA

typedef char DiskBlock[BLOCK_QUANTA];
typedef char Node[30]; // Generic Node type that we can cast from

typedef struct PNode {
  DiskBlock* direct[30];
} PNode; // 240 bytes

typedef struct INode {
  unsigned int id;
  unsigned int bytes;
  unsigned int blocks;
  DiskBlock* direct[15];
  PNode* s_indirect[10];
  PNode** d_indirect[3];
} INode; // 240 bytes

typedef struct SuperBlock {
  char bitmap[256];
  unsigned short free_count;
} SuperBlock;

int wo_mount(char* file_name, void* mem_addr) {
  FILE* fdisk = fopen(file_name, "rb"); // open up the the file for reading
  SuperBlock input;
  fread(&input, sizeof(SuperBlock), 1, fdisk);
  printf("%s \n", input.bitmap);
  printf("%d \n", input.free_count);

  return 0;
}

int main(int argc, char** args) {

  FILE* fdisk = fopen("test", "wb"); // open up the the file for writing
  char test[256];
  
  test[0] = 'a';
  test[1] = 'a';
  test[2] = '\0';
  SuperBlock write_sb;
  memcpy(write_sb.bitmap, test, 256);
  write_sb.free_count = 77;
  printf("%s \n", write_sb.bitmap);
  fwrite(&write_sb, sizeof(SuperBlock), 1, fdisk);
  fclose(fdisk);

  char* disk = malloc(sizeof(char) * DISK_SIZE);
  wo_mount("test", disk);

  return 0;
}