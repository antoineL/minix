#include "inc.h"

#include <sys/stat.h>
#include <string.h>
#include <minix/com.h>
#include <minix/vfsif.h>

int fs_rdlink(void)
{
  struct buf *bp;
  struct dir_record *dir;      
  register int r;
  size_t copylen;

  copylen =  fs_m_in.REQ_MEM_SIZE;
  dir = get_dir_record(fs_m_in.REQ_INODE_NR);

  if (dir == NULL) return(EINVAL); /* no inode found */

  if( !S_ISLNK(dir->d_mode) )
	  r = EACCES;
  else {
      size_t ln_len = 0;
      if (dir->d_rrip.s_link)
          ln_len = (size_t) strlen(dir->d_rrip.s_link);
      
      if(!ln_len)
          return(ENOLINK);

      copylen = copylen > ln_len ? ln_len : copylen;

      r = sys_safecopyto(VFS_PROC_NR, (cp_grant_id_t) fs_m_in.REQ_GRANT,
			   (vir_bytes) 0, (vir_bytes)(dir->d_rrip.s_link),
	  		   (size_t) copylen);
	if (r == OK)
		fs_m_out.RES_NBYTES = copylen;
  }
  
  return(r);
}
