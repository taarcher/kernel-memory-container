//Project 2: Thomas "Andy" Archer, taarcher



//////////////////////////////////////////////////////////////////////
//                      North Carolina State University
//
//
//
//                             Copyright 2018
//
////////////////////////////////////////////////////////////////////////
//
// This program is free software; you can redistribute it and/or modify it
// under the terms and conditions of the GNU General Public License,
// version 2, as published by the Free Software Foundation.
//
// This program is distributed in the hope it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
// more details.
//
// You should have received a copy of the GNU General Public License along with
// this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
//
////////////////////////////////////////////////////////////////////////
//
//   Author:  Hung-Wei Tseng, Yu-Chia Liu
//
//   Description:
//     Core of Kernel Module for Processor Container
//
////////////////////////////////////////////////////////////////////////

#include "memory_container.h"

#include <asm/uaccess.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/poll.h>
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/kthread.h>

/**
*   Node for the linked list of containers
**/
struct ContainerNode{
    //container id
    __u64 cid;
    
    //number of threads in the container
    int t_count;
    
    //number of objects in the container
    int o_count;
    
    //start of object linked list
    struct ObjectNode *start_o;
    
    //container linked likst pointer
    struct ContainerNode *next;
    
    struct ThreadNode *start_t;
    
    struct mutex lock;
};

/**
*   Node for the linked list in each container.
**/
struct ObjectNode {
    struct ObjectNode *next;
    struct ObjectNode *prev;
    
    //task using the object
    //struct task_struct* task;
    
    //object id
    unsigned long oid;
    
    //indicate if the object is locked
//    struct mutex lock;
    
    //something to point to the object
//    void * memory;
    unsigned long start;
};

/**
 * Node for the linked list of threads in each container
 */
struct ThreadNode {
    //thread id
    struct task_struct* task;

    //pointers for the thread linked list
	struct ThreadNode *next;
	struct ThreadNode *prev;
};


//head of the container linked list
struct ContainerNode *head = NULL;

//mutex lock to use
static DEFINE_MUTEX( mtx );

