#include "writeonceFS.h"

void print_disk_block(DiskBlock block) {
  if (block == NULL) {
    printf("undefined block \n");
    return;
  }
  if (block == 0) {
    printf("empty disk block \n");
  }
  printf("%s \n", block);
  printf("done printing the block \n");
}

void print_pnode(PNode node) {
  for (int i=0; i<40; i++) {
    if (node.type == 'b') {
      printf("Index: %d \n", node.direct[i]);
    } else {
      print_pnode(*(PNode*) &node.direct[i]);
    }
  }
}

void print_inode(INode node) {
  printf("INode %s: %d bytes and %d blocks \n", node.name, node.bytes, node.blocks);
}

void print_node(Node node) {
  if (node.type == 'i') {
    print_inode(*(INode*) &node);
  } else if (node.type == 'u'){
    printf("Unused node \n");
  }
}

void print_superblock(SuperBlock sb) {
  printf("Valid disk char: %c \n", sb.valid);
  for (int i = 0; i<5; i++) {
    for (int j = 0; j < 8; j++) {
      // Mask each bit in the byte and print it
      printf("%d ", !!((sb.bitmap[i] << j) & 0x80));
    }
    printf("\n");
  }
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
