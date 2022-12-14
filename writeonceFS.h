#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define DISK_SIZE 4194304 
#define BLOCK_QUANTA 1024

#define PNODE_ARR 33
#define INODE_DIR 15
#define INODE_IND 10
#define INODE_DIND 3
#define BITMAP_SIZE 509

//idk if these need to be anything in particular
#define WO_RDONLY 1
#define WO_WRONLY 2
#define WO_RDWR 3
#define WO_CREATE 4

typedef char DiskBlock[BLOCK_QUANTA];

// generic type that we can cast from
typedef struct Node {
  char type;
  int data[40];
} Node;

typedef struct PNode {
  char type;
  int direct[40];
} PNode; // 248 bytes

typedef struct INode {
  char type;
  unsigned int bytes;
  unsigned int blocks;
  char name[32];
  int direct[INODE_DIR]; // change this to index since the memory addresses will change in between mounts
  int s_indirect[INODE_IND];
  int d_indirect[INODE_DIND];
  int fd; // if there is room to increase the size like this
  int mode;// then this is the file descriptor always associated with the file
} INode; // 248 bytes

typedef struct SuperBlock {
  char valid;
  char bitmap[509];
  unsigned short free_count;
  char padding[4]; // this allows us to fit in one disk perfectly
} SuperBlock;

#define NODES 103
#define BLOCKS (DISK_SIZE-sizeof(SuperBlock)-(sizeof(Node)*NODES))/BLOCK_QUANTA

typedef struct Disk {
  SuperBlock sb;
  Node nodes[NODES];
  DiskBlock blocks[BLOCKS];
} Disk;

int wo_mount(char* file_name, void* mem_addr);
int wo_unmount(void* mem_addr);

int wo_open(char* file_name, int flags, ...);

int wo_read(int fd, void* buff, int bytes);
int wo_write(int fd, void* buff, int bytes);
int wo_close(int fd);

void print_disk_block(DiskBlock block);
void print_pnode(PNode node);
void print_inode(INode node);
void print_node(Node node);
void print_superblock(SuperBlock sb);
void print_disk(Disk* disk);
