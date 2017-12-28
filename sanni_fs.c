/*

  File system created by 
  ---------------------------------------------------------------------------------------

  Name 		: Santhosh Kumar Rajendran
  N-number      : N17824779
  net-id 	: skr334

  For OS Assignment under Professor Katz !!

  LIMITATIONS :
  ---------------------------------------------------------------------------------------
  1. Files can have content of < 4096 bytes only. (i.e. indirect pointer not implemented)
  2. Files created and edited using "touch", "echo" and "cat" commands only. Doesn't work with "vi". Havnen't figured out why yet.. :(
  
  HOW TO : 
  ---------------------------------------------------------------------------------------
  To compile : gcc -Wall sanni_fs.c -o sanni_fs `pkg-config fuse --cflags --libs`
  To mount   : ./sanni_fs <folder_name>       <<<<<<<< Folder should already be present
  To unmount : fusermount -u <folder_name>

*/

// FUSE version
#define FUSE_USE_VERSION 26

//#include <fcntl.h>
#include <errno.h>
#include <fuse.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

/**************************************************************************/

// Global constants

// Filesystem details
#define BLOCK_SIZE 4096
#define MAX_NO_BLOCKS 10000

// Design Details
#define BLOCK_PREFIX "/fusedata/fusedata."
#define SUPER_BLOCK "/fusedata/fusedata.0"
#define ROOT_BLOCK "/fusedata/fusedata.26"
#define FREE_LIST_START "/fusedata/fusedata.1"
#define FREE_START 1
#define FREE_END 25
#define ROOT_NODE 26
#define MAX_BLOCKS 100
#define LOOKUP_PATH "/fusedata/lookup_table"
#define MAX_INODES 20

/**************************************************************************/

// Defining the datastructures

struct filename_to_inode_dict {
	char type;
	char name[20];
	int block_no;
};

struct inode_table {
	int size;
	int uuid;
	int gid;
	int mode;
	int atime;
	int ctime;
	int mtime;
	int linkcount;
	struct filename_to_inode_dict fidict[MAX_INODES];
};

struct file_inode {
	int size;
	int uid;
	int gid;
	int mode;
	int atime;
	int ctime;
	int mtime;
	int indirect;
	int location;
	int linkcount;
};

static struct superblock {
	int creationTime;
	int mounted;
	int devId;
	int freeStart;
	int freeEnd;
	int root;
	int maxBlocks;
	int lookup_entries;
} *super_block;

// Structure to store the path to blockno mapping for easy lookup

static struct path_to_fileno {
	char path[30];
	int fileno;
} lookup[15];


/**************************************************************************/

// Create 10000 files and write zeroes to them all

int create_fusedata_files() {

	FILE *fptr;

	char cptr[50];
	int cnt;
	for (cnt = 0; cnt < MAX_BLOCKS; 	cnt++){	
		sprintf(cptr, "%s%d", BLOCK_PREFIX, cnt);
		fptr = fopen(cptr,"w");
		fprintf(fptr, "%04096d", 0);
		fclose(fptr);
	}

	return 0;
}


// Initializing the fusedata.1 -> fusedata.25 files to hold values from 27 to 9999

int init_free_blocks() {

	FILE *fptr;

	char cptr[50];
	int i = 1;

	sprintf(cptr, "%s%d", BLOCK_PREFIX, i);
	fptr = fopen(cptr,"w");
	int block_no;
	for (block_no = (ROOT_NODE+1); block_no < 400; block_no++)
		fprintf(fptr, "%d ", block_no);
	fclose(fptr);

	for (i = 2; i < ROOT_NODE; i++){	
		sprintf(cptr, "%s%d", BLOCK_PREFIX, i);
		fptr = fopen(cptr,"w");

		for (block_no = 400*(i-1); block_no < 400*i ; block_no++)
			fprintf(fptr, "%d ", block_no);
		fclose(fptr);
	}

	return 0;
}

