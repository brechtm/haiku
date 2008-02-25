/*
 * Copyright 2007 Oliver Ruiz Dorantes, oliver.ruiz.dorantes_at_gmail.com
 *
 * All rights reserved. Distributed under the terms of the MIT License.
 *
 */

#include "snet_buffer.h"

#include <malloc.h>
#include <string.h>

struct snet_buffer {
	struct list_link link;

    uint8*   buffer;
    
	uint16   allocatedSize;
    uint16   expectedSize;
    uint16   puttingSize;
    uint16   pullingSize;

    void*    cookie;

};


snet_buffer*
snb_create(uint16 size)
{
   /* TODO: pointer checking */

#ifdef SNB_BUFFER_ATTACHED
   /* Allocating these 2 buffers together might prevent memory fragmentation? */
   snet_buffer* snb = (snet_buffer*) malloc(sizeof(snet_buffer) + size);
   snb->buffer = ((uint8*)snb) + sizeof(snet_buffer);
#else
   snet_buffer* snb = malloc(sizeof (snet_buffer));
   snb->buffer = malloc(size);
#endif

   snb->pullingSize = snb->puttingSize = 0;
   snb->expectedSize = snb->allocatedSize = size;

   return snb;

}


void
snb_put(snet_buffer* snb, void* data, uint16 size)
{
    /* TODO: check overflow */
    memcpy( &snb->buffer[snb->puttingSize], data, size);
    snb->puttingSize+=size;
}


void*
snb_pull(snet_buffer* snb, uint16 size)
{
    /* TODO: check overflow */
    snb->pullingSize+=size;
    return &snb->buffer[snb->pullingSize-size];

}


inline void
snb_reset(snet_buffer* snb)
{
    snb->puttingSize = snb->pullingSize = 0;
}


void
snb_free(snet_buffer* snb)
{
	if (snb == NULL)
		return;

#ifdef SNB_BUFFER_ATTACHED
    free(snb);
#else
    free(snb->buffer);
    free(snb);
#endif

}


inline void*
snb_get(snet_buffer* snb)
{
    /* TODO: pointer checking */
    return snb->buffer;
}


inline uint16
snb_size(snet_buffer* snb)
{
    /* TODO: pointer checking */
    return snb->expectedSize;
}


inline void*
snb_cookie(snet_buffer* snb)
{
    /* TODO: pointer checking */
    return snb->cookie;
}


inline void
snb_set_cookie(snet_buffer* snb, void* cookie)
{
    /* TODO: pointer checking */
    snb->cookie = cookie;
}


/* Return true if we canot "put" more data in the buffer */
inline bool     snb_completed(snet_buffer* snb)
{
    return (snb->expectedSize == snb->puttingSize);
}


/* Return true if we cannot pull more more data from the buffer */
inline bool      snb_finished(snet_buffer* snb)
{
    return (snb->expectedSize == snb->pullingSize);
}


inline uint16    snb_remaining_to_put(snet_buffer* snb)
{
    return (snb->expectedSize - snb->puttingSize);
}


inline uint16    snb_remaining_to_pull(snet_buffer* snb)
{
    return (snb->expectedSize - snb->pullingSize);
}


/* ISSUE1: Number of packets in the worst case(we always need a bigger 
   buffer than before) increases, never decreases:
   
   SOL1: Delete the smallest when the queue is bigger than X elements 
   SOL2: ? 
   
   ISSUE2: If the queue is not gonna be used for long time. Memory c
   ould be freed
   
   SOL1: Provide purge func.
   SOL2: ?
   
   */
static snet_buffer*
snb_attempt_reuse(snet_buffer* snb, uint16 size)
{
    if ( snb == NULL ||
        ((int16)snb->allocatedSize - (int16)size) < 0 ) {

        /* Impossible or not worth, Creating a new one */
        snb_free(snb);
        return snb_create(size);

    }
    else {
        snb_reset(snb);
        snb->expectedSize = size;
        return snb;
    }

}


void
snb_park(struct list* l, snet_buffer* snb)
{
	snet_buffer* item = NULL;
	/* insert it by order */
	while ((item = list_get_next_item(l, item)) != NULL) {
		if (item->allocatedSize > snb->allocatedSize)
			list_insert_item_before(l, item, snb);
	}
}


snet_buffer*
snb_fetch(struct list* l, uint16 size)
{

	snet_buffer* item = NULL;
	snet_buffer* previous = NULL;
	
	if (!list_is_empty(l))
	while ((item = list_get_next_item(l, item)) != NULL) {
		if (item->allocatedSize == size) {
			/* This one is for us*/
			break;
		}
		else if (item->allocatedSize > size) {
			/* get the previous*/
			item = previous;
			break;
		}
		previous = item;
	}
	
	// reusing previous pointer for another proposit
	previous = snb_attempt_reuse(item, size); 
	
	/* the resulting reused one is the same as we fetched? => remove it from list*/
	if (item == previous) {
		list_remove_item(l, item);
	}
	
	return previous;
}
