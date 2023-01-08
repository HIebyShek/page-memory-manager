#ifndef ALLOCATOR_H
#define ALLOCATOR_H

#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>

typedef int64_t ssize_t;

/**
 * @brief class allocator, which implements page memory
 *
 * @details also create memory abstraction using virtual memory
 *          it allow to avoid external fragmentation. It means, that
 *          methods return pointer to virtual memory, and operations
 *          of write and read to this memory can only be produced by
 *          methods allocator_read and allocator_write
 *
 */
struct allocator
{
    size_t bufSize;         //! buffer size
    long int freeFramesNum; //! number of free memory frames
    char *buffer;
    struct __pageTableEntry *pageTableHead, *pageTableTail; //! head and tail of pages table (represented as forward list)
    struct __frameTableEntry *frameTableHead;               //! head of frames table (represented as forward list)
};

/**
 * @brief class constructor
 *
 * @param mmanager = this
 * @param buffSize memory buffer
 * @return int
 */
int allocator_construct(struct allocator *mmanager, size_t buffSize);

/**
 * @brief class desctructor
 *
 * @param mmanager = this
 * @return int
 */
int allocator_destruct(struct allocator *mmanager);

/**
 * @brief return requested memory from buffer
 *
 * @param mmanager = this
 * @param msize number of bytes to return
 * @return void* pointer to virtual memory. Return NULL if not enough memory in buffer
 */
void *allocator_allocate(struct allocator *mmanager, ssize_t msize);

/**
 * @brief return memory to buffer
 *
 * @param mmanager  = this
 * @param virtAddr pointer to virtual memory
 * @param msize number of bytes
 * @return int
 */
int allocator_deallocate(struct allocator *mmanager, void *virtAddr, ssize_t msize);

/**
 * @brief read bytes from virtual addr to real addr
 * 
 * @param mmanager = this
 * @param toRealAddr real pointer to read data
 * @param fromVirtAddr virtual pointer from where bytes are read
 * @param msize number of bytes to read
 * @return int 
 */
int allocator_read(struct allocator *mmanager,
                   void *toRealAddr,
                   void *fromVirtAddr,
                   ssize_t msize);

/**
 * @brief write bytes from real addr to virtual addr
 * 
 * @param mmanager = this
 * @param toVirtAddr virtual addr to write bytes
 * @param fromRealAddr real addr from where bytes are written
 * @param msize number of bytes to write
 * @return int 
 */
int allocator_write(struct allocator *mmanager,
                    void *toVirtAddr,
                    void *fromRealAddr,
                    ssize_t msize);

#endif