//returns pointer to the memory location of the object
int memory_container_mmap(struct file *filp, struct vm_area_struct *vma)
{
    //find task in the container to get the right container
    struct ContainerNode* temp;
    struct ContainerNode* iterC;
    struct ThreadNode* iterT;
    struct ObjectNode* iterO;
    struct ObjectNode* tempO;
    struct task_struct* ctask = current;
    
    
    unsigned long size = (unsigned long) (vma->vm_end - vma->vm_start);
    unsigned long oid = (unsigned long) vma->vm_pgoff;
    
    
    int i;
    
    mutex_lock( &mtx );
    printk( KERN_DEBUG "mtx locked by mmap" );
    
    iterC = head;
    temp = NULL;
    
    while( temp == NULL ){
        iterC = iterC->next;
        printk( KERN_DEBUG "check container %d", iterC->cid );
        iterT = iterC->start_t->next;
        for( i = 0; i < iterC->t_count; i++ ){
            printk( KERN_DEBUG "checking container %d for thread %d, found %d", iterC->cid, ctask->pid, iterT->task->pid );
            if( iterT->task->pid == ctask->pid ){
                temp = iterC;
                printk( KERN_DEBUG "found thread %d in container %d", ctask->pid, temp->cid );
                i = iterC->t_count;
            }
            if( temp == NULL ){
                iterT = iterT->next;
            }
        }
        if( temp == NULL && iterC->next == NULL ){
            printk( KERN_DEBUG "thread was not found" );
            mutex_unlock( &mtx );
            return 0;
        }
    }
    
    //then search objects in the container to find the oid
    printk( KERN_DEBUG "Container %d has %d objects", temp->cid, temp->o_count );
    tempO = NULL;
    if( temp->start_o->next != NULL ){
        printk( KERN_DEBUG "checking Container %d for object %d", temp->cid, oid );
        iterO = temp->start_o;
        for( i = 0; i < temp->o_count; i++ ){
            iterO = iterO->next;
            printk( KERN_DEBUG "checking container %d for object %d, found %d", temp->cid, oid, iterO->oid );
            if( iterO->oid == oid ){
                tempO = iterO;
                printk( KERN_DEBUG "set object %d to tempO", iterO->oid );
                i = iterC->o_count;
//                return vma->vm_start;
            }
        }
    }
    
    //if found, return the pointer that previous oid returned
    //is this the correct way to return the memory location?-------------------------------------------
    
    if( tempO != NULL ){

        printk( KERN_DEBUG "mmap unlocks mtx" );
        mutex_unlock( &mtx );
        printk( KERN_DEBUG "mmap really unlocked mtx" );
        printk( KERN_DEBUG "return address for object %d: %d", iterO->oid, iterO->start);
        return (iterO->start);
    }
    
    //if not, create object, add it to containers list, save pointer in that list, return pointer
    printk( KERN_DEBUG "creating object %d in container %d", oid, temp->cid );
    printk( KERN_DEBUG "allocating memory" );
    struct ObjectNode* newObj = kmalloc( sizeof (struct ObjectNode), GFP_KERNEL );
    printk( KERN_DEBUG "object id: %d", oid );
    newObj->oid = oid;
    printk( KERN_DEBUG "get memory address:" );
    printk( KERN_DEBUG "size: %d", size );
    printk( KERN_DEBUG "start: %d", vma->vm_start );
//    printk( KERN_DEBUG "kmalloc pointer" );
//    void * kptr = kmalloc( size , GFP_KERNEL );
//    newObj->memory = kptr;
//    printk( KERN_DEBUG "find karea" );
//    char * karea = (char *) (((unsigned long) kptr + (PAGE_SIZE - 1)) & PAGE_MASK);
//    printk( KERN_DEBUG "find pfn" );
//    unsigned long pfn = virt_to_phys ((void *) ((unsigned long) karea)) >> PAGE_SHIFT;
//    newObj->memory = kmalloc( size , GFP_KERNEL );
//    newObj->memory = virt_to_phys((void *)kmalloc( size, GFP_KERNEL ) );
//    printk( KERN_DEBUG "remap pfn range" );
//    remap_pfn_range( vma, vma->vm_start, pfn, size, vma->vm_page_prot );
//    newObj->memory = vma->vm_start;
    remap_pfn_range( vma, vma->vm_start, virt_to_phys(kmalloc( size, GFP_KERNEL ) ) >> PAGE_SHIFT, size, vma->vm_page_prot );
    newObj->start = vma->vm_start;
    printk( KERN_DEBUG "memory location: %d", newObj->start );
    printk( KERN_DEBUG "increase the object count in the container" );
    temp->o_count += 1;
    printk( KERN_DEBUG "Container %d object count: %d", temp->cid, temp->o_count );
    
    //newObj->next = NULL;
//    printk( KERN_DEBUG "assign pointers" );
//    if( temp->start_o->next != NULL ){
//        temp->start_o->next->prev = newObj;
//    }

//check here for null pointer reference ----------------------------------------------------------------
    printk( KERN_DEBUG "check for start of object list" );
    if( temp->o_count > 1 ){
        printk( KERN_DEBUG "move object list pointers" );
        newObj->next = temp->start_o->next;
        printk( KERN_DEBUG "newObj next value is now the old start next" );
    }
//    printk (KERN_DEBUG "Start O next: %d, NewObj next: %d", temp->start_o->next->oid, newObj->next->oid );
    newObj->prev = temp->start_o;
    printk (KERN_DEBUG "NewObj prev: start_o" );
    temp->start_o->next = newObj;
    printk( KERN_DEBUG "New start_o next is %d", temp->start_o->next->oid );
    
    if( newObj->next != NULL ){
        newObj->next->prev = newObj;
        printk( KERN_DEBUG "%d prev: newObj", newObj->next->prev->oid );
    }
    
//    mutex_init( &newObj->lock );
    
    printk( KERN_DEBUG "mmap unlocks mtx" );
    mutex_unlock( &mtx );
//    printk( KERN_DEBUG "trying to return memory" );
    
    return (newObj->start);
}


