#include "writeonceFS.h"
#include <stdio.h>
#include <errno.h> //apparently we have to set this every time we return an error
#include <stdarg.h>
#include <math.h>

#define DEBUG 1

static FILE* disk_file;
static Disk* loaded_disk;
INode* open_files [NODES]; //index is the file descriptor, contains a pointer to the INode
INode* open_modes [NODES]; //track the mode of every file. lets add this as a field in the inode - allen 

int file_count = 0; 

void create_disk(char* file_name) {
  FILE* fdisk = fopen(file_name, "wb"); // open up the the file for reading
  fclose(fdisk);
}

void format_disk(char* file_name) {
  FILE* fdisk = fopen(file_name, "w"); // open up the the file for writing
  Disk* default_disk = malloc(sizeof(Disk));
  SuperBlock* default_sb = malloc(sizeof(SuperBlock));
  default_sb->valid = 't';
  memset(default_sb->bitmap, 0, BITMAP_SIZE);
  default_sb->free_count = BITMAP_SIZE * 8;

  fwrite(default_sb, sizeof(SuperBlock), 1, fdisk);

  // make the array for the nodes
  Node* nodes = malloc(NODES * sizeof(Node));
  for (int i=0; i< NODES; i++) {
    nodes[i].type = 'u'; // u is for unused
    memset(nodes[i].data, 0, 33);
    fwrite(&nodes[i], sizeof(Node), 1, fdisk);
  }

  // make the array for the DiskBlocks
  DiskBlock* blocks = malloc(BLOCKS * sizeof(DiskBlock));
  for (int i=0; i< BLOCKS; i++) {
    memset(blocks[i], 0, 1024);
    fwrite(&blocks[i], sizeof(DiskBlock), 1, fdisk);
  }

  fclose(fdisk);
}

//Read in diskfile
int wo_mount(char* file_name, void* mem_addr) {
  printf("mounting at %p \n", mem_addr);
  disk_file = fopen(file_name, "r+"); // open up the the file for reading
  if (disk_file == NULL) {
    // fclose(disk_file); //can not close a null disk file
    if(DEBUG) printf("Disk does not exist, creating... \n");
    create_disk(file_name);
    if(DEBUG) printf("Formatting disk... \n");
    format_disk(file_name);
    disk_file = fopen(file_name, "r+"); // open up the the file for reading
    if(DEBUG) printf("done \n");
  }

  Disk* disk = (Disk*) mem_addr;

  // if bytes read is zero then we also need to build it
  int bytes_read = fread(&disk->sb, sizeof(SuperBlock), 1, disk_file);
  if (bytes_read == 0) {
    if(DEBUG) printf("Disk is empty. Formatting.\n");
    format_disk(file_name);
  } else {
    if(DEBUG) printf("Read %d bytes of SuperBlock. \n", (int) sizeof(SuperBlock));
  }
  
  // if the valid byte on the super block isnt set, then it is invalid
  if(disk->sb.valid != 't') {
    if(DEBUG) printf("Invalid block. Exiting. \n");
    return -1;
  }

  // read in all of the nodes that are on the disk
  for (int i = 0; i < NODES; i++) {
    fread(&disk->nodes[i], sizeof(Node), 1, disk_file);
  }

  for (int i=0; i<BLOCKS; i++) {
    fread(&disk->blocks[i], sizeof(DiskBlock), 1, disk_file);
  }

  mem_addr = (void*) &disk;
  if(DEBUG) printf("Successfully loaded the disk \n");
  fclose(disk_file);
  fopen(file_name, "w+"); // open the file for writing
  loaded_disk = disk;
  return 0;
}


//Write out entire diskfile
int wo_unmount(void* mem_addr) {
  Disk* disk = (Disk*) mem_addr;
  if(DEBUG){
    printf("%p on unmount \n", disk);
    print_superblock(disk->sb);
  }
  
  fwrite(&disk->sb, sizeof(SuperBlock), 1, disk_file);

  for (int i = 0; i < NODES; i++) {
    if(DEBUG){
      if (disk->nodes[i].type == 'i') {
        print_inode(*(INode*) &disk->nodes[i]);
      }
    }
    fwrite((void*) &(disk->nodes[i]), sizeof(Node), 1, disk_file);
  }

  for (int i=0; i<BLOCKS; i++) {
    fwrite((void*) &disk->blocks[i], sizeof(DiskBlock), 1, disk_file);
  }

  fclose(disk_file);
  return 0;
}

// return the node at the given index.
// this can be an inode or a pnode
Node* get_node(int index) {
  return &loaded_disk->nodes[index];
}

// return the diskblock at the given index
DiskBlock* get_diskblock(int index) {
  return &loaded_disk->blocks[index];
}

