#include "writeonceFS.h"
#define DISK_SIZE 1048576

typedef char DiskBlock[1024];

typedef struct PNode {
  DiskBlock* direct[25];
} PNode; // 200 bytes

typedef struct PNode_p {
  PNode* indirect[25];
} PNode_p; // 200 bytes

typedef struct INode {
  unsigned int number;
  unsigned int size;
  unsigned int blocks;
  DiskBlock* direct[10];
  PNode* indirect[8];
  PNode_p* inindirect[5];
} INode; // 200 bytes

typedef struct SuperBlock {
  INode* free;
} SuperBlock;

int wo_mount(char* file_name, void* mem_addr) {
  FILE* fdisk = fopen(file_name, "rb"); // open up the the file for reading and writing
  int result = fread(mem_addr, sizeof(char), DISK_SIZE, fdisk); // read the contents of the file into the byte array

  if (result == 0) {
    printf("created the disk \n");
    return -1;
  }

  return 0;
}

int main(int argc, char** args) {
  char* disk = malloc(sizeof(char) * DISK_SIZE);
  wo_mount("test.txt", disk);
  printf("%s \n", disk);

  printf("%d \n", sizeof(INode));
  printf("%d \n", sizeof(PNode));
  printf("%d \n", sizeof(DiskBlock));


  return 0;
}