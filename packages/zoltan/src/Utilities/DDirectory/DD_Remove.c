/*****************************************************************************
 * Zoltan Dynamic Load-Balancing Library for Parallel Applications           *
 * Copyright (c) 2000, Sandia National Laboratories.                         *
 * This software is distributed under the GNU Lesser General Public License. *
 * For more info, see the README file in the top-level Zoltan directory.     *
 *****************************************************************************/
/*****************************************************************************
 * CVS File Information :
 *    $RCSfile$
 *    $Author$
 *    $Date$
 *    $Revision$
 ****************************************************************************/

#include <stdio.h>
#include <stdlib.h>

#include "DD_Const.h"



/*  NOTE: See file, README, for associated documentation. (RTH) */



static int DD_Remove_Local (Zoltan_DD_Directory *dd, LB_ID_PTR gid) ;




/********************   Zoltan_DD_Remove()  ***********************/

int Zoltan_DD_Remove (Zoltan_DD_Directory *dd,
 LB_ID_PTR gid,                      /* Incoming list of GIDs to remove */
 int count)                          /* Number of GIDs in removal list  */
   {
   int             *procs = NULL ;   /* list of processors to contact   */
   DD_Remove_Msg   *ptr   = NULL ;
   struct Comm_Obj *plan  = NULL ;   /* efficient MPI communication     */
   char            *sbuff = NULL ;   /* send buffer                     */
   char            *rbuff = NULL ;   /* receive buffer                  */

   int              nrec ;           /* number of receives to expect    */
   int              i ;
   int              err ;            /* error condition to return       */
   int              errcount ;
   char             str[100] ;       /* string to build error messages  */
   char            *yo = "Zoltan_DD_Remove" ;


   /* input sanity checks */
   if (dd == NULL || count < 0 || (gid == NULL && count > 0))
      {
      ZOLTAN_PRINT_ERROR (0, yo, "Invalid input argument") ;
      return ZOLTAN_DD_INPUT_ERROR ;
      }

   /* allocate memory for processor contact list */
   procs = (int *) LB_MALLOC (sizeof (int) * (count == 0) ? 1 : count) ;
   if (procs == NULL)
      {
      ZOLTAN_PRINT_ERROR (dd->my_proc, yo, "Unable to malloc proc list") ;
      return ZOLTAN_DD_MEMORY_ERROR ;
      }

   /* allocate memory for DD_Remove_Msg send buffer */
   sbuff = (char *) LB_MALLOC (dd->remove_msg_size * (count == 0) ? 1 : count) ;
   if (sbuff == NULL)
      {
      ZOLTAN_PRINT_ERROR (dd->my_proc, yo, "Unable to malloc send buffer") ;
      err = ZOLTAN_DD_MEMORY_ERROR ;
      goto fini ;
      }

   /* for each GID, fill in contact list and then message structure */
   for (i = 0 ; i < count ; i++)
      {
      procs[i] = dd->hash(gid + i*dd->gid_length, dd->gid_length, dd->nproc) ;

      ptr = (DD_Remove_Msg *) (sbuff + i * dd->remove_msg_size) ;
      ptr->owner = dd->my_proc ;
      LB_SET_ID (dd->gid_length, ptr->gid, gid + i * dd->gid_length) ;
      }
   /* create dummy message if count is zero to force participation */
   if (count == 0)
      {
      procs[0] = dd->my_proc ;
      ptr = (DD_Remove_Msg *) sbuff ;
      ptr->owner = ZOLTAN_DD_NO_PROC ;
      }

   /* now create efficient communication plan */
   err = LB_Comm_Create (&plan, count, procs, dd->comm,
    ZOLTAN_DD_REMOVE_MSG_TAG, &nrec) ;
   if (err != COMM_OK)
      {
      ZOLTAN_PRINT_ERROR (dd->my_proc, yo, "COMM Create error") ;
      goto fini ;
      }

   /* allocate receive buffer for nrec DD_Remove_Msg structures */
   rbuff = (char *) LB_MALLOC (nrec * dd->remove_msg_size) ;
   if (rbuff == NULL)
      {
      ZOLTAN_PRINT_ERROR (dd->my_proc, yo, "Unable to malloc receive buffer") ;
      err = ZOLTAN_DD_MEMORY_ERROR ;
      goto fini ;
      }

   /* send my remove messages & receive removes directed to me */
   err = LB_Comm_Do (plan, ZOLTAN_DD_UPDATE_MSG_TAG+1, sbuff,
    dd->remove_msg_size, rbuff) ;
   if (err != COMM_OK)
      {
      ZOLTAN_PRINT_ERROR (dd->my_proc, yo, "COMM Do error") ;
      goto fini ;
      }

   /* for each message rec'd,  remove local directory info */
   errcount = 0 ;
   for (i = 0 ; i < nrec ; i++)
      {
      ptr = (DD_Remove_Msg *) (rbuff + i * dd->remove_msg_size) ;
      if (ptr->owner == ZOLTAN_DD_NO_PROC)
         continue ;              /* ignore dummy message */

      err = DD_Remove_Local (dd, ptr->gid) ;
      if (err == ZOLTAN_DD_GID_NOT_FOUND_ERROR)
         errcount++ ;
      }
   err = (errcount == 0) ? ZOLTAN_DD_NORMAL_RETURN
                         : ZOLTAN_DD_GID_NOT_FOUND_ERROR ;

   /* done, now free up things and return */
 fini:
   LB_FREE (&procs) ;
   LB_FREE (&sbuff) ;
   LB_FREE (&rbuff) ;
   LB_Comm_Destroy (&plan) ;

   if (dd->debug_level > 0)
      {
      sprintf (str, "Processed %d GIDs (%d local), %d GIDs not found",
       count, nrec, errcount) ;
      ZOLTAN_PRINT_INFO (dd->my_proc, yo, str) ;
      }
   return err ;
   }







/******************  DD_Remove_Local()  **************************/

/* The given global ID, gid, is removed from the local distributed
// directory. An error is returned if the gid is not found.
*/

static int DD_Remove_Local (Zoltan_DD_Directory *dd,
 LB_ID_PTR gid)                /* GID to be removed (in)  */
   {
   DD_Node *ptr ;
   DD_Node *old ;
   int index ;
   int head = TRUE ;        /* True indicates head of linked list */
   char *yo = "DD_Remove_Local" ;

   /* input sanity checking */
   if (dd == NULL || gid == NULL)
      {
      ZOLTAN_PRINT_ERROR (0, yo, "Invalid input argument") ;
      return ZOLTAN_DD_INPUT_ERROR ;
      }

   /* compute offset into hash table to find head of linked list */
   index = DD_Hash2 (gid, dd->gid_length, dd->table_length) ;

   /* walk linked list until end looking for matching gid (key) */
   for (ptr = dd->table[index] ; ptr != NULL ; ptr = ptr->next)
      {
      if (LB_EQ_ID(dd->gid_length, gid, ptr->gid) == TRUE)
         {
         /* found node to remove, need to preserve its next ptr. */
         if (head == TRUE)
            dd->table[index] = ptr->next ; /* at list head       */
         else
            old->next = ptr->next ;        /* within linked list */

         /* now free to delete node */
         LB_FREE (&ptr) ;
         return ZOLTAN_DD_NORMAL_RETURN ;
         }
      head = FALSE ;         /* no longer at head of list  */
      old = ptr ;            /* previous node              */
      }

   /* We get here only if the global ID has not been found */
   return ZOLTAN_DD_GID_NOT_FOUND_ERROR ;
   }
