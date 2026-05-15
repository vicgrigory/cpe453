/* Welcome to secrets.c! */

/*
* This is the source code for a secret keeper Minix driver!
* You may use this driver to store secrets! It is also possible
* to transfer secrets to another UID.
*
* Dependencies: /usr/src/include/minix
*/

#include <minix/drivers.h>
#include <minix/driver.h>
#include <minix/const.h>
#include <stdio.h>
#include <stdlib.h>
#include <minix/ds.h>
#include <sys/ucred.h>
#include <sys/socket.h>

/* Secret buffer size*/
#ifndef SECRET_SIZE
#define SECRET_SIZE 8192
#endif

/* R/W bits */
#define READ_BITS 4
#define WRITE_BITS 2
#define AND_BITS 7

/*
 * Function prototypes for the hello driver.
 */
FORWARD _PROTOTYPE( char * secret_name,   (void) );
FORWARD _PROTOTYPE( int secret_open,      (struct driver *d, message *m) );
FORWARD _PROTOTYPE( int secret_close,     (struct driver *d, message *m) );
FORWARD _PROTOTYPE( int secret_ioctl,     (struct driver *d, message *m) );
FORWARD _PROTOTYPE( struct device * secret_prepare, (int device) );
FORWARD _PROTOTYPE( int secret_transfer,  (int proc_nr, int opcode,
                                          u64_t position, iovec_t *iov,
                                          unsigned nr_req) );
FORWARD _PROTOTYPE( void secret_geometry, (struct partition *entry) );

/* SEF functions and variables. */
FORWARD _PROTOTYPE( void sef_local_startup, (void) );
FORWARD _PROTOTYPE( int sef_cb_init, (int type, sef_init_info_t *info) );
FORWARD _PROTOTYPE( int sef_cb_lu_state_save, (int) );
FORWARD _PROTOTYPE( int lu_state_restore, (void) );

/* Entry points to the hello driver. */
PRIVATE struct driver secret_tab =
{
    secret_name,
    secret_open,
    secret_close,
    secret_ioctl,
    secret_prepare,
    secret_transfer,
    nop_cleanup,
    secret_geometry,
    nop_alarm,
    nop_cancel,
    nop_select,
    nop_ioctl,
    do_nop,
};

/** Represents the /dev/hello device. */
PRIVATE struct device secret_device;

/* Number of file descriptors open */
PRIVATE int fd_count;

/* Indeces for read/ write heads */
PRIVATE int index_read;
PRIVATE int index_write;

/* Read bit, whether a read fd has been opened */
PRIVATE int read_bit;

/* Buffer for the secret keeping */
PRIVATE char buffer[SECRET_SIZE];

/* Owner of the secret */
PRIVATE uid_t owner = -1;

/* I have deemed this a trivial function */
PRIVATE char * secret_name(void)
{
    printf("secret_name()\n");
    return "sup";
}

/* secret_open 
* The entry point to open a file descriptor for the device.
*     Does not do the reading.
* Inputs: driver d, message m
*     driver called and messages and flags to call the function with
* Output: int - exit status
*/
PRIVATE int secret_open(d, m)
    struct driver *d;
    message *m;
{
    /* getnucred error checking */
    int err;
    /* For getting the user ID */
    struct ucred user;
    /* The OR'd open mode that was sent */
    int open_mode;
    
    /* OR this with 7 to determine the read versus write mode */
    /* 7 = b'111, R/W bits */
    open_mode = m->COUNT & AND_BITS;

    switch (open_mode)
    {
        /* O_RDONLY */
        case READ_BITS:
            /* Check if we have an unread secret */
            if (index_read != index_write) {
                /* Get the UID of the process calling */
                err = getnucred(m->USER_ENDPT, &user);
                if (err == -1) {
                    return -1;
                }

                /* If the calling process is owned by the owner of the secret,
                 *   we allow them to read it
                 */
                if (user.uid == owner) {
                    /* Set the secret as read and add a file descriptor */
                    read_bit = 1;
                    fd_count++;

                    return OK;
                }
                /* Otherwise we deny */
                return EACCES;
            }
            /* If there is no secret we allow anyone to open it */
            /* Still need to increment the file descriptor */
            fd_count++;
            return OK;

        /* O_WRONLY */
        case WRITE_BITS:
            /* Check if we are owned */
            if (owner != -1) {
                /* We cannot open another writing fd if we are owned */
                return ENOSPC;
            }
            /* Otherwise we are empty and we can be opened for writing */

            /* Get the UID of the process calling */
            err = getnucred(m->USER_ENDPT, &user);
            if (err == -1) {
                return err;
            }

            /* Set the owner of the secret */
            owner = user.uid;
            fd_count++;

            return OK;

        /* Everything else */
        default:
            return EACCES;
    }
}

