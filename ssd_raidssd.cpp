/* Copyright 2012 Matias Bjørling */

/* FlashSim is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version. */

/* FlashSim is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details. */

/* You should have received a copy of the GNU General Public License
 * along with FlashSim.  If not, see <http://www.gnu.org/licenses/>. */

/****************************************************************************/

/* Ssd class
 * Matias Bjørling 2012-01-09
 *
 * The Raid SSD is responsible for raiding multiple SSDs together using different mapping techniques.
 */

#include <cmath>
#include <new>
#include <assert.h>
#include <stdio.h>
#include "ssd.h"
#include <sys/mman.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

using namespace ssd;

/* use caution when editing the initialization list - initialization actually
 * occurs in the order of declaration in the class definition and not in the
 * order listed here */
RaidSsd::RaidSsd(uint ssd_size):
	size(ssd_size)
{
/*
 * Idea
 *
 * Create the instances of the SSDs.
 *
 * Techniques
 *
 * 1. Striping
 * 2. Address splitting.
 * 3. Complete control
 */
	Ssds = new Ssd[RAID_NUMBER_OF_PHYSICAL_SSDS];

	return;
}

RaidSsd::~RaidSsd(void)
{
	return;
}

void RaidSsd::swap_ssd(uint id){
	//todo: swap ssd
	// Ssds[id].~Ssd();
	// (void) new (&Ssds[id]) Ssd();
}

double RaidSsd::event_arrive(enum event_type type, ulong logical_address, uint size, double start_time)
{
	return event_arrive(type, logical_address, size, start_time, NULL);
}

/* This is the function that will be called by DiskSim
 * Provide the event (request) type (see enum in ssd.h),
 * 	logical_address (page number), size of request in pages, and the start
 * 	time (arrive time) of the request
 * The SSD will process the request and return the time taken to process the
 * 	request.  Remember to use the same time units as in the config file. */
double RaidSsd::event_arrive(enum event_type type, ulong logical_address, uint size, double start_time, void *buffer)
{
	if (type == WRITE)
		printf("Writing to logical address: %lu\n", logical_address);
	else if (type == READ)
		printf("Read from logical address: %lu\n", logical_address);

	if (PARALLELISM_MODE == 1) // Striping
	{
		double timings[RAID_NUMBER_OF_PHYSICAL_SSDS];
		for (int i=0;i<RAID_NUMBER_OF_PHYSICAL_SSDS;i++)
		{
			if (buffer == NULL)
			{
				printf("Executing stage: %i\n", i);
				timings[i] = Ssds[i].event_arrive(type, logical_address, size, start_time, NULL);
			}

			else
				timings[i] = Ssds[i].event_arrive(type, logical_address, size, start_time, (char*)buffer +(i*PAGE_SIZE));

		}

		for (int i=0;i<RAID_NUMBER_OF_PHYSICAL_SSDS-1;i++)
		{
			if (timings[i] != timings[i+1])
				fprintf(stderr, "ERROR: Timings are not the same. %d %d\n", timings[i], timings[i+1]);
		}

		return timings[0];
	}
	else if (PARALLELISM_MODE == 2) // Splitted address space
	{
		return Ssds[logical_address%RAID_NUMBER_OF_PHYSICAL_SSDS].event_arrive(type, logical_address, size, start_time, (char*)buffer);
	}

	return 0;
}

/*
 * Returns a pointer to the global buffer of the Ssd.
 * It is up to the user to not read out of bound and only
 * read the intended size. i.e. the page size.
 */
void *RaidSsd::get_result_buffer()
{
	return global_buffer;
}

void RaidSsd::print_statistics()
{
	for (uint i=0;i<RAID_NUMBER_OF_PHYSICAL_SSDS;i++)
	{
		printf ("%u %s",i, " th SSD \n");
		Ssds[i].print_statistics();
	}


}

void RaidSsd::write_statistics(FILE* stream, double time)
{
	for (uint i=0;i<RAID_NUMBER_OF_PHYSICAL_SSDS;i++)
	{
		fprintf(stream, "ssd_stat,%d,%lf,=,",i,time);
		Ssds[i].write_statistics(stream);
	}
}



void RaidSsd::reset_statistics(FILE* stream){
	for (uint i=0;i<RAID_NUMBER_OF_PHYSICAL_SSDS;i++)
	{
		//fprintf ( stream,"reset ssd stat %u %s",i, " th SSD \n");
		Ssds[i].reset_statistics();
	}
}

void RaidSsd::write_header( FILE* stream ){
	fprintf( stream, "data_type,ssd_id,time,=," );
	Ssds[0].write_header(stream);
}

void RaidSsd::print_ftl_statistics()
{
	for (uint i=0;i<RAID_NUMBER_OF_PHYSICAL_SSDS;i++)
		{
			printf ("%u %s",i, " th SSD FTL \n");
			Ssds[i].print_ftl_statistics();

		}
}