// Method to get a free block from the free-block-list
int get_free_block() {

	FILE *fptr;

	char *free_list = malloc(2000*sizeof(char));
	int free_block_no;

	fptr = fopen(FREE_LIST_START,"r");
	fscanf(fptr, "%d " , &free_block_no);
	fgets(free_list, 2000, fptr);
	fclose(fptr);

	fptr = fopen(FREE_LIST_START,"w");
	fprintf(fptr, "%s\n", free_list);
	fclose(fptr);

	free(free_list);

	return free_block_no;
}

// Method to return a free block to the free-block-list
void returnFreeBlock( int freeBlock ) {

	FILE *fptr;
	char freeBlocks[4000];
	char newFreeBlocks[4000];

	fptr = fopen(FREE_LIST_START,"r");
	fgets(freeBlocks, 4000, fptr);
	fclose(fptr);

	sprintf(newFreeBlocks, "%d %s", freeBlock, freeBlocks);

	fptr = fopen(FREE_LIST_START,"w");
	fprintf(fptr, "%s", newFreeBlocks);
	fclose(fptr);
}

// Create a superblock struct and store it to fusedata.0
int init_super_block() {

	FILE *fptr;

	// Getting the current time
	time_t rawtime = time(NULL);
	int time_now = (unsigned)rawtime;

	// Initializing the superblock
	super_block = malloc(sizeof(struct superblock));
	super_block->creationTime = time_now;
	super_block->mounted = 0;
	super_block->devId = 20;
	super_block->freeStart = FREE_START;
	super_block->freeEnd = FREE_END;
	super_block->root = ROOT_NODE;
	super_block->maxBlocks = MAX_BLOCKS;
	super_block->lookup_entries = 0;

	// Writing the superblock to the zero'th file
	fptr = fopen(SUPER_BLOCK,"w");
	fwrite(super_block, sizeof(struct superblock), 1 , fptr);
	fclose(fptr);

	return 0;
}

// Updates the Root node/block and writes it the appropriate file
int init_root_block() {

	FILE *fptr;

	// Getting the current time
	time_t rawtime = time(NULL);
	int time_now = (unsigned)rawtime;

	// Initializing the Root Node
	struct inode_table *root_block = malloc(sizeof(struct inode_table));
	root_block->size 		= 4096;
	root_block->uuid 		= 1;
	root_block->gid 		= 1;
	root_block->mode 		= 16877;
	root_block->atime 		= time_now;
	root_block->ctime 		= time_now;
	root_block->mtime 		= time_now;
	root_block->linkcount 	= 2;
	root_block->fidict[0].type		= 'd';
	strcpy(root_block->fidict[0].name,".");
	root_block->fidict[0].block_no	= ROOT_NODE;
	root_block->fidict[1].type		= 'd';
	strcpy(root_block->fidict[1].name,"..");
	root_block->fidict[1].block_no	= ROOT_NODE;

	// Adding entry to lookup table
	strcpy(lookup[0].path,"/");
	lookup[0].fileno = 26;

	// Writing it to lookup file
	fptr = fopen( LOOKUP_PATH , "w");
	fwrite(&lookup , sizeof(lookup), 1 , fptr);
	fclose(fptr);

	// Writing Root Node information to Root Block
	fptr = fopen( ROOT_BLOCK , "w" );
	fwrite(root_block, sizeof(*root_block), 1 , fptr);
	fclose(fptr);

	// Free up the memory used
	free(root_block);

	return 0;
}

// Loads the data from the fusedata.0 file onto the super_block
int load_super_block(){
	
	FILE *fptr;

	fptr = fopen( SUPER_BLOCK , "r");
	super_block = malloc(sizeof(struct superblock));
	fread( super_block , sizeof(super_block) , 1 , fptr);
	super_block->mounted++;
	fclose(fptr);

	printf( "The value of mounted now is : %d\n" , super_block->mounted );
	printf( "The value of lookup_entries now is : %d\n" , super_block->lookup_entries );

	return 0;
}

// Loads the data from the quick lookup table onto the lookup structure
int load_lookup_table() {

	FILE *fptr;

	fptr = fopen( LOOKUP_PATH , "r");
	fread(&lookup , sizeof(lookup), 1 , fptr);
	fclose(fptr);

	return 0;
}



//****************** FUSE FUNCTIONS **********************//