/* secret_close 
* The entry point to close a file descriptor.
* Inputs: driver d, message m
*     driver called and messages and flags to call the function with
* Output: int - exit status
*/
PRIVATE int secret_close(d, m)
    struct driver *d;
    message *m;
{
    /* Not sure when this will be applicable but if
     *   we already have no file descriptors open
     *   then return an error
     */
    if (fd_count == 0) {
        return EBADF;
    }

    /* Decriment fd the counter */
    fd_count--;

    /* Check if all fds have been closed and our secret has been read */
    if (fd_count == 0 && read_bit) {
        /* Reset ourselves to an empty state */
        owner = -1;
        read_bit = 0;
        index_read = 0;
        index_write = 0;
        memset(buffer, 0, sizeof(buffer));
        
        return OK;
    }

    /* In the case that we still have file descriptors
     *   open or the secret has not been opened for reading
     */
    return OK;
}

/* secret_ioctl
* The entry point to pass secret ownership to another user when given SSGRANT.
* Inputs: driver d, message m
*     driver called and messages and flags to call the function with
* Output: int - exit status
*/
PRIVATE int secret_ioctl(d, m)
    struct driver *d;
    message *m;
{
    /* Sys function error checking */
    int res;
    /* Owner ID */
    uid_t grantee;

    /* Check if the request was SSGRANT */
    if (m->REQUEST == SSGRANT) {
        /* Get and safely copy the UID from STDIN */
        res = sys_safecopyfrom(m->IO_ENDPT, (vir_bytes)m->IO_GRANT, 0,
                    (vir_bytes)&grantee, sizeof(grantee), D);
        if (res == -1) {
            return res;
        }

        /* Change the owner */
        owner = grantee;

        return OK;
    }
    
    /* If SSGRANT was not given we throw an error */
    return ENOTTY;
}

/* I have deemed this a trivial function */
PRIVATE struct device * secret_prepare(dev)
    int dev;
{
    secret_device.dv_base.lo = 0;
    secret_device.dv_base.hi = 0;
    secret_device.dv_size.lo = SECRET_SIZE;
    secret_device.dv_size.hi = 0;
    return &secret_device;
}

/* secret_transfer
* The entry point to transfer data between the device and the program.
* Inputs: driver d, message m
*     driver called and messages and flags to call the function with
* Output: int - exit status
*/
PRIVATE int secret_transfer(proc_nr, opcode, position, iov, nr_req)
    int proc_nr;
    int opcode;
    u64_t position;
    iovec_t *iov;
    unsigned nr_req;
{
    /* Bytes used for syscopy and error code */
    int bytes, ret;

    /* Based on the opcode given, either read or write */
    switch (opcode)
    {
        /* Read our secret */
        case DEV_GATHER_S:
            /* Figure out how many bytes to read */
            bytes = index_write-index_read < iov->iov_size ?
                    index_write-index_read : iov->iov_size;

            /* If bytes is less than or equal to 0
             *   we just exit
             */
            if (bytes <= 0)
            {
                return OK;
            }

            /* Actual reading */
            ret = sys_safecopyto(proc_nr, (cp_grant_id_t)iov->iov_addr, 0,
                                (vir_bytes) (buffer+(int)(index_read)),
                                 bytes, D);
            
            /* Set read head */
            index_read += bytes;
            /* Decrement the iov_size to signal we're done */
            iov->iov_size -= bytes;
            break;

        /* Write to our secret */
        case DEV_SCATTER_S:
            /* Figure out how many bytes we want to write
             *   and not write past the buffer size
             */
            bytes = index_write + iov->iov_size < SECRET_SIZE ?
                    iov->iov_size : SECRET_SIZE-index_write;

            if (bytes == 0) {
                return ENOSPC;
            }

            /* Actual writing */
            ret = sys_safecopyfrom(proc_nr, (cp_grant_id_t)iov->iov_addr, 0,
                                (vir_bytes) (buffer+(int)(index_write)),
                                bytes, D);
            
            /* Set our write head */
            index_write += bytes;

            /* Decrease iov size to signal that we're done */
            iov->iov_size -= bytes;
            break;

        /* Anything else */
        default:
            return EINVAL;
    }
    /* Return the code we got from the read or write */
    return ret;
}