// populate DiskBlock with PNode, if the PNode is of type 'b' (block)
void load_pnode(PNode* input, DiskBlock** diskblocks, int* index, int blocks) {
  if (input->type == 'b') {
    for (int i = 0; i < 40 && index < blocks; i++) {  
      DiskBlock* current_block = get_diskblock(input->direct[i]);
      diskblocks[*index] = current_block;
      index++;
    }
  } else {
    for (int i = 0; i < 40 && index < blocks; i++) { 
      PNode* current_pnode = (PNode*) get_node(input->direct[i]);
      load_pnode(current_pnode, diskblocks, index, blocks);
     }
  }
  
}

// return the blocks of an iNode
DiskBlock** get_diskblocks(INode* input) {
  print_inode(*input);
  int blocks = input->blocks;
  int index = 0;
  DiskBlock** diskblocks = (DiskBlock*) malloc(sizeof(DiskBlock) * blocks);
  while (index < blocks) {
    // direct
    if (index < INODE_DIR) { 
      if (!input || !input->direct || input->direct[index] != NULL) {
        printf("the disk block is null \n");
        return NULL;
      }
      DiskBlock* current_block = get_diskblock(input->direct[index]);
      diskblocks[index] = current_block;
      index++;
      continue;
    }

    // single indirect
    if (index < INODE_DIR + INODE_IND) {
      // we need to load in 40 blocks per PNode
      PNode* current_pnode = (PNode*) get_node(input->s_indirect[index - INODE_DIR]);
      load_pnode(current_pnode, diskblocks, &index, blocks);
      continue;
    }

    // double indirect
    if (index < INODE_DIR + INODE_IND + INODE_DIND){
      PNode* current_pnode = (PNode*) get_node(input->s_indirect[index - INODE_DIR - INODE_DIND]);
      load_pnode(current_pnode, diskblocks, &index, blocks);
    }
  }
  return diskblocks;
}

int get_bit(int index) {
  int byte = index / 8;
  int bit = index % 8;
  return !!((loaded_disk->sb.bitmap[byte] & 1 << bit));
}

void set_bit(int index) {
  int byte = index / 8;
  int bit = index % 8;
  loaded_disk->sb.bitmap[byte] |= (1<<bit);
}

int first_free_diskblock() {
  if(DEBUG) printf("getting the first diskblock \n");
  for (int i = 0; i<BITMAP_SIZE; i++) {
    for (int j = 0; j < 8; j++) {
      // Mask each bit in the byte and store it
      int current_bit = loaded_disk->sb.bitmap[i] & (1 << j) != 0;
      int current_index = i*j + j;
      if (current_bit == 0) {
        if(DEBUG) printf("found at %p \n", &loaded_disk->blocks[i]);
        return current_index;
      }
    }
  }
  return -1; // this is if we cannot find a free diskblock
}

// we need to return a file descriptor
// we need macros for all the flags probably
// the mode part should be an optional argument
// can use https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/va-arg-va-copy-va-end-va-start?view=msvc-170
// in class i actually thought he said to split it into open and create - two sep functions
// but i can't see anyone else saying that on piazza or the discord
// and i arrived at the end of that discussion so i guess optional arguments it is

//int wo_open(char* file_name, int flags, int mode) {
int wo_open(char* file_name, int flags, ...) {

  int free_index = -1;
  for (int i=0; i<NODES; i++) {
    // keep track of the first free INode in case we don't find the file
    if (free_index == -1 && loaded_disk->nodes[i].type == 'u') { 
      free_index = i;
      continue;
    }

    // if it is not an INode, just ignore
    if (loaded_disk->nodes[i].type != 'i') { continue; }
    INode* curr_node = &loaded_disk->nodes[i];
    if (strcmp(curr_node->name, file_name) == 0) {
      if(DEBUG) printf("match found with %d blocks \n", curr_node->blocks);
      curr_node->fd = file_count; 
      curr_node->mode = flags;
      file_count++; 
      open_files[curr_node->fd] = curr_node;
      open_modes[curr_node->fd] = flags; // should we check if flag is correct type?
      return curr_node->fd; 
    }
  }

  va_list ap; 
  va_start(ap, flags); //flags is the last known argument
  int mode = va_arg(ap, int); // result is undefined if the third argument does not exist
  if(mode == WO_CREATE){ 
    // then do stuff
    // creating file
    INode* allocating = (Node*) &loaded_disk->nodes[free_index];
    if(DEBUG) printf("Creating file at address %p and index %d \n", allocating, free_index);
    allocating->type = 'i';
    allocating->blocks = 1;
    int first_free = first_free_diskblock();
    printf("free diskblock at index %d \n", first_free);
    print_disk_block(*get_diskblock(first_free));
    set_bit(first_free);
    allocating->direct[0] = first_free;
    allocating->bytes = 0; // this is so we know where to start writing in the blocks
    strcpy(allocating->name, file_name);

    //need to check this
    allocating->fd = file_count; 
    allocating->mode = flags;
    file_count++; 
    open_files[allocating->fd] = allocating;
    return allocating->fd; 
    // return 0;
  }

}

