/*

  cpcd

  COPY CD

  This should be the simplest CD copy/clone command ever!

  1. Determine the type of CD this is:
    a. Audio CD
    b. MP3 CD
    c. OTHER - for now, just supporting (a) and (b)

  2. Determine how the user wants to copy:
    a. Clone ISO
    b. Copy files to a directory

  3. Do it!

 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <cdio/cdio.h>
#include <cdio/disc.h>
#include <cdio/iso9660.h>

// cpp
#include <iostream>
#include <exception>

using namespace std;

/* 
  main


  1. Check Args

  2. Check CD
    a. is it there?
    b. what type is it?

  3. Check copy type
    a. valid type for this CD

  4. Make copy!
    a. check for errors
 */

// Defines

#define DEFAULT_LSN -1

// Forward Declarations
int clone_cd(CdIo_t *p_cdio);
int copy_cd_files(CdIo_t *p_cdio);

int is_cd_available(CdIo_t *p_cdio);

int copy_files_from_iso(CdIo *p_cdio, iso9660_t *p_iso);
int copy_file(iso9660_t *p_iso, iso9660_stat_t *p_file_data);

int copy_blocks(CdIo *p_cdio, lsn_t start_lsn, lsn_t end_lsn);
int copy_blocks_from_iso(iso9660_t *p_iso, lsn_t start_lsn, lsn_t end_lsn);

void exit_attempt(void);

void write_resume_file();
void read_resume_file();

bool is_bad_block(int block);

// GLOBALS - SUPER HACKY

/*

  In gerneral, it is best to avoid global variables, but in this case
  an unexpected call to exit() may occur in the linked libcdio library,
  causing *this* program to terminate unexpectedly.

  The following global variables are used in the exit_attempt() function to
  perform final clean-up before termination.
  
*/

lsn_t g_lsn;
void *g_buffer;

// HACK
int g_bad_blocks[128];

// main
int main(int argc, const char *argv[])
{
  // Initialize Globals
  g_lsn = DEFAULT_LSN;
  g_buffer = NULL;
  
  // Parse args
  // TODO

  // RESUME HACK
  read_resume_file();
  // END RESUME HACK

  // Error Handler
  atexit(exit_attempt);

  CdIo_t *p_cdio = cdio_open (NULL, DRIVER_DEVICE);

  int cd_available = is_cd_available(p_cdio);
  if (cd_available) {
    try {
      copy_cd_files(p_cdio);
    } catch (...) {
      printf("Exception!\n");
      return 1;
    }
  } else {
    cdio_destroy(p_cdio);
    return 2;
  }
  
  cdio_destroy(p_cdio);
  return 0;
}


// Functions

void exit_attempt(void) {
  if (g_buffer != NULL) {
    free(g_buffer);
    g_buffer = NULL;

    // resume
    write_resume_file();

    printf("EXIT - LAST LSN: %li\n", g_lsn);
  }
}

int clone_cd(CdIo_t *p_cdio)
{
}

int copy_cd_files(CdIo_t *p_cdio)
{
  int ret_val = 0;
  
  char *ps_default_device_path = cdio_get_default_device (p_cdio);
  if (NULL == ps_default_device_path) {
    return 0;
  }
  
  iso9660_t *p_iso = iso9660_open (ps_default_device_path);

  if (NULL != p_iso) {
    printf("ISO9660 Open.\n");    
    copy_files_from_iso(p_cdio, p_iso);
    
    bool iso_close_success = iso9660_close (p_iso);
    if (iso_close_success) {
      printf ("ISO9660 Closed.\n");
    } else {
      printf ("ISO9660 NOT Closed.\n");
    }
  } else {
    printf("ISO9660 Not Open\n");
  }

  return ret_val;
}

int is_cd_available(CdIo_t *p_cdio)
{
  int success = 0;

  if (NULL != p_cdio)
  {
    success = 1;
    printf("The driver selected is %s\n", cdio_get_driver_name(p_cdio));
  } else {
    printf("Problem finding CD driver.\n\n");
  }

  return success;
}


