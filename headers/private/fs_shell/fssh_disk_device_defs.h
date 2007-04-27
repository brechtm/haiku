/*
 * Copyright 2003-2007, Haiku, Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _FSSH_DISK_DEVICE_DEFS_H
#define _FSSH_DISK_DEVICE_DEFS_H


#include "fssh_defs.h"


typedef int32_t fssh_partition_id;
typedef int32_t fssh_disk_system_id;
typedef int32_t fssh_disk_job_id;

// partition flags
enum {
	FSSH_B_PARTITION_IS_DEVICE				= 0x01,
	FSSH_B_PARTITION_FILE_SYSTEM			= 0x02,
	FSSH_B_PARTITION_PARTITIONING_SYSTEM	= 0x04,
	FSSH_B_PARTITION_READ_ONLY				= 0x08,
	FSSH_B_PARTITION_MOUNTED				= 0x10,	// needed?
	FSSH_B_PARTITION_BUSY					= 0x20,
	FSSH_B_PARTITION_DESCENDANT_BUSY		= 0x40,
};

// partition statuses
enum {
	FSSH_B_PARTITION_VALID,
	FSSH_B_PARTITION_CORRUPT,
	FSSH_B_PARTITION_UNRECOGNIZED,
	FSSH_B_PARTITION_UNINITIALIZED,	// Only when uninitialized manually.
									// When not recognized while scanning it's
									// B_PARTITION_UNRECOGNIZED.
};

// partition change flags
enum {
	FSSH_B_PARTITION_CHANGED_OFFSET				= 0x000001,
	FSSH_B_PARTITION_CHANGED_SIZE				= 0x000002,
	FSSH_B_PARTITION_CHANGED_CONTENT_SIZE		= 0x000004,
	FSSH_B_PARTITION_CHANGED_BLOCK_SIZE			= 0x000008,
	FSSH_B_PARTITION_CHANGED_STATUS				= 0x000010,
	FSSH_B_PARTITION_CHANGED_FLAGS				= 0x000020,
	FSSH_B_PARTITION_CHANGED_VOLUME				= 0x000040,
	FSSH_B_PARTITION_CHANGED_NAME				= 0x000080,
	FSSH_B_PARTITION_CHANGED_CONTENT_NAME		= 0x000100,
	FSSH_B_PARTITION_CHANGED_TYPE				= 0x000200,
	FSSH_B_PARTITION_CHANGED_CONTENT_TYPE		= 0x000400,
	FSSH_B_PARTITION_CHANGED_PARAMETERS			= 0x000800,
	FSSH_B_PARTITION_CHANGED_CONTENT_PARAMETERS	= 0x001000,
	FSSH_B_PARTITION_CHANGED_CHILDREN			= 0x002000,
	FSSH_B_PARTITION_CHANGED_DESCENDANTS		= 0x004000,
	FSSH_B_PARTITION_CHANGED_DEFRAGMENTATION	= 0x008000,
	FSSH_B_PARTITION_CHANGED_CHECK				= 0x010000,
	FSSH_B_PARTITION_CHANGED_REPAIR				= 0x020000,
	FSSH_B_PARTITION_CHANGED_INITIALIZATION		= 0x040000,
};

// disk device flags
enum {
	FSSH_B_DISK_DEVICE_REMOVABLE		= 0x01,
	FSSH_B_DISK_DEVICE_HAS_MEDIA		= 0x02,
	FSSH_B_DISK_DEVICE_READ_ONLY		= 0x04,
	FSSH_B_DISK_DEVICE_WRITE_ONCE	= 0x08,
};

// disk system flags
enum {
	FSSH_B_DISK_SYSTEM_IS_FILE_SYSTEM							= 0x0001,

	// flags common for both file and partitioning systems
	FSSH_B_DISK_SYSTEM_SUPPORTS_CHECKING							= 0x0002,
	FSSH_B_DISK_SYSTEM_SUPPORTS_REPAIRING							= 0x0004,
	FSSH_B_DISK_SYSTEM_SUPPORTS_RESIZING							= 0x0008,
	FSSH_B_DISK_SYSTEM_SUPPORTS_MOVING								= 0x0010,
	FSSH_B_DISK_SYSTEM_SUPPORTS_SETTING_CONTENT_NAME				= 0x0020,
	FSSH_B_DISK_SYSTEM_SUPPORTS_SETTING_CONTENT_PARAMETERS			= 0x0040,

	// file system specific flags
	FSSH_B_DISK_SYSTEM_SUPPORTS_DEFRAGMENTING						= 0x0100,
	FSSH_B_DISK_SYSTEM_SUPPORTS_DEFRAGMENTING_WHILE_MOUNTED			= 0x0200,
	FSSH_B_DISK_SYSTEM_SUPPORTS_CHECKING_WHILE_MOUNTED				= 0x0400,
	FSSH_B_DISK_SYSTEM_SUPPORTS_REPAIRING_WHILE_MOUNTED				= 0x0800,
	FSSH_B_DISK_SYSTEM_SUPPORTS_RESIZING_WHILE_MOUNTED				= 0x1000,
	FSSH_B_DISK_SYSTEM_SUPPORTS_MOVING_WHILE_MOUNTED				= 0x2000,
	FSSH_B_DISK_SYSTEM_SUPPORTS_SETTING_CONTENT_NAME_WHILE_MOUNTED	= 0x4000,
	FSSH_B_DISK_SYSTEM_SUPPORTS_SETTING_CONTENT_PARAMETERS_WHILE_MOUNTED	= 0x8000,

	// partitioning system specific flags
	FSSH_B_DISK_SYSTEM_SUPPORTS_RESIZING_CHILD						= 0x0100,
	FSSH_B_DISK_SYSTEM_SUPPORTS_MOVING_CHILD						= 0x0200,
	FSSH_B_DISK_SYSTEM_SUPPORTS_SETTING_NAME						= 0x0400,
	FSSH_B_DISK_SYSTEM_SUPPORTS_SETTING_TYPE						= 0x0800,
	FSSH_B_DISK_SYSTEM_SUPPORTS_SETTING_PARAMETERS					= 0x1000,
	FSSH_B_DISK_SYSTEM_SUPPORTS_CREATING_CHILD						= 0x2000,
	FSSH_B_DISK_SYSTEM_SUPPORTS_DELETING_CHILD						= 0x4000,
	FSSH_B_DISK_SYSTEM_SUPPORTS_INITIALIZING						= 0x8000,
};

// disk device job types
enum {
	FSSH_B_DISK_DEVICE_JOB_BAD_TYPE,
	FSSH_B_DISK_DEVICE_JOB_DEFRAGMENT,
	FSSH_B_DISK_DEVICE_JOB_REPAIR,
	FSSH_B_DISK_DEVICE_JOB_RESIZE,
	FSSH_B_DISK_DEVICE_JOB_MOVE,
	FSSH_B_DISK_DEVICE_JOB_SET_NAME,
	FSSH_B_DISK_DEVICE_JOB_SET_CONTENT_NAME,
	FSSH_B_DISK_DEVICE_JOB_SET_TYPE,
	FSSH_B_DISK_DEVICE_JOB_SET_PARMETERS,
	FSSH_B_DISK_DEVICE_JOB_SET_CONTENT_PARMETERS,
	FSSH_B_DISK_DEVICE_JOB_INITIALIZE,
	FSSH_B_DISK_DEVICE_JOB_UNINITIALIZE,
	FSSH_B_DISK_DEVICE_JOB_CREATE,
	FSSH_B_DISK_DEVICE_JOB_DELETE,
	FSSH_B_DISK_DEVICE_JOB_SCAN,
};

// disk device job statuses
enum {
	FSSH_B_DISK_DEVICE_JOB_UNINITIALIZED,
	FSSH_B_DISK_DEVICE_JOB_SCHEDULED,
	FSSH_B_DISK_DEVICE_JOB_IN_PROGRESS,
	FSSH_B_DISK_DEVICE_JOB_SUCCEEDED,
	FSSH_B_DISK_DEVICE_JOB_FAILED,
	FSSH_B_DISK_DEVICE_JOB_CANCELED,
};

// disk device job progress info
typedef struct fssh_disk_device_job_progress_info {
	uint32_t	status;
	uint32_t	interrupt_properties;
	int32_t		task_count;
	int32_t		completed_tasks;
	float		current_task_progress;
	char		current_task_description[256];
} fssh_disk_device_job_progress_info;

// disk device job interrupt properties
enum {
	FSSH_B_DISK_DEVICE_JOB_CAN_CANCEL			= 0x01,
	FSSH_B_DISK_DEVICE_JOB_STOP_ON_CANCEL		= 0x02,
	FSSH_B_DISK_DEVICE_JOB_REVERSE_ON_CANCEL		= 0x04,
	FSSH_B_DISK_DEVICE_JOB_CAN_PAUSE				= 0x08,
};

// string length constants, all of which include the NULL terminator
#define FSSH_B_DISK_DEVICE_TYPE_LENGTH FSSH_B_FILE_NAME_LENGTH
#define FSSH_B_DISK_DEVICE_NAME_LENGTH FSSH_B_FILE_NAME_LENGTH
#define FSSH_B_DISK_SYSTEM_NAME_LENGTH FSSH_B_PATH_NAME_LENGTH

// max size of parameter string buffers, including NULL terminator
#define FSSH_B_DISK_DEVICE_MAX_PARAMETER_SIZE (32 * 1024)

#endif	// _FSSH_DISK_DEVICE_DEFS_H
