#include "writeonceFS.h"
#define DISK_SIZE 4194304 
#define BLOCK_QUANTA 1024

typedef char DiskBlock[BLOCK_QUANTA];

// generic type that we can cast from
typedef struct Node {
  char type;
  char* data[33];
} Node;

typedef struct PNode {
  char type;
  void* direct[33];
} PNode; // 248 bytes

typedef struct INode {
  char type;
  unsigned int bytes;
  unsigned int blocks;
  char name[32];
  DiskBlock* direct[15];
  PNode* s_indirect[10];
  PNode* d_indirect[3];
} INode; // 248 bytes

typedef struct SuperBlock {
  char valid;
  char bitmap[256];
  unsigned short free_count;
  char padding[124]; // this allows us to fit in one disk perfectly
} SuperBlock;

#define NODES 104
#define BLOCKS (DISK_SIZE-sizeof(SuperBlock)-(sizeof(Node)*NODES))/BLOCK_QUANTA

typedef struct Disk {
  SuperBlock sb;
  Node nodes[NODES];
  DiskBlock blocks[BLOCKS];
} Disk;

void create_disk(char* file_name) {
  FILE* fdisk = fopen(file_name, "wb"); // open up the the file for reading
  fclose(fdisk);
}

void format_disk(char* file_name) {
  FILE* fdisk = fopen(file_name, "wb"); // open up the the file for writing
  Disk* default_disk = malloc(sizeof(Disk));
  SuperBlock* default_sb = malloc(sizeof(SuperBlock));
  default_sb->valid = 't';
  memset(default_sb->bitmap, 0, 256);
  default_sb->free_count = 256 * 8;

  fwrite(default_sb, sizeof(SuperBlock), 1, fdisk);

  // make the arry for the nodes
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

void print_disk_block(DiskBlock block) {
  char* cp = block;
  for ( ; *cp != '\0'; ++cp )
  {
    printf("%02x", *cp);
  }
  printf("\n");
}

void print_pnode(PNode node) {
  for (int i=0; i<33; i++) {
    if (node.type == 'b') {
      print_disk_block((DiskBlock*) &node.direct[i]);
    } else {
      print_pnode(*(PNode*) &node.direct[i]);
    }
  }
}
void print_inode(INode node) {
  printf("INode %s: %d bytes and %d blocks \n", node.name, node.bytes, node.blocks);
  for (int i = 0; i < 10; i ++) {
    if (node.s_indirect[i] == NULL) { break; }
    print_pnode(*node.s_indirect[i]); // maybe only print the address?
  }
  for (int i = 0; i < 3; i ++) {
    if (node.d_indirect[i] == NULL) { break; }
    print_pnode(*node.d_indirect[i]); // maybe only print the address?
  }
}

void print_node(Node node) {
  if (node.type == 'i') {
    print_inode(*(INode*) &node);
  } else if (node.type == 'u'){
    printf("unused node \n");
  }
}

int wo_mount(char* file_name, void* mem_addr) {
  int create_disk_flag = 0;
  FILE* fdisk = fopen(file_name, "rb"); // open up the the file for reading
  if (fdisk == NULL) {
    printf("Disk does not exist, creating... \n");
    create_disk(file_name);
    format_disk(file_name);
  }

  // if the file doesn't exist then build the disk structs
  SuperBlock input;

  // if bytes read is zero then we also need to build it
  if (!create_disk_flag) {
    int bytes_read = fread(&input, sizeof(SuperBlock), 1, fdisk);
    if (bytes_read == 0) {
      printf("disk is empty\n");
      format_disk(file_name);
    }
  }
  
  // if the valid byte on the super block isnt set, then it is invalid
  printf("valid char: %c \n", input.valid);
  if(input.valid != 't') {
    printf("invalid block \n");
    format_disk(file_name);
    return -1;
  }

  // read in all of the nodes that are on the disk
  Node nodes[NODES];
  for (int i = 0; i < NODES; i++) {
    fread(&nodes[i], sizeof(Node), 1, fdisk);
    if (nodes[i].type == 'p') {
      printf("found a p node at index %d \n", i);
    } else if(nodes[i].type == 'i'){
      printf("found an i node at index %d \n", i);
    } else {
      printf("found %c \n", nodes[i].type);
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

  for (int i = 0; i < NODES; i ++ ) {
    if (i < 50) {
      fwrite(&itemp, sizeof(INode), 1, fdisk);
    } else {
      fwrite(&ptemp, sizeof(PNode), 1, fdisk);
    }
  }

  fclose(fdisk);

  char* disk = malloc(sizeof(char) * DISK_SIZE);
  wo_mount("test1", disk);
  wo_mount("test", disk);

  return 0;
}