int memory_container_lock(struct memory_container_cmd __user *user_cmd)
{
    //find thread in container to identify container
    struct memory_container_cmd user;
    struct ContainerNode* temp;
    struct ContainerNode* iterC;
    struct ThreadNode* iterT;
    struct ObjectNode* iterO;
    struct ObjectNode* tempO;
    struct task_struct* ctask = current;
    
    int i;
    
    copy_from_user( &user, user_cmd, sizeof (struct memory_container_cmd) );
    
    mutex_lock( &mtx );
    printk( KERN_DEBUG "mtx locked by lock" );
    
    iterC = head;
    
    temp = NULL;
    
//    printk( KERN_DEBUG "search for the container with the task" );
    
    while( temp == NULL ){
        iterC = iterC->next;
//        printk( KERN_DEBUG "check container %d", iterC->cid );
        iterT = iterC->start_t->next;
        for( i = 0; i < iterC->t_count; i++ ){
//            printk( KERN_DEBUG "checking Con %d for thread %d, found %d", iterC->cid, ctask->pid, iterT->task->pid );
            if( iterT->task->pid == ctask->pid ){
                temp = iterC;
//                printk( KERN_DEBUG "found thread in Con %d", temp->cid);
                i = iterC->t_count;
            }
            if( temp == NULL ){
                iterT = iterT->next;
            }
        }
        if( temp == NULL && iterC->next == NULL ){
//            printk( KERN_DEBUG "thread was not found" );
//            printk( KERN_DEBUG "lock unlocks mtx" );
            mutex_unlock( &mtx );
            return 0;
        }
    }
    
    printk( KERN_DEBUG "container %d locked", temp->cid );
    mutex_lock( &temp->lock );
    
    printk( KERN_DEBUG "lock unlocks mtx" );
    mutex_unlock( &mtx );
    
/**    
    //find the object in the container
    tempO = NULL;
    iterO = iterC->start_o->next;
    for( i = 0; i < iterC->o_count; i++ ){
        printk( KERN_DEBUG "check Con %d for object %d, found %d", temp->cid, user.oid, iterO->oid );
        if( iterO->oid == user.oid ){
            printk( KERN_DEBUG "found the object: %d", iterO->oid);
            tempO = iterO;
            i = iterC->o_count;
        }
        if( tempO == NULL && iterO->next == NULL ){
            printk( KERN_DEBUG "object %d was not found", user.oid );
            return 0;
        }
        iterO = iterO->next;
    }
    
    //set the lock value for that object
    mutex_lock( &iterO->lock );
**/    
    return 0;
}


int memory_container_unlock(struct memory_container_cmd __user *user_cmd)
{
    //find thread in container to identify container
    struct memory_container_cmd user;
    struct ContainerNode* temp;
    struct ContainerNode* iterC;
    struct ThreadNode* iterT;
    struct ObjectNode* iterO;
    struct ObjectNode* tempO;
    struct task_struct* ctask = current;
    
    int i;
    
    copy_from_user( &user, user_cmd, sizeof (struct memory_container_cmd) );
    
    mutex_lock( &mtx );
    printk( KERN_DEBUG "mtx locked by unlock" );
    
    iterC = head;
    
    temp = NULL;
    
 //   printk( KERN_DEBUG "search for the container with the task" );
    
    while( temp == NULL ){
        iterC = iterC->next;
 //       printk( KERN_DEBUG "check container %d", iterC->cid );
        iterT = iterC->start_t->next;
        for( i = 0; i < iterC->t_count; i++ ){
 //           printk( KERN_DEBUG "checking Con %d for thread %d, found %d", iterC->cid, ctask->pid, iterT->task->pid );
            if( iterT->task->pid == ctask->pid ){
                temp = iterC;
//                printk( KERN_DEBUG "found thread in Con %d", temp->cid);
                i = iterC->t_count;
            }
            if( temp == NULL ){
                iterT = iterT->next;
            }
        }
        if( temp == NULL && iterC->next == NULL ){
//            printk( KERN_DEBUG "thread was not found" );
//            printk( KERN_DEBUG "unlock unlocks mtx" );
            mutex_unlock( &mtx );
            return 0;
        }
    }
    
    printk( KERN_DEBUG "continer %d unlocked", temp->cid );
    mutex_unlock ( &temp->lock );
    
    printk( KERN_DEBUG "unlock unlocks mtx" );
    mutex_unlock( &mtx );
    
    /**
    //find the object in the container
    tempO = NULL;
    iterO = iterC->start_o;
    for( i = 0; i < iterC->o_count; i++ ){
        iterO = iterO->next;
        printk( KERN_DEBUG "check Con %d for object %d, found %d", temp->cid, user.oid, iterO->oid );
        if( iterO->oid == user.oid ){
            printk( KERN_DEBUG "found the object: %d", iterO->oid);
            tempO = iterO;
            i = iterC->o_count;
        }
        if( tempO == NULL && iterO->next == NULL ){
            printk( KERN_DEBUG "object %d was not found", user.oid );
            return 0;
        }
    }
    
    //set the lock value for that object
    mutex_unlock( &iterO->lock );
    
    **/
    return 0;
}