int copy_files_from_iso(CdIo *p_cdio, iso9660_t *p_iso)
{
  int ret_code = 0;
  
    // Get the PVD
  iso9660_pvd_t pvd;
  iso9660_pvd_t *p_pvd = &pvd;

  bool pvd_success = iso9660_fs_read_pvd (p_cdio, p_pvd);
  if (pvd_success) {
    printf("PVD Read Success.\n");
  } else {
    printf("PVD Read Failure.\n");
    return 0;
  }
  
  lsn_t pvd_lsn = iso9660_get_root_lsn(p_pvd);

  if (0 != pvd_lsn) {
    const char *p_path = "/\0";
    CdioList_t *p_file_list =  iso9660_ifs_readdir(p_iso, p_path);

    if (NULL != p_file_list) {
      printf("Files:\n");
      CdioListNode_t * p_node = NULL;

      _CDIO_LIST_FOREACH (p_node, p_file_list) {
	void *p_node_data = _cdio_list_node_data (p_node);
	iso9660_stat_t *p_file_data = (iso9660_stat_t *)p_node_data;

	if (iso9660_stat_s::_STAT_FILE == p_file_data->type) {

	  // HACK - only copy file named...
	  /*
	  char search_filename[] = "43.mp3";
	  char *p_search_filename = search_filename;
	  
	  char *p_filename = p_file_data->filename;
	  char clean_filename[128]; //<-- HACK
	  char *p_clean_filename = clean_filename;
	  int filename_len =  iso9660_name_translate(p_filename, p_clean_filename);
	  int  found_file = 0 == (strcmp(p_clean_filename, p_search_filename));
	  */
	  bool found_file = true;

	  // Don't overwrite existing files
	  char *p_filename = p_file_data->filename;
	  char clean_filename[128]; //<-- HACK
	  char *p_clean_filename = clean_filename;
	  int filename_len =  iso9660_name_translate(p_filename, p_clean_filename);
	  
	  bool file_exists = false;
	  FILE *fp = fopen(p_clean_filename, "r");
	  if (fp) {
	    file_exists = true;
	    fclose(fp);
	  }
	  
	  if (found_file && false == file_exists) {
	    copy_file(p_iso, p_file_data);
	  }
	}
      }
    }
    
    free(p_file_list);
  }

  return ret_code;
}

int copy_blocks(CdIo *p_cdio, lsn_t start_lsn, lsn_t end_lsn) {

  char *ps_default_device_path = cdio_get_default_device (p_cdio);
  if (NULL == ps_default_device_path) {
    return 0;
  }
  
  iso9660_t *p_iso = iso9660_open (ps_default_device_path);

  if (NULL != p_iso) {
    printf("ISO9660 Open.\n");    
    copy_blocks_from_iso(p_iso, start_lsn, end_lsn);
    
    bool iso_close_success = iso9660_close (p_iso);
    if (iso_close_success) {
      printf ("ISO9660 Closed.\n");
    } else {
      printf ("ISO9660 NOT Closed.\n");
    }
  } else {
    printf("ISO9660 Not Open\n");
  }
}