/* I have deemed this a trivial function */
PRIVATE void secret_geometry(entry)
    struct partition *entry;
{
    printf("secret_geometry()\n");
    entry->cylinders = 0;
    entry->heads     = 0;
    entry->sectors   = 0;
}

/* I have also deemed these trivial functions */
/* They start up the driver and save and restore the state it was in. */
PRIVATE int sef_cb_lu_state_save(int state) {
/* Save the state. */

    ds_publish_mem("buffer", buffer, SECRET_SIZE, DSF_OVERWRITE);
    ds_publish_u32("fd_count", fd_count, DSF_OVERWRITE);
    ds_publish_u32("index_read", index_read, DSF_OVERWRITE);
    ds_publish_u32("index_write", index_write, DSF_OVERWRITE);
    ds_publish_u32("read_bit", read_bit, DSF_OVERWRITE);
    ds_publish_u32("owner", owner, DSF_OVERWRITE);

    return OK;
}

PRIVATE int lu_state_restore() {
/* Restore the state. */
    size_t size = SECRET_SIZE;
    u32_t fd_c;

    u32_t i_r;
    u32_t i_w;

    u32_t r;
    u32_t o;

    ds_retrieve_mem("buffer", buffer, &size);
    ds_delete_mem("buffer");

    ds_retrieve_u32("fd_count", &fd_c);
    ds_delete_u32("fd_count");

    ds_retrieve_u32("index_read", &i_r);
    ds_delete_u32("index_read");
    index_read = (int) i_r;

    ds_retrieve_u32("index_write", &i_w);
    ds_delete_u32("index_write");
    index_write = (int) i_w;

    ds_retrieve_u32("read_bit", &r);
    ds_delete_u32("read_bit");
    read_bit = (int) r; 

    ds_retrieve_u32("owner", &o);
    ds_delete_u32("owner");
    owner = (uid_t) o;

    return OK;
}

PRIVATE void sef_local_startup()
{
    /*
     * Register init callbacks. Use the same function for all event types
     */
    sef_setcb_init_fresh(sef_cb_init);
    sef_setcb_init_lu(sef_cb_init);
    sef_setcb_init_restart(sef_cb_init);

    /*
     * Register live update callbacks.
     */
    /* - Agree to update immediately when LU is requested in a valid state. */
    sef_setcb_lu_prepare(sef_cb_lu_prepare_always_ready);
    /* - Support live update starting from any standard state. */
    sef_setcb_lu_state_isvalid(sef_cb_lu_state_isvalid_standard);
    /* - Register a custom routine to save the state. */
    sef_setcb_lu_state_save(sef_cb_lu_state_save);

    /* Let SEF perform startup. */
    sef_startup();
}

PRIVATE int sef_cb_init(int type, sef_init_info_t *info)
{
/* Initialize the hello driver. */
    int do_announce_driver = TRUE;

    memset(buffer, 0, sizeof(buffer)); // reset to 0
    fd_count = 0;
    index_read = 0;
    index_write = 0;

    switch(type) {
        case SEF_INIT_FRESH:
            printf("The Secret Safe ready for work.\n");
        break;

        case SEF_INIT_LU:
            /* Restore the state. */
            lu_state_restore();
            do_announce_driver = FALSE;

            printf("The Secret Safe: I'm a new version!\n");
        break;

        case SEF_INIT_RESTART:
            printf("The Secret Safe: I've just been restarted!\n");
        break;
    }

    /* Announce we are up when necessary. */
    if (do_announce_driver) {
        driver_announce();
    }

    /* Initialization completed successfully. */
    return OK;
}

PUBLIC int main(int argc, char **argv)
{
    /*
     * Perform initialization.
     */
    sef_local_startup();

    /*
     * Run the main loop.
     */
    driver_task(&secret_tab, DRIVER_STD);
    return OK;
}