void *sanni_init (struct fuse_conn_info *finfo) {

	(void) finfo;

	FILE *fptr;

	if ( (fptr = fopen(SUPER_BLOCK,"r")) ){
		fclose(fptr);
		
		load_super_block();			// Load the superblock from the fusedata.0 file
		load_lookup_table();		// Load the lookup table from the lookup_table file

		return 0;
	}
	else{
		create_fusedata_files();	// Create fusedata files
		init_free_blocks();			// Initialize the free data blocks
		init_super_block();			// Create and store the super block
		init_free_blocks();			// Initialize the free blocks list
		init_root_block();			// Create and store the root block
	}

	return 0;
}

int sanni_getattr(const char *path, struct stat *statbuf) {

	int ret_val = 0;
	
	FILE *fptr;
	int block_no = 0;

	int cnt = 0;
	while(lookup[cnt].fileno != 0){
		if ( strcmp(lookup[cnt].path , path) == 0 ){
			block_no = lookup[cnt].fileno;
			break;
		}
		cnt++;
	}

	if ( block_no == 0 )
		return -ENOENT;
	
	char fuse_file[50];
	sprintf(fuse_file, "%s%d", BLOCK_PREFIX, block_no);

	struct inode_table *temp_node = malloc(sizeof(struct inode_table));
	fptr = fopen( fuse_file, "r" );
	fread(temp_node, sizeof(struct inode_table), 1, fptr);
	fclose(fptr);

	memset(statbuf, 0, sizeof(struct stat));
	statbuf->st_mode 	= temp_node->mode;
	statbuf->st_nlink 	= temp_node->linkcount;
	statbuf->st_size 	= temp_node->size;
	statbuf->st_atime 	= temp_node->atime;
    statbuf->st_mtime 	= temp_node->ctime;
    statbuf->st_ctime 	= temp_node->mtime;

	// Free up the memory used
	free(temp_node);

	return ret_val;
}

int sanni_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi){

	int ret_val = 0;
	
	int block_no;
	FILE *fptr;


	(void) offset;
	(void) fi;

	int cnt = 0;
	while(lookup[cnt].fileno != 0){
		if ( strcmp(lookup[cnt].path , path) == 0 ){
			block_no = lookup[cnt].fileno;
			break;
		}
		cnt++;
	}

	if ( block_no == 0 )
		return -ENOENT;
	
	char fuse_file[50];
	sprintf(fuse_file, "%s%d", BLOCK_PREFIX, block_no);

	struct inode_table *temp_node = malloc(sizeof(struct inode_table));
	fptr = fopen( fuse_file, "r" );
	fread(temp_node, sizeof(struct inode_table), 1, fptr);
	fclose(fptr);

	cnt = 0;
	for ( cnt = 0 ; cnt < temp_node->linkcount ; cnt++){
		if (cnt > 1) {
			char temp[20];
			int i = 0 , pos;
			while ( temp_node->fidict[cnt].name[i] ) {
				if( temp_node->fidict[cnt].name[i] == '/' )
					pos = i;
				i++;
			}
			strcpy( temp , &temp_node->fidict[cnt].name[pos+1] );
			filler(buf, temp , NULL, 0);
		}
		else {
			filler(buf, temp_node->fidict[cnt].name, NULL, 0);
		}
	}

	// Free up the memory used
	free(temp_node);

	return ret_val;
}