int copy_blocks_from_iso(iso9660_t *p_iso, lsn_t start_lsn, lsn_t end_lsn) {
  // Output
  printf("Start:   %i\n", start_lsn);

  for (int i = 0; i < 128; i++) {
    if (g_bad_blocks[i] != 0) {
      printf("Bad Block: %i, \n", g_bad_blocks[i]);
    }
  }
  
  // Read the cd file
  long int blocks_to_read = (end_lsn - start_lsn) + 1;
  long int bytes_to_read = blocks_to_read * ISO_BLOCKSIZE;
  long int buffer_size = bytes_to_read;
  
  g_buffer = calloc(buffer_size, 1);
  char *cur_buffer = (char *)g_buffer;

  const int offset_increment = ISO_BLOCKSIZE / sizeof(char);

  long int blocks_read = 0;
  long int bytes_read = 0;

  lsn_t cur_lsn = start_lsn;
  lsn_t last_lsn = end_lsn;

  while (blocks_read < blocks_to_read && cur_lsn <= last_lsn) {
    /*

      The iso9660_iso_seek_read() function will call exit() on a read failure, 
      printing to stderr:

          **ERROR: fread (): Input/output error

      The call to exit() causes this program to terminate.  To avoid leaking memory,
      the global g_buffer variable is used to free the buffer before termination.
      See the exit_attempt() function for more details.

    */

    if (is_bad_block(cur_lsn)) {
      printf("Skipping Block: %i\n", cur_lsn);
      cur_lsn += 1;
      g_lsn = cur_lsn;

      // Advance the buffer past this block
      cur_buffer = cur_buffer + offset_increment;
      
      continue;
    }

    g_lsn = cur_lsn;
    
    long int cur_bytes_read = iso9660_iso_seek_read (p_iso, cur_buffer, cur_lsn, 1 /* blocks to read */);
    blocks_read++;
    bytes_read += cur_bytes_read;

    cur_buffer = cur_buffer + offset_increment;

    // sanilty check
    if (cur_bytes_read != ISO_BLOCKSIZE) {
      printf("Read mismatch - bytes read: %i\tblock size: %i\n", cur_bytes_read, ISO_BLOCKSIZE);
    }

    if (cur_bytes_read == 0) {
      printf("\nError - read error: %i\n", cur_lsn);
    } else {
      // printf(".");
    }

    cur_lsn += 1;
  }

  // Read file from CD complete
  g_lsn = DEFAULT_LSN;  //<-- reset g_lsn to default (TODO - make this cleaner)

  printf("\n");


  // Write to local file
  char filename[64];
  char startname[24];
  char endname[24];
  
  char *p_filename = filename;
  char *p_startname = startname;
  char *p_endname = endname;

  /*
  itoa(start_lsn, p_startname, 10);
  itoa(end_lsn, p_endname, 10);
  strcat(p_filename, p_startname);
  strcat(p_filename, "-");
  strcat(p_filename, p_endname);
  strcat(p_filename, ".pcd");
  */

  sprintf(p_filename, "%i-%i.pcd", start_lsn, end_lsn);
  
  printf("Writing partial file: %s\n", p_filename);

  FILE *fp = fopen(p_filename, "wb");
  if (fp) {
    // TODO - write 1M bytes at a time
    char *marker = (char *)g_buffer;
    uint32_t write_block_size = 10000; // 10k
    int32_t remaining_bytes_to_write = buffer_size;

    printf("%s\n", p_filename);

    while (0 < remaining_bytes_to_write) {
      if (remaining_bytes_to_write < write_block_size) {
	write_block_size = remaining_bytes_to_write;
      }
      
      uint32_t bytes_written = fwrite(marker, 1, write_block_size, fp);

      // Update write info
      marker = marker + bytes_written;
      remaining_bytes_to_write = remaining_bytes_to_write - bytes_written;
    }
    
    fclose(fp);
  } else {
    printf("Error - cannot create file: %s\n", p_filename);
  }
  
  free(g_buffer);
  g_buffer = NULL;

  return 0;
}