int memory_container_delete(struct memory_container_cmd __user *user_cmd)
{
    struct ThreadNode* temp;
    struct ContainerNode* iterC;
    struct ContainerNode* iterC_prev;
    struct ThreadNode* iterT;
    struct ObjectNode* iterO;
    struct ObjectNode* tempO;
    struct task_struct* ctask = current;
//    struct ThreadNode* temp2;
    
    int i; 
    
    //lock the global mutex
    mutex_lock( &mtx );
    printk( KERN_DEBUG "mtx locked by delete" );
    
    //initialize teh itterators for the linked lists
    iterC = head;
    iterT = head->start_t->next;
    
    temp = NULL;
    
    printk( KERN_DEBUG "search for %d", ctask->pid );
    
    while( temp == NULL ){
        iterC_prev = iterC;
        iterC = iterC->next;
        printk( KERN_DEBUG "check container %d", iterC->cid );
        iterT = iterC->start_t->next;
        for( i = 0; i < iterC->t_count; i++ ){
            printk( KERN_DEBUG "checking Con %d for thread %d, found %d", iterC->cid, ctask->pid, iterT->task->pid );
            if( iterT->task->pid == ctask->pid ){
                temp = iterT;
                printk( KERN_DEBUG "set thread %d to temp", temp->task->pid);
                i = iterC->t_count;
            }
            if( temp == NULL ){
                iterT = iterT->next;
            }
        }
    }
    
    iterC->t_count -= 1;
//    printk( KERN_DEBUG "decrement thread counter for the container %d to %d", iterC->cid, iterC->t_count );
//    printk( KERN_DEBUG "check if container %d thread count is 0 (%d)", iterC->cid, iterC->t_count );
    
    if( iterC->t_count > 0 ){
        printk( KERN_DEBUG "check if end of list" );
        if (temp->next != NULL ){
            temp->next->prev = temp->prev;
        }
        temp->prev->next = temp->next;
        
//        if (temp->next == NULL){
//            temp2 = iterC->start_t->next;
//        } else {
//            temp2 = temp->next;
//        }
    }
    
    //check on the container to see if it needs to be destroyed
    //---------------------------
    /**
    //don't destroy container unless no tasks AND no objects
    if( iterC->t_count == 0 && iterC->o_count == 0 ){
        iterC_prev->next = iterC->next;
//        if( iterC->next != NULL ){
//            iterC->next->prev = iterC_prev;
//        }
        kfree( iterC->start_t );
        
        iterO = iterC->start_o;
/**        
        while( iterC->o_count > 0 ){
            iterO = iterO->next;
            kfree( iterO->memory );
            iterC->o_count -= 1;
            tempO = iterO;
            if( iterO->next != NULL ){
                iterO = iterO->next;
            }
            kfree( tempO );
        }
        
        kfree( iterC->start_o );
        kfree( iterC->start_t );
        kfree( iterC );
    }
    **/
    //--------------
    //free memory for thread
    printk( KERN_DEBUG "free thread node" );
    kfree( temp );
    
    printk( KERN_DEBUG "delete unlocks mtx" );
    mutex_unlock( &mtx );
    //find the thread in the container
    //remove the thread from the container
    //if no other threads in the container, free all objects
    //then delete the container
    return 0;
}