//are we supposed to somehow update the position in the file
//that we are reading from?? because it doesn't say that
//so can we assume that we always read from the start?
int wo_read( int fd,  void* buffer, int bytes){
  INode* target = open_files[fd];
  if (!target) { 
    printf("target is null! \n");
    return -1; 
  }
  if(open_files[fd] == NULL){
    //set errno!!!
    printf("fd is null! \n");
    return -1;
  }
  if(open_modes[fd] == WO_WRONLY){ //we can only write in this case, can't read
    //set errno!!!
    printf("you don't have permission! \n");
    return -1;
  }
  printf("reading the following file \n");

  //not sure this is right
  DiskBlock** diskblocks = get_diskblocks(target);

  // for testing purposes
  print_disk_block(diskblocks[0]);
  memcpy(buffer, diskblocks[0], bytes);
  return 0;

  // get the list of the disk blocks
  int block_index = floor( (double) target->bytes / (double) BLOCK_QUANTA);
  // TODO: consider the case where the most recent disk block is not full
  for (int i = 0; i < target->bytes && i < bytes; i += BLOCK_QUANTA) {
    printf("gettings the %dth disk block \n", block_index);
    memcpy(buffer + i, diskblocks[block_index], BLOCK_QUANTA);
    block_index++;
  }
  return 0;
}

int wo_write(int fd,  void* buffer, int bytes){
  if(open_files[fd] == NULL){
    //set errno!!!
    printf("fd is null! \n");
    return -1;
  }
  if(open_modes[fd] == WO_RDONLY){ //we can only read in this case, can't read
    //set errno!!!
    printf("you don't have permission! \n");
    return -1;
  }

  INode* target = open_files[fd];
  if (!target) { return -1; }

  // write to diskblocks in chunks of 1024
  int blocks_needed;
  // make sure we have enough space to write to
  if (ceil((double) bytes / (double) BLOCK_QUANTA) > target->blocks) {
    // we need to allocate more blocks
    blocks_needed = ceil( (double) bytes / (double) BLOCK_QUANTA) - target->blocks; // TODO: make sure this always rounds up forreal
    for (int i = target->blocks; i < blocks_needed; i++) {
      int index_newblock = first_free_diskblock();
      if (i < INODE_DIR) {
        target->direct[i] = index_newblock;
      } else if (i < INODE_IND) {
        // assign the first free pointer in this pnode to be the block
      } else {
        // assign the first free pointer in this pnode to be the block
      }
    }
  }

  // get the list of the disk blocks
  DiskBlock** diskblocks = get_diskblocks(target);

  // for testnig we will write to the first diskblock
  // only write the given amount of bytes
  memcpy(diskblocks[0], buffer, bytes);
  target->bytes = bytes;
  target->blocks = blocks_needed;
  return 0;

  int block_index = ceil( (double) target->bytes / (double) BLOCK_QUANTA);
  // TODO: consider the case where the most recent disk block is not full
  for (int i = target->bytes; i < target->bytes + bytes; i += BLOCK_QUANTA) {
    printf("copying bytes %d-%d to index %d \n", i, i+BLOCK_QUANTA, block_index);
    printf("%p \n", diskblocks[block_index]);
    memcpy(diskblocks[block_index], buffer + i, BLOCK_QUANTA);
    block_index++;
  }
  // TODO: what happens if we are slightly over an increment of 1024

  // update the size of the file
  target->bytes = target->blocks + bytes;
  target->blocks = blocks_needed;

  return 0; 
}


int wo_close(int fd){
  if(open_files[fd]!= NULL){
    open_files[fd] == NULL; 
    return 0;
  }
  return -1; 
}

void check_size() {
  // these three should be the same
  printf("INode: %d \n", sizeof(INode));
  printf("PNode: %d \n", sizeof(PNode));
  printf("Node: %d \n", sizeof(Node));
  // This should be 0
  printf("Size difference: %d \n", DISK_SIZE - sizeof(Disk));
}

int main(int argc, char** args) {
  
  // whenever you change the metadata make sure to run this
  // check_size(); return 0;

  Disk* disk = malloc(sizeof(Disk));
  int mount_result = wo_mount("test_disk1", disk);
  int open_result = wo_open("test_file1", 0,  WO_CREATE);
  printf("open result: %d \n", open_result);
  char buffer[32];
  strcpy(buffer, "hello, world");
  char* read_buffer = malloc(sizeof(char) * 32);
  printf("buffer address %p \n", read_buffer);
  int write_result = wo_write(open_result, buffer, 32);
  int read_result = wo_read(open_result, read_buffer, 32);
  if (read_result >= 0) {
    printf("read result: %s \n", read_result);
  } else {
    printf("buffer result: %d \n", read_result);
  }
  // int open_result2 = wo_open("test_file1", 0, 0);
  int unmount_result = wo_unmount(disk);

  return 0;
}