int copy_file(iso9660_t *p_iso, iso9660_stat_t *p_file_data)
{
  char *p_filename = p_file_data->filename;
  char clean_filename[128]; //<-- HACK
  char *p_clean_filename = clean_filename;
  int filename_len =  iso9660_name_translate(p_filename, p_clean_filename);
  
  lsn_t lsn = p_file_data->lsn;
  uint32_t size = p_file_data->size;

  // Output
  printf("-- %s --\n\n", p_clean_filename);

  printf("Reading: %s\n", p_clean_filename);
  printf("Start:   %i\n", lsn);
  
  // Read the cd file
  long int blocks_to_read = (size / ISO_BLOCKSIZE) + 1; //<-- over-read, then toss the extra bytes
  long int bytes_to_read = blocks_to_read * ISO_BLOCKSIZE;

  g_buffer = calloc(bytes_to_read, 1);

  unsigned char *cur_buffer = (unsigned char *)g_buffer;
  const int offset_increment = ISO_BLOCKSIZE / sizeof(unsigned char);
  
  long int blocks_read = 0;
  long int bytes_read = 0;
  lsn_t cur_lsn = lsn;
  lsn_t last_lsn = lsn + blocks_to_read;

  // RESUME HACK
  if (g_lsn != DEFAULT_LSN) {
    cur_lsn = g_lsn;
  }
  // END RESUME HACK
  
  while (blocks_read < blocks_to_read && cur_lsn < last_lsn) {

    if (is_bad_block(cur_lsn)) {
      printf("Skipping Block: %i\n", cur_lsn);
      cur_lsn += 1;
      g_lsn = cur_lsn;

      // Advance the buffer past this block
      cur_buffer = cur_buffer + offset_increment;

      continue;
    }
    
    g_lsn = cur_lsn;
    
    long int cur_bytes_read = iso9660_iso_seek_read (p_iso, cur_buffer, cur_lsn, 1 /* blocks to read */);
    blocks_read++;
    bytes_read += cur_bytes_read;

    cur_buffer = cur_buffer + offset_increment;

    if (cur_bytes_read == 0) {
      printf("\nError - read error: %i\n", cur_lsn);
    } else {
      // printf(".");
    }

    //cur_lsn += ISO_BLOCKSIZE; //<-- not sure about this...
    //
    cur_lsn += 1;
  }

  // Read file from CD complete
  g_lsn = DEFAULT_LSN; //<-- reset g_lsn to default (TODO - make this cleaner)


  // Error Handling
  /*
  if (bytes_to_read != bytes_read) {
    printf("Error: bytes read not equal to bytes requested (%li : %li)\n", bytes_to_read, bytes_read);
    free(g_buffer);
    g_buffer = NULL;
    return -1;
  } else {
    printf("Success\n");
  }
  */

  // HACK
  //free(g_buffer);
  //g_buffer = NULL;
  //return 0;
  // END HACK
  
  uint32_t tossed_bytes = bytes_to_read - size;

  // Write to local file
  FILE *fp = fopen(p_clean_filename, "wb");
  if (fp) {
    // TODO - write 1M bytes at a time
    char *marker = (char *)g_buffer;
    uint32_t write_block_size = 10000; // 10k
    int32_t remaining_bytes_to_write = size;

    printf("Writing: %s\n", p_clean_filename);

    while (0 < remaining_bytes_to_write) {
      if (remaining_bytes_to_write < write_block_size) {
	write_block_size = remaining_bytes_to_write;
      }
      
      uint32_t bytes_written = fwrite(marker, 1, write_block_size, fp);

      // Update write info
      marker = marker + bytes_written;
      remaining_bytes_to_write = remaining_bytes_to_write - bytes_written;
    }
    
    fclose(fp);
  } else {
    printf("Error - cannot create file: %s\n", p_clean_filename);
  }
  
  free(g_buffer);
  g_buffer = NULL;

  printf("\n");
  
  return 0;
}


/*

  Resume copy after unexpected exit()

 */

void write_resume_file() {
  const char resume_filename[] = "resume.cpd";

  char bad_block[32];
  char *p_bad_block = bad_block;

  sprintf(p_bad_block, "%i\n", g_lsn);
  int string_length = strlen(p_bad_block);
  
  FILE *fp = fopen(resume_filename, "a");
  if (fp) {
    fwrite(p_bad_block, 1, string_length, fp);
    fclose(fp);
  }
}

void read_resume_file() {
  const char resume_filename[] = "resume.cpd";

  int *p_bad_block = g_bad_blocks;

  FILE *fp = fopen(resume_filename, "r");
  if (fp) {
    while (EOF != fscanf(fp, "%i\n", p_bad_block) && p_bad_block < &g_bad_blocks[127]) {
      p_bad_block++;
    }
    fclose(fp);
  }
}

bool is_bad_block(int block) {
  for (int i = 0; i < 128; i++) {
    if (g_bad_blocks[i] == block) {
      return true;
    }
  }
  return false;
}