/**
*   Create an object in the corresponding container.  If container not
*   created, create the container.
**/
int memory_container_create(struct memory_container_cmd __user *user_cmd)
{
    struct memory_container_cmd user;
    struct ContainerNode* temp;
    struct task_struct* ctask = current;
    
    //copy info from user
    copy_from_user( &user, user_cmd, sizeof (struct memory_container_cmd) );
    
    //lock global mutex
    mutex_lock( &mtx );
    printk( KERN_DEBUG "create locked mtx" );
    
    
    if (head == NULL){
        printk( KERN_DEBUG "create head" );
        head = kmalloc( sizeof( struct ContainerNode ), GFP_KERNEL );
        head->t_count = 0;
        head->o_count = 0;
        head->next = NULL;
//        head->cid = user.cid;
    }
    
    temp = head;
    int flag = 0;
    
    printk( KERN_DEBUG "look for container %d", user.cid);
    while( temp->next != NULL && flag == 0 ){
        temp = temp->next;
        if(temp->cid == user.cid){
            printk( KERN_DEBUG "found container %d", temp->cid );
            flag = 1;
        }
    }
    
    if( flag == 0 ){
        printk( KERN_DEBUG "create container %d", user.cid );
        struct ContainerNode* newCon = kmalloc(sizeof(struct ContainerNode), GFP_KERNEL);
        printk( KERN_DEBUG "set new next pointer" );
        newCon->next = head->next;
        printk( KERN_DEBUG "set head's next pointer" );
        head->next = newCon;
        printk( KERN_DEBUG "initialize counters" );
        newCon->t_count = 0;
        newCon->o_count = 0;
        printk( KERN_DEBUG "initialize lock" );
        mutex_init( &newCon->lock );
        printk( KERN_DEBUG "allocate memory for thread node");
        struct ThreadNode* thread = kmalloc(sizeof(struct ThreadNode),GFP_KERNEL);
        newCon->start_t = thread;
        newCon->start_t->next = NULL;
        newCon->start_t->prev = NULL;
        printk( KERN_DEBUG "allocate memory for object node" );
        struct ObjectNode* object = kmalloc(sizeof(struct ObjectNode),GFP_KERNEL);
        newCon->start_o = object;
        newCon->start_o->next = NULL;
        newCon->start_o->prev = NULL;
        printk( KERN_DEBUG "set container id" );
        newCon->cid = user.cid;
        
        printk( KERN_DEBUG "set temp" );
        temp = newCon;
    }
    
    printk( KERN_DEBUG "create new node for thread" );
    struct ThreadNode* newThread = kmalloc(sizeof(struct ThreadNode), GFP_KERNEL);
    newThread->next = NULL;
    newThread->prev = NULL;
    
    printk( KERN_DEBUG "set task for thread node" );
    newThread->task = ctask;
    
    printk( KERN_DEBUG "move pointers" );
    if( temp->t_count > 0 ){
        newThread->next = temp->start_t->next;
        newThread->next->prev = newThread;
    }
    
    temp->start_t->next = newThread;
    newThread->prev = temp->start_t;
    
    temp->t_count += 1;
    
    //unlock global mutex
    mutex_unlock( &mtx );
    printk( KERN_DEBUG "mtx unlocked by create" );
    

    return 0;
}