int sanni_mkdir(const char *path, mode_t mode){
	int ret_val = 0;
	(void) mode;

	// Get current time
	time_t rawtime = time(NULL);
	int time_now = (unsigned)rawtime;

	int free_block_no;
	FILE *fptr;

	// Get a free block from the free block list
	free_block_no = get_free_block();

	struct inode_table *new_node = malloc(sizeof(struct inode_table));
	new_node->size 		= 4096;
	new_node->uuid 		= 1;
	new_node->gid 		= 1;
	new_node->mode 		= 16877;
	new_node->atime 	= time_now;
	new_node->ctime 	= time_now;
	new_node->mtime 	= time_now;
	new_node->linkcount = 2;
	new_node->fidict[0].type		= 'd';
	strcpy(new_node->fidict[0].name,".");
	new_node->fidict[0].block_no	= free_block_no;
	new_node->fidict[1].type		= 'd';
	strcpy(new_node->fidict[1].name,"..");
	new_node->fidict[1].block_no	= ROOT_NODE;

	// Writing to the appropriate fusedata file
	char fuse_file[50];
	sprintf(fuse_file, "%s%d", BLOCK_PREFIX, free_block_no);

	fptr = fopen( fuse_file , "w" );
	fwrite(new_node, sizeof(*new_node), 1 , fptr);
	fclose(fptr);


	// Updating the Lookup table
	super_block->lookup_entries++;
	strcpy(lookup[super_block->lookup_entries].path,path);
	lookup[super_block->lookup_entries].fileno = free_block_no;

	// Getting the parent folder
	char parent_path[20];

	int cnt = 0 , pos;
	while ( path[cnt] ) {
		if( path[cnt] == '/' )
			pos = cnt;
		cnt++;
	}
	strncpy( parent_path , path , pos );
	parent_path[pos] = '\0';

	// Getting the parent folder file number
	int p_node;
	if ( pos == 0 )
		p_node = ROOT_NODE;
	else {
		cnt = 0;
		while(lookup[cnt].fileno != 0){
			if ( strcmp(lookup[cnt].path , parent_path) == 0 ){
				p_node = lookup[cnt].fileno;
				break;
			}
			cnt++;
		}	
	}

	//
	// Reading and Updating the parent folder's fusedata file
	//

	sprintf(fuse_file, "%s%d", BLOCK_PREFIX, p_node);
	fptr = fopen( fuse_file , "r" );
	
	struct inode_table *parent_node = malloc(sizeof(struct inode_table));
	fread(parent_node, sizeof(struct inode_table), 1 , fptr);
	fclose(fptr);

	int new_entry = parent_node->linkcount;
	parent_node->fidict[new_entry].type = 'd';
	strcpy(parent_node->fidict[new_entry].name,path);
	parent_node->fidict[new_entry].block_no = free_block_no;
	parent_node->linkcount++;

	// Writing to the appropriate fusedata file

	fptr = fopen( fuse_file , "w" );
	fwrite(parent_node, sizeof(*parent_node), 1 , fptr);
	fclose(fptr);

	// Free memory 
	free(new_node);
	free(parent_node);

	return ret_val;
}

int sanni_rename(const char *path, const char *newpath) {

	int ret_val = 0;
	FILE *fptr;

	// find the block number for parent folder
	char parent_path[20];

	int cnt = 0 , pos;
	while ( path[cnt] ) {
		if( path[cnt] == '/' )
			pos = cnt;
		cnt++;
	}
	strncpy( parent_path , path , pos );
	parent_path[pos] = '\0';

	// Getting the parent folder file number
	int p_node;
	if ( pos == 0 )
		p_node = ROOT_NODE;
	else {
		cnt = 0;
		while(lookup[cnt].fileno != 0){
			if ( strcmp(lookup[cnt].path , parent_path) == 0 ){
				p_node = lookup[cnt].fileno;
				break;
			}
			cnt++;
		}	
	}

	// Updating the parent file_to_inode_dict
	char fuse_file[50];
	sprintf(fuse_file, "%s%d", BLOCK_PREFIX, p_node);

	struct inode_table *temp_node = malloc(sizeof(struct inode_table));
	fptr = fopen( fuse_file, "r" );
	fread(temp_node, sizeof(struct inode_table), 1, fptr);
	fclose(fptr);

	cnt = 0;
	while ( temp_node->fidict[cnt].name ) {
		if( (strcmp( temp_node->fidict[cnt].name , path ) == 0) ){
			strcpy( temp_node->fidict[cnt].name , newpath );
			break;
		}
		cnt++;
	}

	fptr = fopen( fuse_file , "w" );
	fwrite(temp_node, sizeof(*temp_node), 1 , fptr);
	fclose(fptr);

	// Updating the lookup table
	cnt = 0;
	while(lookup[cnt].fileno != 0){
		if ( strcmp(lookup[cnt].path , path) == 0 ){
			strcpy( lookup[cnt].path , newpath );
			break;
		}
		cnt++;
	}

	// Free the temp node created
	free(temp_node);

	return ret_val;
}

