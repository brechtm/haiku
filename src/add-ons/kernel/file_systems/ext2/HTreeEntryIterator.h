/*
 * Copyright 2010, Haiku Inc. All rights reserved.
 * This file may be used under the terms of the MIT License.
 *
 * Authors:
 *		Janito V. Ferreira Filho
 */
#ifndef HTREE_ENTRY_ITERATOR_H
#define HTREE_ENTRY_ITERATOR_H


#include <AutoDeleter.h>

#include "DirectoryIterator.h"


class Volume;


class HTreeEntryIterator {
public:
								HTreeEntryIterator(off_t offset,
									Inode* directory);
								~HTreeEntryIterator();

			status_t			Init();
			
			status_t			Lookup(uint32 hash, int indirections,
									DirectoryIterator**	iterator,
									bool& detachRoot);
			bool				HasCollision() { return fHasCollision; }
						
			status_t			GetNext(uint32& offset);

			uint32				BlocksNeededForNewEntry();
			status_t			InsertEntry(Transaction& transaction,
									uint32 hash, uint32 block,
									uint32 newBlocksPos, bool hasCollision);
private:
								HTreeEntryIterator(uint32 block,
									uint32 blockSize, Inode* directory,
									HTreeEntryIterator* parent,
									bool hasCollision);

private:
			Inode*				fDirectory;
			Volume*				fVolume;
			status_t			fInitStatus;

			bool				fHasCollision;
			uint16				fLimit, fCount;
			uint16				fFirstEntry;
			uint16				fCurrentEntry;

			uint32				fBlockSize;
			off_t				fBlockNum;

			HTreeEntryIterator*	fParent;
			HTreeEntryIterator*	fChild;
};

#endif	// HTREE_ENTRY_ITERATOR_H