int memory_container_free(struct memory_container_cmd __user *user_cmd)
{
    struct memory_container_cmd user;
    struct ContainerNode* temp;
    struct ContainerNode* iterC;
    struct ThreadNode* iterT;
    struct ObjectNode* iterO;
    struct ObjectNode* tempO;
    struct task_struct* ctask = current;
    
    int i;
    printk( KERN_DEBUG "copy oid from user" );
    copy_from_user( &user, user_cmd, sizeof (struct memory_container_cmd) );
    
    mutex_lock( &mtx ) ;
    printk( KERN_DEBUG "mtx locked by free" );
    
    //find container the thread is in
    iterC = head;
    
    temp = NULL;
    
    printk( KERN_DEBUG "search for the container with the task" );
    
    while( temp == NULL ){
        iterC = iterC->next;
//        printk( KERN_DEBUG "check container %d", iterC->cid );
        iterT = iterC->start_t->next;
        for( i = 0; i < iterC->t_count; i++ ){
//            printk( KERN_DEBUG "checking Con %d for thread %d, found %d", iterC->cid, ctask->pid, iterT->task->pid );
            if( iterT->task->pid == ctask->pid ){
                temp = iterC;
                printk( KERN_DEBUG "found thread in Con %d", temp->cid );
                i = iterC->t_count;
            }
            if( temp == NULL ){
                iterT = iterT->next;
            }
        }
        if( temp == NULL && iterC->next == NULL ){
            printk( KERN_DEBUG "thread was not found" );
            mutex_unlock( &mtx );
            return 0;
        }
    }
    
    //find object once container identified
//    printk( KERN_DEBUG "set tempO object container" );
    tempO = NULL;
    if( iterC->o_count == 0 ){
        printk( KERN_DEBUG "no objects, just leave" );
        mutex_unlock( &mtx );
        return 0;
    }
    
//   printk( KERN_DEBUG "initialize iterator" );
    iterO = temp->start_o;
    if( temp->start_o == NULL ){
        printk( KERN_DEBUG "the start of the object list is messed up" );
    }
    if(iterO == NULL){
        printk( KERN_DEBUG "the iterO is not initializing" );
    }
//    printk( KERN_DEBUG "checking %d objects", temp->o_count );
    for( i = 0; i < temp->o_count; i++ ){
//        printk( KERN_DEBUG "checking object %d", i );
        if( iterO->next == NULL ){
//            printk( KERN_DEBUG "the next pointer is messed up" );
        }
        iterO = iterO->next;
//        printk( KERN_DEBUG "checking container %d for object %d, found %d", temp->cid, user.oid, iterO->oid );
        if( iterO->oid == user.oid ){
            tempO = iterO;
            printk( KERN_DEBUG "set object %d to tempO", iterO->oid );
            i = iterC->o_count;
        }
    }
    
    if( tempO == NULL ){
        mutex_unlock( &mtx );
        printk( KERN_DEBUG "object not found" );
        return 0;
    }
    
//    printk( KERN_DEBUG "free object memory" );
    
    //is this using free correctly for the memory address?------------------------------------------------------------
//    kfree( &iterO->memory );
    
    
    iterC->o_count -= 1;
    printk( KERN_DEBUG "adjust object pointers");
    tempO = iterO;
    iterO->prev->next = iterO->next;
    if( iterO->next != NULL ){
        iterO->next->prev = iterO->prev;
    }
    
    printk( KERN_DEBUG "free object" );
    kfree( iterO->start );
    
    printk( KERN_DEBUG "free the node space" );
    kfree( iterO );
    
    printk( KERN_DEBUG "free unlocks mtx" );
    mutex_unlock( &mtx );
    //free object
    //remove object from list
    return 0;
}


/**
 * control function that receive the command in user space and pass arguments to
 * corresponding functions.
 */
int memory_container_ioctl(struct file *filp, unsigned int cmd,
                              unsigned long arg)
{
    switch (cmd)
    {
    case MCONTAINER_IOCTL_CREATE:
        return memory_container_create((void __user *)arg);
    case MCONTAINER_IOCTL_DELETE:
        return memory_container_delete((void __user *)arg);
    case MCONTAINER_IOCTL_LOCK:
        return memory_container_lock((void __user *)arg);
    case MCONTAINER_IOCTL_UNLOCK:
        return memory_container_unlock((void __user *)arg);
    case MCONTAINER_IOCTL_FREE:
        return memory_container_free((void __user *)arg);
    default:
        return -ENOTTY;
    }
}
