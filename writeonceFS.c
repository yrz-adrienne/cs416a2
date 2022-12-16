#include "writeonceFS.h"
#include <stdio.h>
#include <errno.h> //apparently we have to set this every time we return an error
#include <stdarg.h>
#include <math.h>


#define DEBUG 0

static FILE* disk_file;
static Disk* loaded_disk;
INode* open_files [NODES]; //index is the file descriptor, contains a pointer to the INode

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
  if(DEBUG) printf("mounting at %p \n", mem_addr);
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
    errno = EBADMSG; 
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

// populate diskblocks array with blocks 
void load_pnode(PNode* input, DiskBlock** diskblocks, int* index, int blocks) {
  if (input->type == 'b') {
    for (int i = 0; i < NODE_DATA && index < blocks; i++) {  
      DiskBlock* current_block = get_diskblock(input->direct[i]);
      diskblocks[*index] = current_block;
      index++;
    }
  } else {
    for (int i = 0; i < NODE_DATA && index < blocks; i++) { 
      PNode* current_pnode = (PNode*) get_node(input->direct[i]);
      load_pnode(current_pnode, diskblocks, index, blocks);
     }
  }
  
}

// return the blocks of an iNode
DiskBlock** get_diskblocks(INode* input) {
  if(DEBUG) print_inode(*input);
  int blocks = input->blocks;
  int index = 0;
  DiskBlock** diskblocks = (DiskBlock*) malloc(sizeof(DiskBlock) * blocks);
  while (index < blocks) {
    
    // direct
    if (index < INODE_DIR) { 
      DiskBlock* current_block = get_diskblock(input->direct[index]);
      if(DEBUG) printf("%d -> %d \n", index, input->direct[index]);
      diskblocks[index] = current_block;
      index++;
      continue;
    }

    // single indirect
    if (index >= INODE_DIR && index < INODE_DIR + INODE_IND * NODE_DATA) {
      // we need to load in 40 blocks per PNode
      int block_index = index - INODE_DIR;
      int pnode_index = floor(index / NODE_DATA);
      if (input->s_indirect[pnode_index] == -1) { break; }
      PNode* target_node = (PNode*) get_node(input->s_indirect[pnode_index]);
      int pnode_pointer_index = block_index % NODE_DATA;
      if(DEBUG) printf("%d -> s_indirect: %d \n", index, pnode_index);
      DiskBlock* current_block = get_diskblock(target_node->direct[pnode_pointer_index]);
      if(DEBUG) printf("%d -> direct: %d \n", index, pnode_pointer_index);
      if(DEBUG) printf("%d -> diskblock index: %d \n", index, pnode_pointer_index);
      diskblocks[index] = current_block;
      index++;
      continue;
    }

    // double indirect
    if (index >= INODE_DIR + INODE_IND * NODE_DATA && index < INODE_DIR + INODE_IND * NODE_DATA + INODE_DIND * NODE_DATA * NODE_DATA){
      
      int block_index = index - INODE_DIR - INODE_IND*NODE_DATA;
      int dpnode_index = floor(block_index / (NODE_DATA * NODE_DATA));
      
      PNode* target_ind = (PNode*) get_node(input->d_indirect[dpnode_index]); 
      int pnode_index = floor((block_index - NODE_DATA*NODE_DATA*dpnode_index)/NODE_DATA); 

      PNode* target_node = (PNode*) get_node(target_ind->direct[pnode_index]);
      int pnode_pointer_index = block_index % (NODE_DATA * NODE_DATA) % NODE_DATA; // 5
      diskblocks[index] = get_diskblock(target_node->direct[pnode_pointer_index]); 
      index++;
      printf("gb: dpnode: %d, pnode: %d, pnode_point: %d\n", dpnode_index, pnode_index, pnode_pointer_index); 
      if(DEBUG) printf("%d -> %d \n", index, target_node->direct[pnode_pointer_index]);
      continue;
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
  for (int i = 0; i<BITMAP_SIZE*8; i++) {
    if (!get_bit(i)) {
      return i;
    }
  }
  errno = ENOSPC; 
  return -1; // this is if we cannot find a free diskblock
}

int first_free_node() {
  if(DEBUG) printf("getting the first node \n");
  for (int i = 0; i<NODES; i++) {
    if (loaded_disk->nodes[i].type == 'u'){
      return i; 
    }
  }
  errno = ENOMEM;
  return -1; // this is if we cannot find a free node
}

void init_inode_indirects(INode * node){
  for(int i = 0; i < INODE_IND; i++){
    node->s_indirect[i] = -1; 
  }
  for(int i = 0; i < INODE_DIND; i++){
    node->d_indirect[i] = -1; 
  }
}

//need this for double indirects
void init_pnode(PNode * node){
  for(int i = 0; i < NODE_DATA; i++){
    node->direct[i] = -1; 
  }
}


void print_disk_indices(INode* node) {
  for (int i=0;i < INODE_DIR; i++) {
    printf("index: %d \n", node->direct[i]);
  }
  for (int i=0;i < NODE_DATA; i++) {
    print_pnode(*(PNode*) get_node(node->s_indirect[i]));
  } 
}


int wo_open(char* file_name, int flags, ...) {

  int free_index = -1;
  for (int i=0; i<NODES; i++) {
    // keep track of the first free INode in case we don't find the file
    if(DEBUG) printf("node #: %d\n", i); 
    if (free_index == -1 && loaded_disk->nodes[i].type == 'u') { 
      free_index = i;
      continue;
    }

    // if it is not an INode, just ignore
    if (loaded_disk->nodes[i].type != 'i') { continue; }
    INode* curr_node = &loaded_disk->nodes[i];
    if (strcmp(curr_node->name, file_name) == 0) {
      if(DEBUG) print_inode(*curr_node);
      curr_node->fd = file_count; 
      curr_node->mode = flags;
      file_count++; 
      open_files[curr_node->fd] = curr_node;
      return curr_node->fd; 
    }
  }

  va_list ap; 
  va_start(ap, flags); //flags is the last known argument
  int mode = va_arg(ap, int); // result is undefined if the third argument does not exist
  va_end(ap); 
  if(mode == WO_CREATE){ 
    // creating file
    INode* allocating = (Node*) &loaded_disk->nodes[free_index];
    if(DEBUG) printf("Creating file at address %p and index %d \n", allocating, free_index);
    allocating->type = 'i';
    allocating->blocks = 1;
    int first_free = first_free_diskblock();
    if(DEBUG) printf("free diskblock at index %d \n", first_free);
    if(DEBUG) print_disk_block(*get_diskblock(first_free));
    set_bit(first_free);
    allocating->direct[0] = first_free;
    init_inode_indirects(allocating); 
    
    allocating->bytes = 0; // this is so we know where to start writing in the blocks
    strcpy(allocating->name, file_name);

    allocating->fd = file_count; 
    allocating->mode = flags;
    file_count++; 
    open_files[allocating->fd] = allocating;
    return allocating->fd; 
  }

}


int wo_read( int fd,  void* buffer, int bytes){
  INode* target = open_files[fd];
  if(open_files[fd] == NULL){
    errno = ENOENT; 
    if(DEBUG) printf("fd is null! \n");
    return -1;
  }
  if(target->mode == WO_WRONLY){ //we can only write in this case, can't read
    if(DEBUG) printf("you don't have permission! \n");
    errno = EPERM; 
    return -1;
  }
  if(DEBUG) printf("reading the following file \n");

  DiskBlock** diskblocks = get_diskblocks(target);

  int bytes_read = 0;
  int block_index = 0;
  // TODO: consider the case where the most recent disk block is not full
  if(DEBUG) printf("starting at block %d \n", block_index);
  while (bytes_read < bytes) {
    int bytes_to_read = bytes - bytes_read;
    int read_size = bytes_to_read > BLOCK_QUANTA ? BLOCK_QUANTA : bytes_to_read;
    memcpy(buffer + bytes_read, diskblocks[block_index], read_size);

    bytes_read += read_size;
    block_index ++;
  }
  if(DEBUG) printf("finished reading the file \n");
  return 0;
}

// for some reason on the ilab, it wasn't picking up ceil from math.h on make
double ceil(double input){
  return (int)input + 1; 
}


int wo_write(int fd,  void* buffer, int bytes){
  if(open_files[fd] == NULL){
    //set errno!!!
    if(DEBUG) printf("fd is null! \n");
    errno = ENOENT; 
    return -1;
  }
  INode* target = open_files[fd];
  if(target->mode == WO_RDONLY){ //we can only read in this case, can't read
    //set errno!!!
    if(DEBUG) printf("you don't have permission! \n");
    errno = EPERM; 
    return -1;
  }

  // write to diskblocks in chunks of 1024
  double total_bytes = (double) bytes + (double) target->bytes;
  int blocks_needed = (int) ceil( total_bytes / (double) BLOCK_QUANTA);
  if(DEBUG) printf("\nwe need %d blocks \n\n", blocks_needed);

  
  if (blocks_needed > target->blocks) {
    // we need to allocate more blocks
    int s_indirect_index = 0;
    int d_indirect_index = 0;
    int allocated_blocks = target->blocks; 
    while (allocated_blocks < blocks_needed) {
      int index_newblock = first_free_diskblock();
      if(index_newblock == -1){
        if(DEBUG) printf("out of space"); 
        errno = ENOSPC; 
        return -1; 
      }
      if(DEBUG) printf("allocating diskblock: %d\n", index_newblock);
      set_bit(index_newblock);
      if (allocated_blocks < INODE_DIR) {
        target->direct[allocated_blocks] = index_newblock;
      } else if (allocated_blocks >= INODE_DIR && allocated_blocks < INODE_DIR + INODE_IND * NODE_DATA) {

        // assign the first free pointer in this pnode to be the block
        int block_index = allocated_blocks - INODE_DIR; 
        int pnode_index = floor(block_index / NODE_DATA);

        if(target->s_indirect[pnode_index] == -1){
          int new_ind = first_free_node(); 
          PNode* new = (PNode*)(&loaded_disk->nodes[new_ind]); 
          target->s_indirect[pnode_index] = new_ind;  
          new->type = 'p'; 
        }

        if(DEBUG) printf("%d -> %d \n", allocated_blocks, index_newblock);
        if(DEBUG) printf("sind index %d \n", pnode_index);
        PNode* target_node = (PNode*) get_node(target->s_indirect[pnode_index]);
        int pnode_pointer_index = block_index % NODE_DATA; // 5
        if(DEBUG) printf("block index %d \n", pnode_pointer_index);
        target_node->direct[pnode_pointer_index] = index_newblock;
        
      } else {
        //start using d_indirect
        int block_index = allocated_blocks - INODE_DIR - INODE_IND*NODE_DATA;
        int dpnode_index = floor(block_index / (NODE_DATA * NODE_DATA));
        
        if(target->d_indirect[dpnode_index] == -1){
          int new_ind = first_free_node(); 
          PNode* new = (PNode*)(&loaded_disk->nodes[new_ind]); 
          init_pnode(new);
          target->d_indirect[dpnode_index] = new_ind;  
          new->type = 'd'; 
        }
        PNode* target_ind = (PNode*) get_node(target->d_indirect[dpnode_index]); 
        int pnode_index = floor((block_index - NODE_DATA*NODE_DATA*dpnode_index)/NODE_DATA); 

        if(target_ind->direct[pnode_index] == -1){
          int new_ind = first_free_node(); 
          PNode* new = (PNode*)(&loaded_disk->nodes[new_ind]); 
          target_ind->direct[pnode_index] = new_ind; 
          new->type = 'p'; 
        }

        if(DEBUG) printf("%d -> %d \n", allocated_blocks, index_newblock);
        if(DEBUG) printf("dind index %d \n", dpnode_index);
        if(DEBUG) printf("sind index %d \n", pnode_index);
        if(DEBUG) printf("target ind -> dir[pnode_index] %d \n", target_ind->direct[pnode_index]); 
        if(DEBUG) printf("target ind address: %p\n", target_ind); 
        PNode* target_node = (PNode*) get_node(target_ind->direct[pnode_index]);
        int pnode_pointer_index = block_index % (NODE_DATA * NODE_DATA) % NODE_DATA; // 5
        if(DEBUG) printf("block index %d \n", pnode_pointer_index);
        if(DEBUG) printf("pnode pointer index: %d, newblock: %d\n", pnode_pointer_index, index_newblock); 
        if(DEBUG) printf("disk: %p, target:%p, target node: %p\n", loaded_disk, &target_node->direct[pnode_pointer_index], target_node); 
        target_node->direct[pnode_pointer_index] = index_newblock;
        
      }

      allocated_blocks++;
    }
    target->blocks = blocks_needed;
  }


  // get the list of the disk blocks
  DiskBlock** diskblocks = get_diskblocks(target);


  // only write the given amount of bytes
  int bytes_written = 0;
  int block_index = floor( (double) (target->bytes) / BLOCK_QUANTA);
  while (bytes_written < bytes) {
    size_t block_offset = bytes_written == 0 ? (size_t) target->bytes % BLOCK_QUANTA : 0;
    int bytes_to_write = bytes - bytes_written;
    int write_size = bytes_to_write > BLOCK_QUANTA ? BLOCK_QUANTA - block_offset : bytes_to_write - block_offset;
    if(1) printf("offset: %d\t\tbytes to write: %d\t\twrite size: %d\t\tblock index: %d\t\tmemory location: %p \n", block_offset, bytes_to_write, write_size, block_index, diskblocks[block_index]);
    // diskblocks[block_index] = diskblocks + sizeof(DiskBlocks) * block_index
    if(1) printf("%p + %d = %p \n", diskblocks[block_index], block_offset,  ((char*) diskblocks[block_index]) + block_offset); // TODO: make this offset correct
    memcpy(((char*) diskblocks[block_index]) + block_offset, buffer + bytes_written, write_size);

    bytes_written += write_size;
    block_index ++;
  }

  // update the size of the file
  target->bytes += bytes;

  return 0; 
}


int wo_close(int fd){
  if(open_files[fd]!= NULL){
    open_files[fd] == NULL; 
    return 0;
  }
  errno = ENOENT; 
  return -1; 
}

void check_size() {
  // these three should be the same
  printf("INode: %d \n", sizeof(INode));
  printf("PNode: %d \n", sizeof(PNode));
  printf("Node: %d \n", sizeof(Node));
  // This should be 0
  printf("Size difference: %d \n", DISK_SIZE - sizeof(Disk));
  printf("blocks: %d\n", BLOCKS); 
}

int main(int argc, char** args) {
  
  // run below to check metadata changes make sure to run this
  // check_size(); return 0;

  Disk* disk = malloc(sizeof(Disk));
  printf("size %d\n", sizeof(char) * BLOCK_QUANTA * INODE_IND * NODE_DATA); 
  int mount_result = wo_mount("test_disk1", disk);
  int open_result = wo_open("test_file1", WO_RDWR,  WO_CREATE);
  const int message_size = sizeof(char) * BLOCK_QUANTA * INODE_IND * NODE_DATA;
  char* message1 = malloc(message_size);
  char* message2 = malloc(message_size);
  char* message3 = malloc(message_size);

  memset(message1, 'p', message_size);
  memset(message2, 'm', message_size);
  memset(message3, 't', message_size);

  char* read_buffer = malloc(message_size*2 + 1);
  printf("buffer address %p \n", read_buffer);
  int writes[3];
  writes[0] = wo_write(open_result, message1, message_size);
  writes[1] = wo_write(open_result, message2, message_size);
  // writes[2] = wo_write(open_result, message3, message_size);
  
  printf("try to read: %d\n", wo_read(open_result, read_buffer, message_size)); 
  int unmount_result = wo_unmount(disk);

  return 0;
}