int sanni_create(const char *path, mode_t mode, struct fuse_file_info *fi) {

	int ret_val = 0;

	(void) mode;

	// Get current time
	time_t rawtime = time(NULL);
	int time_now = (unsigned)rawtime;

	int free_block_no_1 , free_block_no_2;
	FILE *fptr;

	// Get a free block from the free block list
	free_block_no_1 = get_free_block();
	free_block_no_2 = get_free_block();

	struct file_inode *new_node = malloc(sizeof(struct file_inode));
	new_node->size = 0;
	new_node->uid = 1;
	new_node->gid = 1;
	new_node->mode = 33261;
	new_node->linkcount = 1;
	new_node->atime = time_now;
	new_node->ctime = time_now;
	new_node->mtime = time_now;
	new_node->indirect = 0;
	new_node->location = free_block_no_2;

	// Writing to the appropriate fusedata file
	char fuse_file[50];
	sprintf(fuse_file, "%s%d", BLOCK_PREFIX, free_block_no_1);

	fptr = fopen( fuse_file , "w" );
	fwrite(new_node, sizeof(*new_node), 1 , fptr);
	fclose(fptr);

	// Updating the Lookup table
	super_block->lookup_entries++;
	strcpy(lookup[super_block->lookup_entries].path,path);
	lookup[super_block->lookup_entries].fileno = free_block_no_1;

	// Getting the parent folder
	char parent_path[20];

	int cnt = 0 , pos;
	while ( path[cnt] ) {
		if( path[cnt] == '/' )
			pos = cnt;
		cnt++;
	}
	strncpy( parent_path , path , pos );
	parent_path[pos] = '\0';

	// Getting the parent folder file number
	int p_node;
	if ( pos == 0 )
		p_node = ROOT_NODE;
	else {
		cnt = 0;
		while(lookup[cnt].fileno != 0){
			if ( strcmp(lookup[cnt].path , parent_path) == 0 ){
				p_node = lookup[cnt].fileno;
				break;
			}
			cnt++;
		}	
	}

	//
	// Reading and Updating the parent folder's fusedata file
	//

	sprintf(fuse_file, "%s%d", BLOCK_PREFIX, p_node);
	fptr = fopen( fuse_file , "r" );
	
	struct inode_table *parent_node = malloc(sizeof(struct inode_table));
	fread(parent_node, sizeof(struct inode_table), 1 , fptr);
	fclose(fptr);

	int new_entry = parent_node->linkcount;
	parent_node->fidict[new_entry].type = 'f';
	strcpy(parent_node->fidict[new_entry].name,path);
	parent_node->fidict[new_entry].block_no = free_block_no_1;
	parent_node->linkcount++;

	// Writing to the appropriate fusedata file

	fptr = fopen( fuse_file , "w" );
	fwrite(parent_node, sizeof(*parent_node), 1 , fptr);
	fclose(fptr);

	// Get file handler for the actual file
	sprintf(fuse_file, "%s%d", BLOCK_PREFIX, free_block_no_2);
	fptr = fopen( fuse_file , "w" );
	fi->fh = (uint64_t)fptr;

	// Free the memory allocated
	free(new_node);
	free(parent_node);

	return ret_val;
}

int sanni_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {

	int ret_val = 0;
	FILE *fptr = (FILE*)fi->fh;

	//ret_val = fwrite(buf ,sizeof(char) , size , fptr);
	ret_val = fprintf(fptr, "%s", buf);
	fclose(fptr);

	//
	// Update the size in the inode file
	//

	int cnt = 0, node;
	while(lookup[cnt].fileno != 0){
		if ( strcmp(lookup[cnt].path , path) == 0 ){
			node = lookup[cnt].fileno;
			break;
		}
		cnt++;
	}

	char fuse_file[50];
	sprintf(fuse_file, "%s%d", BLOCK_PREFIX, node);

	struct file_inode *new_node = malloc(sizeof(struct file_inode));

	fptr = fopen( fuse_file,"r" );
	fread( new_node, sizeof(*new_node), 1 , fptr );
	fclose(fptr);

	new_node->size += sizeof(char) * size;

	fopen( fuse_file,"w" );
	fwrite( new_node, sizeof(*new_node), 1 , fptr );
	fclose(fptr);

	free(new_node);

	return ret_val;
}

