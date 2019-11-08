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
#include <sys/types.h>
#include <cdio/cdio.h>
#include <cdio/disc.h>
#include <cdio/iso9660.h>

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

// Forward Declarations
int clone_cd(CdIo_t *p_cdio);
int copy_cd_files(CdIo_t *p_cdio);

int is_cd_available(CdIo_t *p_cdio);

int copy_files_from_iso(CdIo *p_cdio, iso9660_t *p_iso);
int copy_file(iso9660_t *p_iso, iso9660_stat_t *p_file_data);

// main
int main(int argc, const char *argv[])
{
  // Parse args
  // TODO

  CdIo_t *p_cdio = cdio_open (NULL, DRIVER_DEVICE);

  int cd_available = is_cd_available(p_cdio);
  if (cd_available) {
    copy_cd_files(p_cdio);
  } else {
    cdio_destroy(p_cdio);
    return 1;
  }

  cdio_destroy(p_cdio);
  return 0;
}


// Functions

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

	if (_STAT_FILE == p_file_data->type) {
	  copy_file(p_iso, p_file_data);
	}
      }
    }
    
    free(p_file_list);
  }

  return ret_code;
}

int copy_file(iso9660_t *p_iso, iso9660_stat_t *p_file_data)
{
  char *p_filename = p_file_data->filename;
  char clean_filename[128]; //<-- HACK
  char *p_clean_filename = clean_filename;
  int filename_len =  iso9660_name_translate(p_filename, p_clean_filename);
  
  lsn_t lsn = p_file_data->lsn;
  uint32_t size = p_file_data->size;
  
  // Read the cd file
  long int blocks_to_read = (size / ISO_BLOCKSIZE) + 1; //<-- over-read, then toss the extra bytes
  uint32_t bytes_to_read = blocks_to_read * ISO_BLOCKSIZE;

  void *buffer = malloc(bytes_to_read);
  long int bytes_read = iso9660_iso_seek_read (p_iso, buffer, lsn, blocks_to_read);
  
  uint32_t tossed_bytes = bytes_to_read - size;

  // Write to local file
  FILE *fp = fopen(p_clean_filename, "wb");
  if (fp) {
    // TODO - write 1M bytes at a time
    char *marker = buffer;
    uint32_t write_block_size = 10000; // 10k
    int32_t remaining_bytes_to_write = size;

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
  
  free(buffer);

  return 0;
}
