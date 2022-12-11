#include "print.h"

void print_disk_block(DiskBlock block) {
  if (block == NULL || block == 0) {
    return;
  }
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
      print_disk_block(*(DiskBlock*) &node.direct[i]);
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

void print_superblock(SuperBlock sb) {
  printf("Valid disk char: %c \n", sb.valid);
}

void print_disk(Disk* disk) {
  print_superblock(disk->sb);
  for (int i = 0; i<NODES; i++) {
    print_node(disk->nodes[i]);
  }
  for (int i = 0; i<BLOCKS; i++) {
    print_disk_block(disk->blocks[i]);
  }
}