int sanni_open(const char *path, struct fuse_file_info *fi) {

	int ret_val = 0;
	FILE *fptr;

	// Finding the file inode
	int cnt = 0, node;
	while(lookup[cnt].fileno != 0){
		if ( strcmp(lookup[cnt].path , path) == 0 ){
			node = lookup[cnt].fileno;
			break;
		}
		cnt++;
	}

	char fuse_file[50];
	sprintf(fuse_file, "%s%d", BLOCK_PREFIX, node);

	struct file_inode *file_node = malloc(sizeof(struct file_inode));

	// Finding the location of the file from the inode 
	fptr = fopen( fuse_file,"r" );
	fread( file_node, sizeof(*file_node), 1 , fptr );
	fclose(fptr);

	node = file_node->location;

	// Open file handler to the actual file 
	sprintf(fuse_file, "%s%d", BLOCK_PREFIX, node);

	fptr = fopen(fuse_file,"a+");
	fi->fh = (uint64_t)fptr;

	free(file_node);

	return ret_val;
}

int sanni_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
	
	int ret_val = 0;
	FILE *fptr = (FILE*) fi->fh;

	// Getting the data from the file and updating the buffer with the content
	// Return the number of characters read
	ret_val = fread( buf, sizeof(char), size, fptr);
	fclose(fptr);

	return ret_val;
}

int sanni_unlink(const char *path) {
	
	int ret_val = 0;
	FILE *fptr;

	// Finding the file inode
	int cnt = 0, node;
	while(lookup[cnt].fileno != 0){
		if ( strcmp(lookup[cnt].path , path) == 0 ){
			node = lookup[cnt].fileno;
			lookup[cnt].fileno = 99;	// Deletes the entry in the lookup table
			strcpy(lookup[cnt].path,"blah");
			break;
		}
		cnt++;
	}
	
	// Return this block to the free block list
	returnFreeBlock(node);

	// Finding the location of the file from the inode 
	char fuse_file[50];
	sprintf(fuse_file, "%s%d", BLOCK_PREFIX, node);

	struct file_inode *file_node = malloc(sizeof(struct file_inode));

	fptr = fopen( fuse_file,"r" );
	fread( file_node, sizeof(*file_node), 1 , fptr );
	fclose(fptr);

	int file_node_no = file_node->location;

	// Write zeros to the file_inode
	fptr = fopen( fuse_file,"w" );
	fprintf(fptr, "%04096d", 0);
	fclose(fptr);	

	// Return this block to the free block list too
	returnFreeBlock(file_node_no);

	// Write zeros to the actual file
	sprintf(fuse_file, "%s%d", BLOCK_PREFIX, file_node_no);
	fptr = fopen( fuse_file,"w" );
	fprintf(fptr, "%04096d", 0);
	fclose(fptr);

	//
	// Remove entry from the parent folder (Get parent folder, find the file, and remove it from the folder)
	//

	// get the parent folder
	char parent_path[20];

	cnt = 0;
	int pos;
	while ( path[cnt] ) {
		if( path[cnt] == '/' )
			pos = cnt;
		cnt++;
	}
	strncpy( parent_path , path , pos );
	parent_path[pos] = '\0';

	int p_node;
	if ( pos == 0 )
		p_node = ROOT_NODE;
	else {
		cnt = 0;
		while(lookup[cnt].fileno != 0){
			if ( strcmp(lookup[cnt].path , parent_path) == 0 ){
				p_node = lookup[cnt].fileno;
				break;
			}
			cnt++;
		}	
	}

	sprintf(fuse_file, "%s%d", BLOCK_PREFIX, p_node);

	// Get the data from the parent node onto a temp inode_table
	struct inode_table *parent_node = malloc(sizeof(struct inode_table));
	
	fptr = fopen(fuse_file,"r");
	fread(parent_node, sizeof(struct inode_table), 1 , fptr);
	fclose(fptr);

	// Remove the file from this folder info
	for ( cnt = 0 ; cnt < parent_node->linkcount ; cnt++){
		if ( parent_node->fidict[cnt].block_no == node ){
			parent_node->fidict[cnt].block_no = 0;
			strcpy( parent_node->fidict[cnt].name , "\0") ;
			parent_node->fidict[cnt].type = '\0';
			parent_node->linkcount--;
			break;
		}
	}

	// Write this folder inode_table back to the fusedata file
	fptr = fopen(fuse_file,"w");
	fwrite(parent_node, sizeof(struct inode_table), 1 , fptr);
	fclose(fptr);

	// Free the memory allocated for the structures
	free(file_node);
	free(parent_node);

	return ret_val;
}

