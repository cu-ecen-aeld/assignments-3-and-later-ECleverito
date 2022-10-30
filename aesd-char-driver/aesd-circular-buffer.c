/**
 * @file aesd-circular-buffer.c
 * @brief Functions and data related to a circular buffer imlementation
 *
 * @author Dan Walkes
 * @date 2020-03-01
 * @copyright Copyright (c) 2020
 *
 */

#ifdef __KERNEL__
#include <linux/string.h>
#else
#include <string.h>
#endif

#include "aesd-circular-buffer.h"


/**
 * @param buffer the buffer to search for corresponding offset.  Any necessary locking must be performed by caller.
 * @param char_offset the position to search for in the buffer list, describing the zero referenced
 *      character index if all buffer strings were concatenated end to end
 * @param entry_offset_byte_rtn is a pointer specifying a location to store the byte of the returned aesd_buffer_entry
 *      buffptr member corresponding to char_offset.  This value is only set when a matching char_offset is found
 *      in aesd_buffer.
 * @return the struct aesd_buffer_entry structure representing the position described by char_offset, or
 * NULL if this position is not available in the buffer (not enough data is written).
 */
struct aesd_buffer_entry *aesd_circular_buffer_find_entry_offset_for_fpos(struct aesd_circular_buffer *buffer,
            size_t char_offset, size_t *entry_offset_byte_rtn )
{

    uint64_t i;
    uint64_t byteCount=0;
    struct aesd_buffer_entry *entrySearcher;
    bool entryFound=false;
    size_t char_offset_res;

    if(buffer==NULL)
        return NULL;

    for(i = buffer->out_offs; i<AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED; i++)
    {
        if(i==bufferLength(buffer))
        {
            entry_offset_byte_rtn=NULL;
            return NULL;
        }

        entrySearcher=&buffer->entry[i];

        if(char_offset<=(byteCount+entrySearcher->size-1))
        {
            entryFound=true;
            byteCount+=entrySearcher->size;
            break;
        }
        else
        {
            byteCount+=entrySearcher->size;
        }
    }

    if(!entryFound)
    {
        for(i=0; i<buffer->in_offs; i++)
        {
            entrySearcher=&buffer->entry[i];

            if(char_offset<=(byteCount+entrySearcher->size-1))
            {
                entryFound=true;
                byteCount+=entrySearcher->size;
                break;
            }
            else
            {
                byteCount+=entrySearcher->size;
            }

        }
    }

    if(entryFound)
    {   
        char_offset_res = entrySearcher->size - (byteCount-char_offset);
        *entry_offset_byte_rtn = char_offset_res;
        return entrySearcher;

    }
    else
    {
        entry_offset_byte_rtn=NULL;
        return NULL;
    }
}

/**
* Adds entry @param add_entry to @param buffer in the location specified in buffer->in_offs.
* If the buffer was already full, overwrites the oldest entry and advances buffer->out_offs to the
* new start location.
* Any necessary locking must be handled by the caller
* Any memory referenced in @param add_entry must be allocated by and/or must have a lifetime managed by the caller.
*/
void aesd_circular_buffer_add_entry(struct aesd_circular_buffer *buffer, const struct aesd_buffer_entry *add_entry)
{

    buffer->entry[buffer->in_offs] = *add_entry;    

    if(buffer->full)
    {
        //If ptrs are at end of buffer array and needs to wrap-around
        if(buffer->out_offs==(AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED-1))
        {
            buffer->out_offs=0;
            buffer->in_offs=0;
        }
        else
        {
            buffer->out_offs++;
            buffer->in_offs++;
        }
    }
    else
    {

        if(buffer->in_offs==(AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED-1))
        {
            buffer->in_offs=0;
        }
        else
        {
            buffer->in_offs++;
        }

        if(buffer->in_offs==buffer->out_offs)
        {
            buffer->full=true;
        }
    }

}

/**
* Initializes the circular buffer described by @param buffer to an empty struct
*/
void aesd_circular_buffer_init(struct aesd_circular_buffer *buffer)
{
    memset(buffer,0,sizeof(struct aesd_circular_buffer));
}

uint64_t bufferLength(struct aesd_circular_buffer *buffer)
{
    if(buffer->full)
    {
        return AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
    }
    else if(buffer->in_offs > buffer->out_offs)
    {
        return buffer->in_offs - buffer->out_offs;
    }
    else if(buffer->out_offs > buffer->in_offs)
    {
        return (AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED - buffer->out_offs) + buffer->in_offs;
    }
    else
        //buffer empty
        return 0;

}

//Returns total number of bytes in buffer
void bufferSize(struct aesd_circular_buffer *buffer, uint64_t *bufferSize)
{
    struct aesd_buffer_entry *entryPtr = NULL;
    uint64_t index;
    *bufferSize = 0;

    if(buffer->full)
    {
        index = 0;
        AESD_CIRCULAR_BUFFER_FOREACH(entryPtr,buffer,index)
        {
            (*bufferSize)+=entryPtr->size;
        }
    }
    else if(buffer->in_offs > buffer->out_offs)
    {
        index = buffer->out_offs;
        for(index=buffer->out_offs;index<buffer->in_offs;index++)
        {
            entryPtr=&(buffer->entry[index]);
            (*bufferSize)+=entryPtr->size;
        }
    }
    else if(buffer->out_offs > buffer->in_offs)
    {
        index = buffer->out_offs;
        for(index=buffer->out_offs;index<AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;index++)
        {
            entryPtr=&(buffer->entry[index]);
            (*bufferSize)+=entryPtr->size;
        }

        index = 0;
        for(index=buffer->out_offs;index<AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;index++)
        {
            entryPtr=&(buffer->entry[index]);
            (*bufferSize)+=entryPtr->size;
        }
    }
    else
        //buffer empty
        *bufferSize = 0;
}