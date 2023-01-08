#include <malloc.h>
#include <string.h>
#include <math.h>

#include "allocator.h"

#define PAGE_SIZE 4096

struct __pageTableEntry
{
  struct __pageTableEntry *next;
  void *memAddr;
  size_t pageNumber;
};

struct __pageTableEntry *__pageTableEntry_construct(size_t pageNumber)
{
  struct __pageTableEntry *entry = (struct __pageTableEntry *)malloc(sizeof(struct __pageTableEntry));
  entry->next = NULL;
  entry->memAddr = NULL;
  entry->pageNumber = pageNumber;

  return entry;
}

void __pageTable_destruct(struct __pageTableEntry *head)
{
  struct __pageTableEntry *currPage = head,
                          *nextPage = NULL;

  while (currPage != NULL)
  {
    nextPage = currPage->next;
    free(currPage);
    currPage = nextPage;
  }
}

struct __pageTableEntry *__pageTable_find(struct __pageTableEntry *head, struct __pageTableEntry *next)
{
  if (head == NULL)
    return NULL;

  struct __pageTableEntry *ret = head;
  while (ret->next != next)
    ret = ret->next;
  return ret;
}

struct __pageTableEntry *__pageTableEntry_find(struct __pageTableEntry *head, size_t pageNumber)
{
  struct __pageTableEntry *currentPage = head;
  while (currentPage->pageNumber != pageNumber && currentPage != NULL)
    currentPage = currentPage->next;

  return currentPage;
}

struct __pageTableEntry *__pageTable_erase(struct __pageTableEntry *head,
                                           struct __pageTableEntry *currentPage)
{
  struct __pageTableEntry *tail = __pageTable_find(head, NULL);

  if (currentPage == tail &&
      currentPage != head)
  {
    struct __pageTableEntry *prevLast = head;
    while (prevLast->next != tail)
      prevLast = prevLast->next;

    free(tail);
    prevLast->next = NULL;
  }
  else if (currentPage == tail &&
           currentPage == head)
  {
    free(currentPage);
    return NULL;
  }
  else
  {
    struct __pageTableEntry *forDelete = currentPage->next;

    currentPage->next = forDelete->next;
    currentPage->memAddr = forDelete->memAddr;
    currentPage->pageNumber = forDelete->pageNumber;

    free(forDelete);
  }
  return head;
}

struct __frameTableEntry
{
  struct __frameTableEntry *next;
  void *memAddr;
  bool isFree;
};

struct __frameTableEntry *__frameTableEntry_construct(void *memAddr)
{
  struct __frameTableEntry *entry = (struct __frameTableEntry *)malloc(sizeof(struct __frameTableEntry));
  entry->next = NULL;
  entry->isFree = 1;
  entry->memAddr = memAddr;

  return entry;
}

struct __frameTableEntry *__frameTable_construct(char *buffer, size_t bufSize)
{
  struct __frameTableEntry *frameTableHead = __frameTableEntry_construct(buffer);

  struct __frameTableEntry *currentFrame = frameTableHead;
  for (size_t i = PAGE_SIZE; i < bufSize; i += PAGE_SIZE)
  {
    currentFrame->next = __frameTableEntry_construct(buffer + i);
    currentFrame = currentFrame->next;
  }

  return frameTableHead;
}

struct __frameTableEntry *__frameTableEntry_find(struct __frameTableEntry *head, void *memAddr)
{
  struct __frameTableEntry *currentFrame = head;
  while (currentFrame != NULL &&
         currentFrame->memAddr != memAddr)
    currentFrame = currentFrame->next;

  return currentFrame;
}

void __frameTable_destruct(struct __frameTableEntry *head)
{
  struct __frameTableEntry *currFrame = head,
                           *nextFrame = NULL;

  while (currFrame != NULL)
  {
    nextFrame = currFrame->next;
    free(currFrame);
    currFrame = nextFrame;
  }
}

void __zero_fill_allocator(struct allocator *mmanager)
{
  mmanager->buffer = NULL;
  mmanager->bufSize = 0;
  mmanager->freeFramesNum = 0;

  mmanager->pageTableHead = NULL;
  mmanager->pageTableTail = NULL;
  mmanager->frameTableHead = NULL;
}

// buffer size should be much more than PAGE_SIZE for effective memory using
int allocator_construct(struct allocator *mmanager, size_t buffSize)
{
  __zero_fill_allocator(mmanager);

  if (buffSize > 0)
  {
    mmanager->bufSize = buffSize + ((buffSize % PAGE_SIZE == 0) ? 0 : PAGE_SIZE);

    mmanager->buffer = (char *)malloc(mmanager->bufSize);

    mmanager->frameTableHead = __frameTable_construct(mmanager->buffer, mmanager->bufSize);
    mmanager->freeFramesNum = mmanager->bufSize / PAGE_SIZE;

    return 0;
  }

  return 1;
}