int sanni_opendir(const char *path, struct fuse_file_info *fi) {
	// Open dir does not do anything. The inode file open in handled in the readdir itself
	return 0;
}

int sanni_releasedir(const char *path, struct fuse_file_info *fi) {
	// Same goes for release dir. Nothing handled here.
	return 0;
}

int sanni_release(const char *path, struct fuse_file_info *fi) {
	// Same goes for release. Nothing handled here.
	return 0;
}

int sanni_statfs(const char *path, struct statvfs *statv) {

	int ret_val = 0;

	// Getting the number of free blocks from the free block list
	FILE *fptr;
	int free_start;

	fptr = fopen(FREE_LIST_START,"r");
	fscanf(fptr,"%1d", &free_start);
	fclose(fptr);
	
	
	// Updating the values using the above obtained value
	statv->f_bsize 		= BLOCK_SIZE;
	statv->f_frsize 	= BLOCK_SIZE;
	statv->f_blocks 	= MAX_NO_BLOCKS;
	statv->f_bfree 		= (MAX_NO_BLOCKS - free_start);
	statv->f_bavail 	= (MAX_NO_BLOCKS - free_start);
	statv->f_files 		= 4;
	statv->f_ffree 		= (MAX_NO_BLOCKS - free_start);
	statv->f_favail 	= (MAX_NO_BLOCKS - free_start);
	statv->f_fsid 		= 20;
	statv->f_flag		= 1;
	statv->f_namemax 	= 20;

	return ret_val;

}

void sanni_destroy(void *data) {

	FILE *fptr;

	// Updating the superblock onto fusedata.0 before quitting
	fptr = fopen(SUPER_BLOCK,"w");
	fwrite(super_block, sizeof(struct superblock), 1 , fptr);
	fclose(fptr);

	// Updating the lookup file
	fptr = fopen( LOOKUP_PATH , "w");
	fwrite(&lookup , sizeof(lookup), 1 , fptr);
	fclose(fptr);

	free(super_block);

	// Nothing else to be done here. All memory and file pointers are freed immediately after use.

}

static struct fuse_operations sanni_oper = {

	// Done 

	.init		= sanni_init,			// The init method which gets called at the beginning of mount
	.getattr	= sanni_getattr,		// Get the attributes of the entity(file and folder)
	.readdir	= sanni_readdir,		// Read the contents of the folder
	.mkdir		= sanni_mkdir,			// Creates a new folder
	.destroy	= sanni_destroy,		// Cleanup. Function executed just before exiting
	.opendir 	= sanni_opendir,		// opendir methid functionality handled in readdir itself
	.rename		= sanni_rename,			// Rename a file or a folder	
	.create		= sanni_create,			// Creates a node for a non-directory node.
	.write 		= sanni_write, 			// Writes to the file opened
	.open 		= sanni_open,			// Open the file and stores the file handle
	.read 		= sanni_read,			// Read the file using the file handle given by OPEN
	.statfs		= sanni_statfs,			// Give statistics about the filesystem
	.releasedir = sanni_releasedir,		// Release the dir-file handles opened in readdir, mkdir
	.release 	= sanni_release, 		// Release the file handles opened in open
	.unlink		= sanni_unlink 			// Remove and unlink a file 

	// To be done
	/*
	.link		= sanni_link,
	*/
};

int main(int argc, char *argv[])
{
	return fuse_main(argc, argv, &sanni_oper, NULL);
}
