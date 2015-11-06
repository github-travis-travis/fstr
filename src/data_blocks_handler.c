#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#include "common.h"
#include "disk_emulator.h"

#include "data_blocks_handler.h"

// TODO
// Modify tests and run them
// 1- check that in get_block_number_of_first_datablock, position_of_first_data_block is the same as the constant written by Tanuj.Maybe run tests for tanuj constant
// 2- change get_superblock by superblock global variable. or change get_superblock to return the global variable?
// 3- Decrease number of free datablocks in superblock!

big_int get_block_number_of_first_datablock(void) {
// int get_block_number_of_first_datablock(struct superblock * sp) {
	big_int block_size, total_number_of_inodes, inode_size, number_inodes_per_block, position_of_first_data_block;
	
	block_size = BLOCK_SIZE; //(int) sp->block_size;
	total_number_of_inodes = NUM_INODES; //(int) sp->number_of_inodes;
	inode_size = INODE_SIZE; //(int) sp->inode_size;

	number_inodes_per_block = block_size / inode_size;
	position_of_first_data_block = (int) ceil(((double)total_number_of_inodes) / ((double)number_inodes_per_block)) + 1; // Superblock block number is 0

	return position_of_first_data_block;
}

// first block number is when i = 1
big_int get_ith_block_number_in_datablock(char * datablock, int i) {
	// a block number is encoded on 4 bytes (size of an integer)
	big_int block_number;
	memcpy(&block_number, &datablock[(i-1) * sizeof(big_int)], sizeof(big_int));
	return block_number;
}

// first block number is when i = 1
void set_ith_block_number_in_datablock(char * datablock, int i, big_int block_number) {
	// a block number is encoded on 4 bytes (size of an integer)
	memcpy(&datablock[(i-1) * sizeof(big_int)], &block_number, sizeof(big_int));
}

// Returns 0 if there is only the pointer to the next datablock containing IDs of free datablocks) or if there is no pointer.
// 1st datablock number is always the ID of the next datablock containing the IDs of the free datablocks
int has_at_least_one_datablock_number_left_without_pointer(char * datablock) {
	int second_datablock_number;

	second_datablock_number = get_ith_block_number_in_datablock(datablock, 2);

	if (second_datablock_number == 0) {
		return 0;
	}
	return 1;
}

int is_datablock_full_of_free_datablock_numbers(char * datablock) {
	int position_of_last_block_number;
	big_int last_block_number;

	position_of_last_block_number = BLOCK_SIZE - sizeof(big_int);
	memcpy(&last_block_number, &datablock[position_of_last_block_number], sizeof(big_int));

	if (last_block_number == 0) {
		return 0;
	}
	return 1;
}

int get_ith_position_of_free_spot_in_free_datablock_number_list_for_new_free_datablock(char * datablock) {
	// a block number is encoded on 4 bytes (size of an integer)
	big_int block_number;
	int i;

	for (i = 1; (i * sizeof(big_int)) <= BLOCK_SIZE; i++) {
		memcpy(&block_number, &datablock[(i-1) * sizeof(big_int)], sizeof(big_int));

		if (block_number == 0) {
			return i;
		}
	}
	return -1;
}

// Shift all datablock numbers to the left in the buffer thus removing the second block number. Add 0s at the end of the buffer
// The 1st block number corresponding to the pointer to the next datablock containing list of block numbers is maintained in the buffer
void shift_datablock_numbers_in_buffer_to_left_except_pointer_to_next_block(char * datablock) {
	int bytes_to_be_copied, position_of_last_block_number;
	
	bytes_to_be_copied = BLOCK_SIZE - 2 * sizeof(big_int);
	memcpy(&datablock[sizeof(big_int)], &datablock[2 * sizeof(big_int)], bytes_to_be_copied);

	position_of_last_block_number = BLOCK_SIZE - sizeof(big_int);
	memset(&datablock[position_of_last_block_number], 0, sizeof(big_int)); // set 0s at the end of the buffer (empty block number)
}

struct data_block * data_block_alloc(void) {
	struct data_block * datablock;

	int position_of_first_datablock;
	big_int free_block_number_to_be_used;
	char read_buffer[BLOCK_SIZE];
	
	// We don't implement lock here for superblock !

	
	// 	We get 1st datablock. That's also the block that contains the id of the free data blocks and 
	// 	the pointer to the next data block containing other part of free data block numbers!
	
