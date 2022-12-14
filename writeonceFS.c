#include "writeonceFS.h"
#include <stdio.h>
#include <errno.h> //apparently we have to set this every time we return an error
#include <stdarg.h>

#define DEBUG 1

static FILE* disk_file;
static INode* curr_file;
static Disk* loaded_disk;
INode* open_files [50]; //index is the file descriptor, contains a pointer to the INode
INode* open_modes [50]; //track the mode of every file

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

// populate DiskBlock with PNode, if the PNode is of type 'b' (block)
void load_pnode(PNode* input, DiskBlock** result, int* index, int blocks) {
  if (input->type == 'b') {
    int local_blocks = 0;
    while (*index < blocks && local_blocks < 33) {
      result[*index] = input->direct[*index];
      *index = (*index) + 1;
      local_blocks++;
    }
    return;
  } else if (input->type == 'p') {
    return;
  }
  
}

// return the diskblock at the given index
DiskBlock* get_diskblock(int index) {
  return &loaded_disk->blocks[index];
}

// return the blocks of an iNode
DiskBlock** get_diskblocks(INode* input) {
  print_inode(*input);
  int blocks = input->blocks;
  int index = 0;
  DiskBlock** result = (DiskBlock*) malloc(sizeof(DiskBlock) * blocks);
  while (index < blocks) {
    // direct
    if (index < INODE_DIR) { 
      if (!input || !input->direct || input->direct[index] != NULL) {
        printf("the disk block is null \n");
        return NULL;
      }
      printf("%d \n", input->direct[index]);
      DiskBlock* sdf;
      print_disk_block(*get_diskblock(input->direct[index]));
      result[index] = input->direct[index];
      index++;
      // if(DEBUG) printf("%s \n", input->direct[index]);
      continue;
    }
    return result;
    // single indirect
    for (int i = 0; i<INODE_IND && index < blocks; i++) {
      // we need to load in 33 blocks per PNode
      load_pnode(input->s_indirect[i], result, &index, blocks);
      continue;
    }

    // double indirect
    for (int i = 0; i < INODE_DIND && index < blocks; i++){
      //

    }
  }
  return result;
}

int get_bit(int input, int index) {
    int k = 1 << index;
    return (input & k == 0) ? 0 : 1;
}

int set_bit(int* input, int index) {
  *input |= 1 << index;
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
      curr_file = curr_node;
      DiskBlock** blocks = get_diskblocks(curr_file);
      if(DEBUG){
        printf("loaded the disk blocks \n");
        for (int i=0; i<curr_file->blocks; i++) {
          print_disk_block(*blocks[i]);
        }
      }
      curr_file->fd = file_count; 
      file_count++; 
      open_files[curr_file->fd] = curr_file;
      open_modes[curr_file->fd] = flags; // should we check if flag is correct type?
      return curr_file->fd; 
      //return 0; //why were there two return 0s here??? one was above
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
    set_bit(&loaded_disk->sb.bitmap, first_free);
    allocating->direct[0] = first_free;
    strcpy(get_diskblock(first_free), "hello"); //this is a test
    strcpy(allocating->name, file_name);

    //need to check this
    curr_file = allocating; 
    curr_file->fd = file_count; 
    file_count++; 
    open_files[curr_file->fd] = curr_file;
    open_modes[curr_file->fd] = flags; // should we check if flag is correct type?
    return curr_file->fd; 
    // return 0;
  }

}

//are we supposed to somehow update the position in the file
//that we are reading from?? because it doesn't say that
//so can we assume that we always read from the start?
int wo_read( int fd,  void* buffer, int bytes){
  if(open_files[fd] == NULL){
    //set errno!!!
    return -1;
  }
  if(open_modes[fd] == WO_WRONLY){ //we can only write in this case, can't read
    //set errno!!!
    return -1;
  }

  //not sure this is right
  DiskBlock** blocks = get_diskblocks(open_files[fd]);

  return 0;
}



int wo_write(int fd,  void* buffer, int bytes){
  if(open_files[fd] == NULL){
    //set errno!!!
    return -1;
  }
  if(open_modes[fd] == WO_RDONLY){ //we can only read in this case, can't read
    //set errno!!!
    return -1;
  }
  return 0; 
}


int wo_close(int fd){
  if(open_files[fd]!= NULL){
    open_files[fd] == NULL; 
    return 0;
  }
  return -1; 
}

int main(int argc, char** args) {
  Disk* disk = malloc(sizeof(Disk));
  int mount_result = wo_mount("test_disk1", disk);
  int open_result = wo_open("test_file1", 0,  WO_CREATE);
  // int open_result2 = wo_open("test_file1", 0, 0);
  int unmount_result = wo_unmount(disk);

  return 0;
}