int allocator_destruct(struct allocator *mmanager)
{
  free(mmanager->buffer);
  __pageTable_destruct(mmanager->pageTableHead);
  __frameTable_destruct(mmanager->frameTableHead);
  __zero_fill_allocator(mmanager);

  return 0;
}

void __allocator_push_pageTableEntry(struct allocator *mmanager)
{
  if (mmanager->pageTableHead == NULL)
  {
    mmanager->pageTableHead = __pageTableEntry_construct(1);
    mmanager->pageTableTail = mmanager->pageTableHead;
  }
  else
  {
    mmanager->pageTableTail->next =
        __pageTableEntry_construct(mmanager->pageTableTail->pageNumber + 1);

    mmanager->pageTableTail = mmanager->pageTableTail->next;
  }
}

void *__allocator_find_free_frame(struct allocator *mmanager)
{
  struct __frameTableEntry *currentFrame = mmanager->frameTableHead;
  while (currentFrame != NULL)
  {
    if (currentFrame->isFree)
    {
      currentFrame->isFree = 0;
      mmanager->freeFramesNum--;
      return currentFrame->memAddr;
    }
    currentFrame = currentFrame->next;
  }
  return NULL;
}
void *allocator_allocate(struct allocator *mmanager, ssize_t msize)
{
  if (mmanager->freeFramesNum < ceil(msize / PAGE_SIZE))
    return NULL;

  void *virtAddr = NULL;

  while (msize > 0)
  {
    __allocator_push_pageTableEntry(mmanager);

    mmanager->pageTableTail->memAddr = __allocator_find_free_frame(mmanager);

    if (virtAddr == NULL)
      virtAddr = (void *)(mmanager->pageTableTail->pageNumber << 12);

    msize -= PAGE_SIZE;
  }

  return virtAddr;
}

void __allocator_freeUp_frame(struct allocator *mmanager, void *memAddr)
{
  struct __frameTableEntry *currentFrame = __frameTableEntry_find(mmanager->frameTableHead, memAddr);
  currentFrame->isFree = 1;
  mmanager->freeFramesNum++;
}

int allocator_deallocate(struct allocator *mmanager, void *virtAddr, ssize_t msize)
{
  if ((((size_t)virtAddr << 52) >> 52) != 0 || msize <= 0)
    return 1;

  size_t pageNumber = (size_t)virtAddr >> 12;
  struct __pageTableEntry *currentPage = __pageTableEntry_find(mmanager->pageTableHead, pageNumber);
  if (currentPage == NULL)
    return 1;

  while (msize > 0)
  {
    __allocator_freeUp_frame(mmanager, currentPage->memAddr);

    mmanager->pageTableHead = __pageTable_erase(mmanager->pageTableHead, currentPage);
    mmanager->pageTableTail = __pageTable_find(mmanager->pageTableHead, NULL);

    msize -= PAGE_SIZE;
  }

  return 0;
}

int allocator_read(struct allocator *mmanager, void *toRealAddr, void *fromVirtAddr,
                   ssize_t msize)
{
  size_t pageNumber = (size_t)fromVirtAddr >> 12;
  size_t offset = (((size_t)fromVirtAddr << 52) >> 52);

  struct __pageTableEntry *current = __pageTableEntry_find(mmanager->pageTableHead, pageNumber);
  if (current == NULL)
    return 1;

  if (msize > 0)
  {
    if (PAGE_SIZE - offset >= msize)
      memcpy(toRealAddr, current->memAddr + offset, msize);
    else
    {
      if (current->next == NULL)
        return 1;

      memcpy(toRealAddr, current->memAddr + offset, PAGE_SIZE - offset);
      msize -= PAGE_SIZE - offset;

      allocator_read(mmanager, toRealAddr + PAGE_SIZE - offset,
                     (void *)(current->next->pageNumber << 12), msize);
    }
  }

  return 0;
}

int allocator_write(struct allocator *mmanager, void *toVirtAddr, void *fromRealAddr,
                    ssize_t msize)
{
  size_t pageNumber = (size_t)toVirtAddr >> 12;
  size_t offset = (((size_t)toVirtAddr << 52) >> 52);

  struct __pageTableEntry *current = __pageTableEntry_find(mmanager->pageTableHead, pageNumber);
  if (current == NULL)
    return 1;

  if (msize > 0)
  {
    if (PAGE_SIZE - offset >= msize)
      memcpy(current->memAddr + offset, fromRealAddr, msize);
    else
    {
      if (current->next == NULL)
        return 1;

      memcpy(current->memAddr + offset, fromRealAddr, PAGE_SIZE - offset);
      msize -= PAGE_SIZE - offset;

      allocator_write(mmanager, (void *)(current->next->pageNumber << 12),
                      fromRealAddr + PAGE_SIZE - offset, msize);
    }
  }

  return 0;
}