	position_of_first_datablock = get_block_number_of_first_datablock();
	free_block_number_to_be_used = -1;
	
	// Read the datablock containing normally some of the free data block numbers.
	if (read_block(position_of_first_datablock, read_buffer) == -1) {
		return NULL;
	}

	if (has_at_least_one_datablock_number_left_without_pointer(read_buffer) == 0) {
		// No block numbers left, we copy content of datablock pointed by current block into current block

		// We check if there's a block pointed by current block. If not, we return an error (NULL)
		big_int next_block_number = get_ith_block_number_in_datablock(read_buffer, 1);
		
		if (next_block_number == 0) {
			return NULL;
		}

		if (read_block(next_block_number, read_buffer) == -1) {
			return NULL;
		}

		if (write_block(position_of_first_datablock, read_buffer, BLOCK_SIZE) == -1) {
			return NULL;
		}

		// The block that was pointed by 1st data block will be used as the free data block
		free_block_number_to_be_used = next_block_number;
	}
	else {
		free_block_number_to_be_used = get_ith_block_number_in_datablock(read_buffer, 2);
		shift_datablock_numbers_in_buffer_to_left_except_pointer_to_next_block(read_buffer);
		write_block(position_of_first_datablock, read_buffer, BLOCK_SIZE);
	}

	datablock = (struct data_block *) malloc(sizeof(struct data_block));
	datablock->data_block_id = free_block_number_to_be_used;
	memset(datablock->block, 0, BLOCK_SIZE); // set 0s to the buffer
	write_block(free_block_number_to_be_used, datablock->block, BLOCK_SIZE); // Set 0s the block on disk too

	superblock.num_free_blocks--; // Decrement the number of free data blocks

	// mark_superblock_modified();

	return datablock;
}

struct data_block * bread(big_int data_block_nb) {
	struct data_block * datablock;

	datablock = (struct data_block *) malloc(sizeof(struct data_block));
	if (read_block(data_block_nb, datablock->block) == -1) {
		free(datablock);
		return NULL;
	}

	datablock->data_block_id = data_block_nb;
	return datablock;
}

int bwrite(struct data_block * datablock) {
	return write_block(datablock->data_block_id, datablock->block, BLOCK_SIZE);
}

int data_block_free(struct data_block * datablock) {
	int position_of_first_datablock, ith_position;
	char read_buffer[BLOCK_SIZE], buffer[BLOCK_SIZE];

	// We don't implement lock here for superblock!

	// Look at first datablock and see if it's full (full of free datablock numbers).
	position_of_first_datablock = get_block_number_of_first_datablock();

	// Read the datablock containing normally some of the free data block numbers.
	if (read_block(position_of_first_datablock, read_buffer) == -1) {
		return -1;
	}

	if (is_datablock_full_of_free_datablock_numbers(read_buffer) == 1) {
		// We copy the content of the 1st datablock (containing free datablock numbers) into the new free datablock
		// Then we clear the 1st datablock number and set the 1st block number (pointer) to be the number of the new free datablock

		if (write_block(datablock->data_block_id, read_buffer, BLOCK_SIZE) == -1) {
			return -1;
		}

		memset(buffer, 0, BLOCK_SIZE); // We sets 0s in the whole block
		set_ith_block_number_in_datablock(buffer, 1, datablock->data_block_id);
		if (write_block(position_of_first_datablock, buffer, BLOCK_SIZE) == -1) {
			return -1;
		}
	}
	else {
		// We add the new free datablock number at the 1st empty spot in the list of datablock numbers in the 1st datablock

		memset(datablock->block, 0, BLOCK_SIZE); // We sets 0s in the whole new datablock
		if (write_block(datablock->data_block_id, datablock->block, BLOCK_SIZE) == -1) {
			return -1;
		}

		ith_position = get_ith_position_of_free_spot_in_free_datablock_number_list_for_new_free_datablock(read_buffer);
		set_ith_block_number_in_datablock(read_buffer, ith_position, datablock->data_block_id);
		if (write_block(position_of_first_datablock, read_buffer, BLOCK_SIZE) == -1) {
			return -1;
		}
	}

	superblock.num_free_blocks++;
	// mark_superblock_modified();

	return 0;
}