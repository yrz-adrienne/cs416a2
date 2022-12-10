#include "writeonceFS.h"
#define DISK_SIZE 4194304 
#define BLOCK_QUANTA 1024

typedef char DiskBlock[BLOCK_QUANTA];
typedef char* Node[30]; // Generic Node type that we can cast from

typedef struct PNode {
  char type;
  DiskBlock* direct[29];
} PNode; // 248 bytes

typedef struct INode {
  char type;
  unsigned int id;
  unsigned int bytes;
  unsigned int blocks;
  DiskBlock* direct[15];
  PNode* s_indirect[10];
  PNode** d_indirect[3];
} INode; // 248 bytes

typedef struct SuperBlock {
  char valid;
  char bitmap[256];
  unsigned short free_count;
  char padding[76]; // this allows us to fit in one disk perfectly
} SuperBlock;

#define NODES 101
#define BLOCKS (DISK_SIZE-sizeof(SuperBlock)-(sizeof(Node)*NODES))/BLOCK_QUANTA

typedef struct Disk {
  SuperBlock sb;
  Node nodes[NODES];
  DiskBlock blocks[BLOCKS];
} Disk;

int wo_mount(char* file_name, void* mem_addr) {
  int create_disk_flag = 0;
  printf("hello \n");
  FILE* fdisk = fopen(file_name, "rb"); // open up the the file for reading
  if (fdisk == NULL) {
    printf("Disk does not exist \n");
    create_disk_flag = 1;
    return -1;
  }
  // if the file doesn't exist then build the disk structs

  SuperBlock input;

  // if bytes read is zero then we also need to build it
  if (!create_disk_flag) {
    int bytes_read = fread(&input, sizeof(SuperBlock), 1, fdisk);
    if (bytes_read == 0) {
      printf("disk is empty\n");
      create_disk_flag = 1;
    }
  }

  // check if we need to create the disk
  if (create_disk_flag) {
    SuperBlock sb;
    char byte_mask[256];

  }
  
  printf("%s \n", input.bitmap);
  printf("%d \n", input.free_count);
  if(input.valid != 't') {
    printf("invalid block \n");
    return -1;
  }
  // if the valid byte on the super block isnt set, then it is invalid

  for (int i = 0; i < 100; i++) {
    Node temp;
    fread(&temp, sizeof(Node), 1, fdisk);
    if (temp[0] == 'p') {
      printf("found a p node at index %d \n", i);
    } else if(temp[0] == 'i'){
      printf("found an i node at index %d \n", i);
    } else {
      printf("found nothing \n");
    }
  }

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
  write_sb.valid = 't';
  write_sb.free_count = 77;
  printf("%s \n", write_sb.bitmap);
  fwrite(&write_sb, sizeof(SuperBlock), 1, fdisk);

  // The difference needs to be zero
  printf("%d \n", DISK_SIZE - sizeof(Disk));

  // All of these structs need to be the same size
  printf("%d \n", sizeof(PNode));
  printf("%d \n", sizeof(INode));
  printf("%d \n", sizeof(Node));

  INode itemp;
  itemp.type = 'i';
  PNode ptemp;
  ptemp.type = 'p';

  for (int i = 0; i < 100; i ++ ) {
    if (i < 50) {
      fwrite(&itemp, sizeof(INode), 1, fdisk);
    } else {
      fwrite(&ptemp, sizeof(PNode), 1, fdisk);
    }
  }

  fclose(fdisk);

  char* disk = malloc(sizeof(char) * DISK_SIZE);
  wo_mount("test", disk);

  return 0;
}